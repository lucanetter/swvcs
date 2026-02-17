# swvcs install script
# Adds the swvcs bin\ directory to the current user's PATH permanently.
# Run once from the project root:  .\install.ps1

$binDir = Join-Path $PSScriptRoot "bin"

if (-not (Test-Path "$binDir\swvcs.exe")) {
    Write-Host "ERROR: swvcs.exe not found in $binDir" -ForegroundColor Red
    Write-Host "Build the project first:  cmake --build build-mingw -j"
    exit 1
}

# Read the current user PATH from the registry (not just the session PATH)
$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")

if ($currentPath -split ";" | Where-Object { $_ -eq $binDir }) {
    Write-Host "Already on PATH: $binDir" -ForegroundColor Yellow
} else {
    $newPath = ($currentPath.TrimEnd(";") + ";" + $binDir)
    [Environment]::SetEnvironmentVariable("Path", $newPath, "User")
    Write-Host "Added to user PATH: $binDir" -ForegroundColor Green
}

Write-Host ""
Write-Host "Done. Open a new terminal and run:  swvcs --help"
