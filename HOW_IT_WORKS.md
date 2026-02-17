# How swvcs Works

A technical overview of the design, architecture, and implementation decisions behind swvcs.

---

## The Problem

SolidWorks files (`.SLDPRT`, `.SLDASM`, `.SLDDRW`) are **binary files**. Unlike source code, you cannot read them as text, compare two versions line by line, or meaningfully merge two edited copies together. This makes standard version control tools like Git largely useless for SolidWorks work — you can technically store the files in a Git repository, but you get none of the benefits: no diffs, no meaningful history, and large binary files bloat the repository over time.

In practice, most engineers handle this with manual conventions:
- Folders named `bracket_v1`, `bracket_v2`, `bracket_FINAL`, `bracket_FINAL2`
- USB drives with dated folders
- Shared network drives with no history at all

These approaches are error-prone, wasteful of disk space, and make it impossible to answer basic questions like: *"What was the mass of this part three weeks ago?"* or *"When did I add that feature?"*

swvcs is designed to solve this problem without requiring Git knowledge, a server, or any changes to how SolidWorks is used.

---

## The Approach

Since SolidWorks files cannot be diffed or merged, swvcs takes the simplest approach that actually works: **full-file snapshots**.

Every commit stores a complete copy of the file at that point in time. This sounds wasteful, but:

1. **Deduplication via SHA-256** — before copying a file, swvcs hashes its contents. If an identical file has already been committed, no copy is made. The new commit record simply points to the existing blob. This means committing an unchanged file costs essentially nothing.

2. **Metadata is separate from the file** — physical properties (mass, volume, surface area, bounding box, material, feature count) are extracted from SolidWorks at commit time and stored in a SQLite database. This makes it possible to search and compare commits without opening the files.

3. **Thumbnails** — a 256×256 preview image is captured at commit time, so you can visually identify what a part looked like at any point in history without reopening old files in SolidWorks.

---

## Architecture

```
┌──────────────────────┐     ┌──────────────────────┐
│     swvcs.exe        │     │   swvcs-gui.exe       │
│     (CLI)            │     │   (Qt6 GUI)           │
└──────────┬───────────┘     └──────────┬────────────┘
           │                            │
           └────────────┬───────────────┘
                        │  (shared backend)
           ┌────────────▼───────────────┐
           │       CommitEngine         │  ← orchestrates a commit
           │       RevertEngine         │  ← orchestrates a revert
           └────────────┬───────────────┘
                        │
          ┌─────────────┼──────────────┐
          │             │              │
    ┌─────▼──────┐ ┌────▼──────┐ ┌────▼────────┐
    │SwConnection│ │Repository │ │  Filesystem │
    │(COM API)   │ │(SQLite DB)│ │(blobs+thumbs│
    └────────────┘ └───────────┘ └─────────────┘
```

The CLI and GUI are separate executables, but they share the same backend code. Neither has any special logic — they both call `CommitEngine` and `RevertEngine`, which do all the actual work.

### Components

**SwConnection** (`sw_connection.cpp`)
Manages the link to a running SolidWorks process using Windows COM (Component Object Model). When SolidWorks is open, it registers itself in Windows' "Running Object Table". SwConnection finds it there and establishes a connection, then calls SolidWorks API methods to read document properties, trigger saves, capture screenshots, and open/close files.

**Repository** (`repository.cpp`)
Manages the `.swvcs/` folder that lives alongside your SolidWorks project files. It owns the SQLite database (`swvcs.db`) which stores all commit metadata. It also provides file paths for blobs (binary file snapshots) and thumbnails, which are stored directly on disk rather than in the database (databases are not efficient for large binary files).

**CommitEngine** (`commit_engine.cpp`)
Orchestrates the commit process. When you run `swvcs commit "message"`, this is what runs:
1. Gets the active document path from SolidWorks
2. Tells SolidWorks to save the file
3. Computes a SHA-256 hash of the file on disk
4. If no blob with that hash exists, copies the file to `.swvcs/blobs/{hash}.bin`
5. Captures a 256×256 thumbnail
6. Queries SolidWorks for physical properties (mass, volume, surface area, bounding box, material, feature count)
7. Writes a commit record to the SQLite database and updates HEAD

**RevertEngine** (`revert_engine.cpp`)
Orchestrates a revert. When you run `swvcs revert <hash>`, this:
1. Looks up the commit in the database
2. Tells SolidWorks to close the file
3. Copies the blob back over the original file on disk
4. Tells SolidWorks to reopen the file
5. Updates HEAD

---

## The COM API Connection

SolidWorks exposes its entire functionality through a COM (Component Object Model) API — the same interface used by VBA macros inside SolidWorks. Normally you would use the SolidWorks SDK (a set of `.tlb` type library files that ship with SolidWorks) to get compile-time access to these APIs.

swvcs takes a different approach: it uses **late-binding IDispatch** calls. This means instead of importing the SolidWorks type library at compile time, it calls API methods by name at runtime using `IDispatch::Invoke`. The advantages:

- The project builds on any machine, even one without SolidWorks installed
- No dependency on a specific SolidWorks version — the same binary works with SW 2019 through 2025+
- No need to ship or reference type library files

The trade-off is that there is no compile-time type checking for the SolidWorks calls — an incorrectly spelled method name fails at runtime, not at build time. This is mitigated by using `SUCCEEDED()` / `FAILED()` checks on every call and falling back gracefully (e.g., if mass properties are unavailable, they're stored as 0 rather than crashing).

---

## Storage Layout

```
your-project/
├── bracket.SLDPRT          ← your actual working file (untouched)
└── .swvcs/
    ├── swvcs.db            ← SQLite database
    ├── blobs/
    │   ├── a1b2c3d4....bin ← full copy of bracket.SLDPRT at commit a1b2c3d4
    │   └── e5f6a7b8....bin ← full copy at a later commit
    └── thumbs/
        ├── a1b2c3d4....bmp ← 256×256 preview at commit a1b2c3d4
        └── e5f6a7b8....bmp
```

### The database (`swvcs.db`)

The database has two tables:

**`commits`** — one row per snapshot. Columns:

| Column | What it stores |
|---|---|
| `hash` | SHA-256 hex string — also the filename of the blob |
| `message` | The commit message you typed |
| `timestamp` | ISO-8601 UTC time of the commit |
| `author` | Windows username of the person who committed |
| `parent_hash` | Hash of the previous commit (empty for the first commit) |
| `doc_path` | Full path of the original file |
| `doc_type` | `Part`, `Assembly`, or `Drawing` |
| `mass` | Mass in kg at time of commit |
| `volume` | Volume in m³ |
| `surface_area` | Surface area in m² |
| `feature_count` | Number of features in the feature tree |
| `material` | Material name (parts only) |
| `bbox_x/y/z` | Bounding box extents in mm |
| `config_count` | Number of SolidWorks configurations |
| `blob_size_bytes` | File size of the stored snapshot |

**`config`** — key/value store. Currently holds two keys:
- `HEAD` — the hash of the most recent commit
- `version` — schema version number (used for future migrations)

### Blobs

The blob filename is the SHA-256 hash of the file contents. This gives two properties:

1. **Integrity** — if a blob file is corrupted or modified, its hash will no longer match its filename, making corruption detectable.
2. **Deduplication** — if you commit the same file twice without changes, the hash is identical, so `CommitEngine` skips the copy. Two different commits can point to the same blob.

### Database migrations

As new fields are added to the schema (like the bounding box columns added in v3), older databases are upgraded automatically. The `InitSchema()` function runs `ALTER TABLE ... ADD COLUMN` for any column that doesn't exist yet. SQLite ignores `ALTER TABLE` calls that would create a duplicate column (they throw an exception which is silently caught). This means:

- Old databases open with the new binary without any manual intervention
- Old commits show `0` or `""` for fields that didn't exist when they were created
- No data is ever lost during an upgrade

---

## Why SQLite?

SQLite was chosen over plain JSON files (the original storage format) for several reasons:

- **Queryable** — you can run SQL queries against the database to find commits, compare values over time, or filter by property. JSON files require loading everything into memory and filtering in code.
- **Atomic writes** — SQLite transactions mean a commit either fully succeeds or fully fails. A crash mid-commit won't leave the database in a corrupt state.
- **Single file** — the entire metadata store is one file, easy to back up or move.
- **No server** — SQLite is an embedded library compiled directly into the swvcs executable. There is nothing to install or configure.
- **Inspectable** — tools like DB Browser for SQLite let you open the database and read its contents directly, without writing any code.

---

## Why Qt for the GUI?

Qt6 was chosen for the graphical interface because:

- It produces a native-looking Windows application with standard controls
- It has a mature model for background work (QTimer for polling SolidWorks) without blocking the UI
- `windeployqt` automates the bundling of all required DLLs for distribution
- The MinGW build of Qt works with the same MSYS2 toolchain used for the rest of the project — no Visual Studio required

The GUI and CLI share 100% of the backend code. The GUI target in CMakeLists.txt recompiles the same `.cpp` files (`sw_connection`, `repository`, `commit_engine`, `revert_engine`, `utils`) rather than linking against a separate library. This keeps the build simple and ensures the two frontends always behave identically.

---

## Build System

The project uses CMake with the MinGW Makefiles generator. Two key third-party dependencies are fetched automatically at configure time via CMake's `FetchContent`:

- **SQLiteCpp 3.3.2** — a C++ wrapper around SQLite3. Configured with `SQLITECPP_INTERNAL_SQLITE=ON` so SQLite3 is compiled from source into the static library. This means the final `.exe` has no runtime dependency on `sqlite3.dll`.
- **nlohmann/json v3.11.3** — a header-only JSON library. Included for potential future use and config serialisation.

Neither library needs to be installed on the build machine — CMake downloads and builds them automatically.

---

## Limitations and Future Work

**Single-file commits** — swvcs currently commits the single active document. An assembly (`.SLDASM`) references many part files; a complete assembly snapshot would need to commit all referenced parts atomically. This is the most significant limitation for real-world assembly work.

**No branching** — the history is a single linear chain (`parent_hash` forms a linked list). Branching, merging, and tagging are not implemented.

**No compression** — blobs are stored as raw file copies. SolidWorks part files are already in a proprietary binary format that doesn't compress well with standard algorithms, but zstd compression could still reduce blob sizes by 20–40%.

**No remote storage** — all data lives in the `.swvcs/` folder next to the project files. A future version could sync blobs and the database to cloud object storage (S3, Azure Blob) to enable collaboration across machines.

**Thumbnail reliability** — the thumbnail is captured using SolidWorks' `SaveBMP` method, which takes a screenshot of the current viewport. If the model is shown in a 2D drawing view rather than a 3D rendered view, the thumbnail may be blank or unhelpful.
