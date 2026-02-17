# swvcs — SolidWorks Version Control System

A lightweight, standalone CLI + GUI tool for snapshotting and reverting SolidWorks files (`.SLDPRT`, `.SLDASM`, `.SLDDRW`).
Built in C++ using the SolidWorks COM API. No Git, no plugins; just runs alongside SolidWorks.

---

## Project Structure

```
swvcs/
├── CMakeLists.txt
├── README.md
├── SETUP.md              # Full install + build instructions
├── include/
│   ├── types.h           # Shared data types (Commit, SwMeta, Result, etc.)
│   ├── sw_connection.h   # SolidWorks COM API wrapper
│   ├── repository.h      # .swvcs/ folder + SQLite database management
│   ├── commit_engine.h   # Snapshot + SHA-256 hash logic
│   ├── revert_engine.h   # Restore a previous snapshot
│   ├── utils.h           # Formatting / helpers
│   ├── main_window.h     # GUI — main window (Qt6)
│   └── commit_dialog.h   # GUI — commit message dialog (Qt6)
└── src/
    ├── main.cpp           # CLI entry point
    ├── sw_connection.cpp
    ├── repository.cpp
    ├── commit_engine.cpp
    ├── revert_engine.cpp
    ├── utils.cpp
    └── gui/
        ├── main_gui.cpp       # GUI entry point
        ├── main_window.cpp    # 3-panel main window
        └── commit_dialog.cpp  # Commit message dialog
```

---

## How It Works

All commit metadata is stored in a **SQLite database** (`.swvcs/swvcs.db`). Binary snapshots and thumbnails remain as files on disk.

```
.swvcs/
├── swvcs.db              # SQLite database — all commit metadata + HEAD
├── blobs/
│   ├── a1b2c3d4....bin   # Full copy of the .SLDPRT at that point in time
│   └── ...               # Deduplicated by SHA-256 hash
└── thumbs/
    └── a1b2c3d4....bmp   # 256x256 screenshot of the model
```

Every commit stores a **full copy** of the file (not a diff). This is intentional — SolidWorks binary files aren't diffable, and storage is cheap. Identical files produce the same hash, so unchanged files don't get re-stored.

### Metadata captured per commit

| Field | Description |
|---|---|
| `hash` | SHA-256 of the snapshot file |
| `message` | User-provided description |
| `timestamp` | ISO-8601 UTC timestamp |
| `author` | Windows username |
| `parent_hash` | Previous commit (empty for first commit) |
| `doc_path` | Full path to the `.SLDPRT` / `.SLDASM` |
| `doc_type` | `Part`, `Assembly`, or `Drawing` |
| `mass` | Mass in kg |
| `volume` | Volume in m³ |
| `surface_area` | Surface area in m² |
| `feature_count` | Number of features in the feature tree |
| `material` | Material name (parts only, e.g. `1060 Alloy`) |
| `bbox_x/y/z` | Bounding box extents in mm |
| `config_count` | Number of SolidWorks configurations |
| `blob_size_bytes` | File size of the stored snapshot |

---

## Prerequisites

- Windows 10/11 (SolidWorks is Windows-only)
- SolidWorks installed (any version from ~2019 onwards)
- MSYS2 + MinGW-w64 (compiler toolchain)
- CMake 3.20+
- Qt 6.x — MinGW 64-bit kit (for the GUI only)

See **[SETUP.md](SETUP.md)** for full installation and build instructions.

---

## Build

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"

cmake -S . -B build-mingw -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER="C:/msys64/mingw64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="C:/msys64/mingw64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/msys64/mingw64/bin/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_64;C:/msys64/mingw64"

cmake --build build-mingw -j
```

Outputs: `bin\swvcs.exe` (CLI) and `bin\swvcs-gui.exe` (GUI).

> Replace `6.x.x` with your installed Qt version (e.g. `6.10.2`).

---

## Usage

### 1. Open your SolidWorks project folder in a terminal

```bat
cd C:\Projects\BracketDesign
```

### 2. Initialise a repository (once per project)

```bat
swvcs init
```

Creates `.swvcs\` with the SQLite database. Existing repos are updated automatically if opened with a newer version of swvcs.

### 3. Start SolidWorks and open your part/assembly

### 4. Commit a snapshot

```bat
swvcs commit "Initial design - base plate"
swvcs commit "Added mounting holes"
```

### 5. View history

```bat
swvcs log
swvcs log --full
```

### 6. Revert to a previous commit

```bat
swvcs revert a1b2c3d4
```

Closes the document in SolidWorks, overwrites the file with the stored snapshot, then reopens it.

### 7. Check status

```bat
swvcs status
```

---

## Current Limitations (v0.1)

- **Single file only** — assemblies with multiple referenced parts need each part committed separately.
- **No branching** — linear history only for now.
- **No compression** — blobs are stored as raw copies. Large files (100 MB+) will use significant disk space.
- **Thumbnail** — relies on `SaveBMP`, which requires the model to be in a rendered 3D viewport. May be blank on 2D drawing sheets or complex assemblies.
- **Material** — only populated for part documents; empty for assemblies and drawings.

---

## Roadmap

- [ ] Assembly-aware commits (snapshot all referenced parts together)
- [ ] Blob compression (zstd)
- [x] Simple GUI (Qt6 + MinGW)
- [x] SQLite metadata database
- [ ] Branching
- [ ] Watch mode (auto-commit on SW save)
