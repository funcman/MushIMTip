#=====================================================================
# build.ps1 - MushIMTip one-click build script (PowerShell)
#
# Usage:
#   script\build.ps1                # Release build
#   script\build.ps1 -Config Debug  # Debug build
#   script\build.ps1 -Clean         # Clean and rebuild (can combine with -Config)
#   script\build.ps1 -CleanOnly     # Clean only, do not build
#
# Outputs:
#   bin\<Config>\MushIMTip.exe
#=====================================================================
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [string]$Arch = "x64",
    [switch]$Clean,
    [switch]$CleanOnly
)

$ErrorActionPreference = "Stop"

$Root  = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root "build"

#--- Clean: build/ and the build artifacts under bin/ -------------------
function Invoke-Clean {
    Write-Host "==> Cleaning build artifacts ..."
    Remove-Item -Recurse -Force $Build -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force (Join-Path $Root "bin") -ErrorAction SilentlyContinue
}

if ($Clean -or $CleanOnly) {
    Invoke-Clean
    if ($CleanOnly) { exit 0 }
}

#--- Find cmake: prefer PATH, then the one bundled with Visual Studio ---
$cmake = $null

$cmd = Get-Command cmake.exe -ErrorAction SilentlyContinue
if ($cmd) {
    $cmake = $cmd.Source
} else {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -property installationPath
        if ($vsPath) {
            $candidate = Join-Path $vsPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
            if (Test-Path $candidate) { $cmake = $candidate }
        }
    }
    # Fall back to the default install path when vswhere is unavailable
    if (-not $cmake) {
        $candidate = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if (Test-Path $candidate) { $cmake = $candidate }
    }
}

if (-not $cmake) {
    Write-Error "cmake not found; please install CMake or Visual Studio"
}

Write-Host "==> cmake: $cmake"

#--- Configure (default generator: the newest installed Visual Studio) --
Write-Host "==> Configuring ($Config / $Arch) ..."
& $cmake -S $Root -B $Build -A $Arch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "==> Building ..."
& $cmake --build $Build --config $Config --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host ""
Write-Host "==> Build complete"
Write-Host "    Executable: $Root\bin\$Config\MushIMTip.exe"
