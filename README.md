# swvcs — SolidWorks Version Control System

A lightweight, standalone CLI tool for snapshotting and reverting SolidWorks files (`.SLDPRT`, `.SLDASM`, `.SLDDRW`).  
Built in C++ using the SolidWorks COM API. No Git, no plugins — just runs alongside SolidWorks.

---

## Project Structure

```
swvcs/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── types.h           # Shared data types (Commit, Result, etc.)
│   ├── sw_connection.h   # SolidWorks COM API wrapper
│   ├── repository.h      # .swvcs/ folder management
│   ├── commit_engine.h   # Snapshot + hash logic
│   ├── revert_engine.h   # Restore a previous snapshot
│   └── utils.h           # Formatting / helpers
└── src/
    ├── main.cpp           # CLI entry point
    ├── sw_connection.cpp
    ├── repository.cpp
    ├── commit_engine.cpp
    ├── revert_engine.cpp
    └── utils.cpp
```

---

## How It Works

```
.swvcs/
├── HEAD                  # Points to the current commit hash
├── config.json           # Repo metadata
├── commits/
│   ├── a1b2c3d4...json   # Commit record (message, timestamp, SW metadata)
│   └── ...
├── blobs/
│   ├── a1b2c3d4...bin    # Full copy of the .SLDPRT at that point in time
│   └── ...               # (deduped by SHA-256 hash)
└── thumbs/
    └── a1b2c3d4...bmp    # 256x256 screenshot of the model
```

Every commit stores a **full copy** of the file (not a diff). This is intentional — SolidWorks binary files aren't diffable, and storage is cheap. Identical files produce the same hash, so unchanged files don't get re-stored.

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
  -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_64"

cmake --build build-mingw -j
```

Outputs: `bin\swvcs.exe` (CLI) and `bin\swvcs-gui.exe` (GUI).

> **Note:** If `nlohmann/json` fetch fails due to no internet on the build machine,
> download the single header manually to `third_party/json.hpp` and adjust CMakeLists.txt.

---

## Usage

### 1. Open your SolidWorks project folder in a terminal

```bat
cd C:\Projects\BracketDesign
```

### 2. Initialise a repository

```bat
swvcs init
```

This creates a `.swvcs\` folder alongside your SolidWorks files.

### 3. Start SolidWorks and open your part/assembly

### 4. Commit a snapshot

```bat
swvcs commit "Initial design - base plate"
```

```bat
swvcs commit "Added mounting holes"
```

### 5. View history

```bat
swvcs log
```

```
* commit a1b2c3d4
  Author:  jsmith
  Date:    2025-02-17T14:32:00Z
  File:    C:\Projects\BracketDesign\bracket.SLDPRT (Part)
  Mass:    0.4821 kg
  Volume:  0.0001 m^3

      Added mounting holes

  commit b5e6f7a8
  Author:  jsmith
  Date:    2025-02-17T13:15:00Z
  ...
```

### 6. Revert to a previous commit

```bat
swvcs revert a1b2c3d4
```

This closes the document in SolidWorks, overwrites the file, and reopens it.

### 7. Check status

```bat
swvcs status
```

---

## Current Limitations (v0.1)

- **Single file only** — assemblies with multiple referenced parts need each part committed separately. Full assembly snapshot support is planned.
- **No branching** — linear history only for now.
- **No compression** — blobs are stored as raw copies. Large files (100MB+) will use significant disk space.
- **Thumbnail** — relies on `SaveBMP`, which requires the model to be in a rendered viewport. May be blank for complex assemblies.

---

## Roadmap

- [ ] Assembly-aware commits (snapshot all referenced parts together)
- [ ] Blob compression (zstd)
- [x] Simple GUI (Qt6 + MinGW)
- [ ] Branching
- [ ] Watch mode (auto-commit on SW save)