# swvcs — Setup & Usage Guide

Complete guide for installing prerequisites, building both executables, and using swvcs.

---

## Prerequisites

| Tool | Purpose | Download |
|---|---|---|
| Windows 10/11 | Required (SolidWorks is Windows-only) | — |
| SolidWorks | Must be running for commit/revert | — |
| MSYS2 + MinGW | C++ compiler toolchain | https://www.msys2.org |
| CMake 3.20+ | Build system | https://cmake.org/download |
| Qt 6.x (MinGW 64-bit) | GUI framework | https://www.qt.io/download-open-source |

### Installing MSYS2 + MinGW

1. Download and run the MSYS2 installer from https://www.msys2.org
2. Open the MSYS2 terminal and run:
   ```bash
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake make
   ```
3. The compilers will be at `C:\msys64\mingw64\bin\`

### Installing Qt 6 (MinGW kit)

1. Download the Qt Online Installer from https://www.qt.io/download-open-source
2. Create a free Qt account and log in during install
3. In the component selector, tick **only**:
   ```
   Qt → Qt 6.x.x → MinGW 13.x 64-bit   ✅
   ```
   Everything else (MSVC, Android, Sources, Qt Creator) can be skipped.
4. Qt installs to `C:\Qt\6.x.x\mingw_64\`

> If Qt is already installed with only the MSVC kit, run `C:\Qt\MaintenanceTool.exe`,
> choose "Add or remove components", and add the **MinGW 13.x 64-bit** kit.

---

## Building

Open PowerShell in the project root (`C:\Users\lucan\Downloads\cadhub`) and run:

### Step 1 — Configure

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"

cmake -S . -B build-mingw -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER="C:/msys64/mingw64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="C:/msys64/mingw64/bin/g++.exe" `
  -DCMAKE_MAKE_PROGRAM="C:/msys64/mingw64/bin/mingw32-make.exe" `
  -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/mingw_64;C:/msys64/mingw64"
```

Replace `6.10.2` with whatever Qt version you installed.
The second path (`C:/msys64/mingw64`) lets CMake find the MSYS2 sqlite3 library used by SQLiteCpp.

Expected output (last few lines):
```
-- Qt6 found at C:/Qt/6.10.2/mingw_64/lib/cmake/Qt6 — building swvcs-gui
-- Configuring done
-- Generating done
-- Build files have been written to: .../build-mingw
```

### Step 2 — Build

```powershell
cmake --build build-mingw -j
```

Both executables are written to `bin\`:
```
bin/
  swvcs.exe       ← CLI tool
  swvcs-gui.exe   ← GUI application
```

> The warning about `LPOLESTR` in `sw_connection.cpp` is harmless — it does not affect the build.

### Step 3 — Deploy Qt DLLs (first time only)

`swvcs-gui.exe` needs Qt's DLLs next to it to run. Run this once after building:

```powershell
cd C:\Users\lucan\Downloads\cadhub\bin
C:\Qt\6.10.2\mingw_64\bin\windeployqt.exe swvcs-gui.exe
```

After this, `swvcs-gui.exe` can be run by double-clicking with no Qt installation required on the target machine.

### Rebuilding after code changes

No need to re-run the configure step. Just:

```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build build-mingw -j
```

---

## Building the installer

The installer packages both executables, all Qt DLLs, and adds swvcs to the user's PATH automatically. It requires no administrator rights and includes an uninstaller.

### Prerequisites

Download and install **Inno Setup 6** (free): https://jrsoftware.org/isinfo.php

### Steps

**1. Build the project** (if not already done):
```powershell
$env:Path = "C:\msys64\mingw64\bin;$env:Path"
cmake --build build-mingw -j
```

**2. Deploy Qt DLLs** (if not already done):
```powershell
cd C:\Users\lucan\Downloads\cadhub\bin
C:\Qt\6.10.2\mingw_64\bin\windeployqt.exe swvcs-gui.exe
```

**3. Compile the installer:**

Open `installer.iss` in Inno Setup Compiler and press **F9** (Build > Compile).

Or from the command line:
```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer.iss
```

Output: `installer\swvcs-setup.exe`

### What the installer does

- Installs to `%LocalAppData%\swvcs\` (no admin rights needed)
- Copies `swvcs.exe`, `swvcs-gui.exe`, and all Qt DLLs
- Adds the install directory to the user's PATH so `swvcs` works in any terminal
- Creates a **Start Menu** shortcut for the GUI
- Registers a proper **uninstaller** that also cleans up PATH

---

## Using the GUI (swvcs-gui.exe)

### Launching

After completing the build and deploy steps above, run the GUI by double-clicking:

```
bin\swvcs-gui.exe
```

Or from PowerShell:

```powershell
& "C:\Users\lucan\Downloads\cadhub\bin\swvcs-gui.exe"
```

> **First launch checklist:**
> 1. Build completed (`cmake --build build-mingw -j`) ✓
> 2. Qt DLLs deployed (`windeployqt.exe swvcs-gui.exe`) ✓
> 3. At least one project folder initialised with `swvcs.exe init` (see CLI section below)

### Walkthrough

1. **Open a repo folder** — Click "Open Repo" and select the folder that contains your `.SLDPRT` / `.SLDASM` files. The folder must already have a `.swvcs\` directory (created by `swvcs.exe init`).
2. **Check connection** — The toolbar shows the active SolidWorks document. Green dot = connected, grey = SolidWorks not running.
3. **Commit** — Click `+ Commit`, type a message, press Enter or click Commit. The snapshot is saved immediately.
4. **Browse history** — The left panel lists all commits, newest first, with a thumbnail preview and metadata. Click any entry to see full details on the right.
5. **Revert** — Click a commit in the list, then click "Revert to this version". SolidWorks will close the file, restore it, and reopen it automatically.

---

## Using the CLI (swvcs.exe)

See also: `usage.txt` for a quick test sequence.

Add `bin\` to your PATH, or call the exe by full path.

### Commands

```
swvcs init [dir]          Initialise a repository (run once per project folder)
swvcs status              Show HEAD commit and active SolidWorks document
swvcs commit "message"    Snapshot the active document
swvcs log [--full]        List all commits, newest first
swvcs revert <hash>       Restore working file to a previous commit
```

### Typical workflow

```bat
:: 1. Navigate to your SolidWorks project folder
cd C:\Projects\BracketDesign

:: 2. Initialise once
swvcs.exe init

:: 3. Open your part in SolidWorks, then verify connection
swvcs.exe status

:: 4. Commit a baseline
swvcs.exe commit "baseline"

:: 5. Make changes in SolidWorks, then commit again
swvcs.exe commit "changed fillet on bracket arm"

:: 6. View history
swvcs.exe log

:: 7. Revert to an earlier commit (first 7+ chars of the hash)
swvcs.exe revert a1b2c3d4
```

### What gets stored

Each commit creates:
- `.swvcs/blobs/{hash}.bin` — full copy of the `.SLDPRT` or `.SLDASM`
- `.swvcs/thumbs/{hash}.bmp` — 256×256 preview screenshot
- A row in `.swvcs/swvcs.db` — all metadata (message, timestamp, author, mass, volume, surface area, material, bounding box, feature count, file size, etc.)

Identical files share the same blob (deduplication via SHA-256), so repeated commits of an unchanged file use no extra disk space.

---

## Repository layout

```
your-project/
├── bracket.SLDPRT
└── .swvcs/
    ├── swvcs.db          ← SQLite database (all commit metadata + HEAD)
    ├── blobs/            ← full file snapshots (.bin)
    └── thumbs/           ← 256×256 preview images (.bmp)
```

---

## Troubleshooting

| Problem | Fix |
|---|---|
| `No .swvcs repository found` | Run `swvcs.exe init` in the project folder first |
| `SolidWorks is not running` | Open SolidWorks and load your part before committing |
| `swvcs-gui.exe` crashes on launch | Run `windeployqt.exe swvcs-gui.exe` in `bin\` to deploy Qt DLLs |
| CMake can't find Qt | Add `-DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/mingw_64"` to the configure command |
| Thumbnail is blank | The SolidWorks viewport must be in a rendered 3D view (not a 2D drawing sheet) |
