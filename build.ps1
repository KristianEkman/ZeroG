# Set error action to stop
$ErrorActionPreference = "Stop"

# 1. Find cmake.exe
$cmakePath = "cmake"
$cmakeInPath = $false

try {
    $null = Get-Command cmake -ErrorAction Stop
    $cmakeInPath = $true
    Write-Host "Found cmake in PATH."
} catch {
    Write-Host "cmake not found in PATH, searching in Visual Studio installations..."
    
    $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath
        if ($vsPath) {
            $vsCmake = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $vsCmake) {
                # Use absolute path and escape it correctly for invocation
                $cmakePath = $vsCmake
                Write-Host "Found VS cmake at: $vsCmake"
            }
        }
    }
}

# Double check if we successfully resolved cmake
if ($cmakePath -eq "cmake" -and -not $cmakeInPath) {
    Write-Error "Could not find cmake.exe. Please install CMake or Visual Studio with C++ tools."
    exit 1
}

# Ensure builds directory exists in workspace root so unit tests can write log files
if (-not (Test-Path "builds")) {
    New-Item -ItemType Directory -Name "builds" | Out-Null
}

# 2. Configure project
Write-Host "Configuring CMake..."
& $cmakePath -S . -B build_win

# 3. Build project
Write-Host "Building project in Release mode..."
& $cmakePath --build build_win --config Release

Write-Host "Build complete! Output binaries are in build_win/builds/Release/"
