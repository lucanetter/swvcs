# swvcs packaging script
# Builds the project, deploys Qt DLLs, and builds the installer.
# Run from the project root: .\package.ps1
#
# Output:
#   bin\                       - self-contained folder (run swvcs-gui.exe directly)
#   installer\swvcs-setup.exe  - single-file installer (if Inno Setup is installed)

param(
    [string]$QtPath    = "C:\Qt\6.10.2\mingw_64",
    [string]$MinGWPath = "C:\msys64\mingw64\bin",
    [string]$InnoSetup = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

# --- 1. Toolchain ---
Write-Host ""
Write-Host "[1/4] Setting up toolchain..." -ForegroundColor Cyan
$env:Path = "$MinGWPath;$env:Path"

# --- 2. Build ---
Write-Host "[2/4] Building..." -ForegroundColor Cyan
cmake --build "$root\build-mingw" -j
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed. Configure first:" -ForegroundColor Red
    Write-Host '  cmake -S . -B build-mingw -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ...'
    exit 1
}
Write-Host "Build OK" -ForegroundColor Green

# --- 3. Deploy Qt DLLs ---
Write-Host "[3/4] Deploying Qt DLLs..." -ForegroundColor Cyan
$windeployqt = "$QtPath\bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    Write-Host "windeployqt not found at: $windeployqt" -ForegroundColor Red
    Write-Host "Set -QtPath to your Qt mingw_64 directory"
    exit 1
}
& $windeployqt "$root\bin\swvcs-gui.exe" --release
if ($LASTEXITCODE -ne 0) {
    Write-Host "windeployqt failed" -ForegroundColor Red
    exit 1
}
Write-Host "Qt DLLs deployed" -ForegroundColor Green

# --- 4. Build installer ---
Write-Host "[4/4] Building installer..." -ForegroundColor Cyan
if (Test-Path $InnoSetup) {
    New-Item -ItemType Directory -Path "$root\installer" -Force | Out-Null
    & $InnoSetup "$root\installer.iss"
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Installer ready: installer\swvcs-setup.exe" -ForegroundColor Green
    }
    else {
        Write-Host "Installer build failed (bin\ is still usable)" -ForegroundColor Yellow
    }
}
else {
    Write-Host "Inno Setup not found - skipping installer." -ForegroundColor Yellow
    Write-Host "Install from https://jrsoftware.org/isinfo.php to enable this step."
}

# --- Done ---
Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "  Standalone : $root\bin\swvcs-gui.exe"
if (Test-Path "$root\installer\swvcs-setup.exe") {
    Write-Host "  Installer  : $root\installer\swvcs-setup.exe"
}
Write-Host ""
Write-Host "To share: send installer\swvcs-setup.exe - recipient just double-clicks it."
