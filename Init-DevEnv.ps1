<#
.SYNOPSIS
    Sets up a build-ready PowerShell environment for MSVC builds on Windows.

.DESCRIPTION
    Dot-source this script from the repository root to get a shell in which
    the MSVC presets (debug-msvc, release-msvc, ...) configure, build, and
    test out of the box:

        . .\Init-DevEnv.ps1
        cmake --preset debug-msvc
        cmake --build --preset debug-msvc
        ctest --test-dir build-debug-msvc

    It performs three steps:

    1. Imports the Visual Studio developer environment (cl.exe, Windows SDK,
       ninja) for the latest installed VS instance, via the official
       Launch-VsDevShell.ps1 resolved through vswhere.
    2. Guards the CMake PATH order: VsDevCmd appends Visual Studio's bundled
       CMake, which may be older than the floor required by our presets. If
       the first cmake on PATH is too old, a compatible one found elsewhere
       on PATH is re-prepended.
    3. Resolves a Qt for MSVC (see below), prepends its bin directory to
       PATH (so test executables find the Qt DLLs), and exports
       CMAKE_PREFIX_PATH (so plain presets work without a
       CMakeUserPresets.json).

    Qt resolution order:
      a. -QtDir parameter (path to the Qt prefix, e.g. .../6.8.5/msvc2022_64)
      b. A Qt already resolvable from the environment (Qt6_DIR or a
         CMAKE_PREFIX_PATH entry) is respected untouched — superbuild shells
         that consume this repo keep their own Qt selection.
      c. qt.user.json at the repo root (git-ignored; see qt.user.example.json)
      d. Auto-discovery under D:\dev\sdk\qt and C:\Qt (highest version wins)

.NOTES
    Requires Visual Studio 2022+ with the "Desktop development with C++"
    workload and a Qt 6.8+ MSVC kit. Re-running in an initialized shell is a
    no-op for the VS import.
#>
[CmdletBinding()]
param(
    # Qt prefix to use, e.g. D:\dev\sdk\qt\qt-6.8.5\6.8.5\msvc2022_64
    [string]$QtDir,

    [ValidateSet('amd64', 'x86', 'arm64')]
    [string]$Arch = 'amd64'
)

$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot

Write-Host '==================================================='
Write-Host '  qt-json-query: MSVC development environment setup'
Write-Host '==================================================='

# --- 1. Visual Studio developer environment -------------------------------

if ($env:VSCMD_VER) {
    Write-Host "Visual Studio dev environment already active (VsDevCmd $env:VSCMD_VER); skipping import"
} else {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found ($vswhere). Is Visual Studio installed?"
    }

    $vsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $vsPath) {
        throw 'No Visual Studio instance with the C++ workload found (vswhere returned nothing).'
    }

    Write-Host "Importing VS developer environment from [$vsPath] ($Arch)"
    # Launch-VsDevShell.ps1 shells out to vswhere.exe by bare name; make sure
    # the installer directory is on PATH so that lookup succeeds quietly.
    $installerDir = Split-Path $vswhere
    if ($env:PATH.Split(';') -notcontains $installerDir) {
        $env:PATH = $env:PATH + ';' + $installerDir
    }
    & (Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1') -Arch $Arch -SkipAutomaticLocation | Out-Null

    if (-not (Get-Command cl -ErrorAction SilentlyContinue)) {
        throw 'cl.exe not on PATH after VS environment import; check the Visual Studio installation.'
    }
}

# --- 2. CMake PATH-order guard ---------------------------------------------

# CMake floor: presets schema version 9 requires CMake >= 3.30. VsDevCmd
# APPENDS VS's bundled CMake to PATH; if a stale shell or odd PATH order puts
# an older cmake first, configure fails. Re-prepend the first compatible one.
$cmakeFloor = [version]'3.30.0'

function Get-CMakeVersion([string]$exe) {
    try {
        $line = (& $exe --version 2>$null | Select-Object -First 1)
        if ($line -match '(\d+\.\d+\.\d+)') { return [version]$Matches[1] }
    } catch {}
    return $null
}

$firstCMake = Get-Command cmake -ErrorAction SilentlyContinue
$firstVersion = if ($firstCMake) { Get-CMakeVersion $firstCMake.Source } else { $null }

if (-not $firstVersion -or $firstVersion -lt $cmakeFloor) {
    $compatible = Get-Command cmake -All -ErrorAction SilentlyContinue |
        Where-Object { (Get-CMakeVersion $_.Source) -ge $cmakeFloor } |
        Select-Object -First 1
    if ($compatible) {
        $cmakeBinDir = Split-Path $compatible.Source
        Write-Host "Prefixing compatible CMake path to env [$cmakeBinDir]"
        $env:PATH = $cmakeBinDir + ';' + $env:PATH
    } else {
        Write-Warning "No CMake >= $cmakeFloor found on PATH; presets will fail to configure."
    }
} else {
    Write-Host "CMake on PATH meets the required floor ($firstVersion >= $cmakeFloor)"
}

# --- 3. Qt resolution -------------------------------------------------------

function Test-QtPrefix([string]$prefix) {
    return $prefix -and (Test-Path (Join-Path $prefix 'lib\cmake\Qt6\Qt6Config.cmake'))
}

$qtPrefix = $null
$qtFromEnvironment = $false

if ($QtDir) {
    if (-not (Test-QtPrefix $QtDir)) {
        throw "-QtDir [$QtDir] does not look like a Qt prefix (no lib\cmake\Qt6\Qt6Config.cmake)."
    }
    $qtPrefix = $QtDir
    Write-Host "Using Qt prefix from -QtDir [$qtPrefix]"
}

# Superbuild / preconfigured-shell support: if the environment already
# carries a resolvable Qt (Qt6_DIR or a CMAKE_PREFIX_PATH entry), respect it
# instead of overriding — e.g. when this repo is a submodule of a superbuild
# whose own init script has set up the environment. -QtDir still wins as the
# explicit override.
if (-not $qtPrefix -and $env:Qt6_DIR) {
    # Qt6_DIR points at <prefix>\lib\cmake\Qt6
    $candidate = (Resolve-Path (Join-Path $env:Qt6_DIR '..\..\..') -ErrorAction SilentlyContinue)?.Path
    if (Test-QtPrefix $candidate) {
        $qtPrefix = $candidate
        $qtFromEnvironment = $true
        Write-Host "Respecting Qt from existing Qt6_DIR [$env:Qt6_DIR]"
    }
}

if (-not $qtPrefix -and $env:CMAKE_PREFIX_PATH) {
    $qtPrefix = $env:CMAKE_PREFIX_PATH.Split(';') | Where-Object { Test-QtPrefix $_ } | Select-Object -First 1
    if ($qtPrefix) {
        $qtFromEnvironment = $true
        Write-Host "Respecting Qt from existing CMAKE_PREFIX_PATH [$qtPrefix]"
    }
}

if (-not $qtPrefix) {
    $userConfig = Join-Path $repoRoot 'qt.user.json'
    if (Test-Path $userConfig) {
        $config = Get-Content -Raw $userConfig | ConvertFrom-Json
        $qtPrefix = $config.qtPrefixes | Where-Object { Test-QtPrefix $_ } | Select-Object -First 1
        if ($qtPrefix) {
            Write-Host "Using Qt prefix from qt.user.json [$qtPrefix]"
        } else {
            Write-Warning 'qt.user.json found, but none of its qtPrefixes exist; falling back to auto-discovery.'
        }
    }
}

if (-not $qtPrefix) {
    # Auto-discovery: <root>\<qt-x.y.z>\<x.y.z>\msvc*_64 and <root>\<x.y.z>\msvc*_64
    $discovered = foreach ($root in 'D:\dev\sdk\qt', 'C:\Qt') {
        if (Test-Path $root) {
            Get-ChildItem -Path "$root\*\msvc*_64", "$root\*\*\msvc*_64" -Directory -ErrorAction SilentlyContinue |
                Where-Object { Test-QtPrefix $_.FullName }
        }
    }
    $qtPrefix = $discovered |
        Sort-Object { if ($_.Parent.Name -match '(\d+\.\d+\.\d+)') { [version]$Matches[1] } else { [version]'0.0.0' } } -Descending |
        Select-Object -ExpandProperty FullName -First 1
    if ($qtPrefix) {
        Write-Host "Auto-discovered Qt prefix [$qtPrefix]"
    }
}

if (-not $qtPrefix) {
    throw 'No Qt MSVC kit found. Pass -QtDir, or create qt.user.json (see qt.user.example.json).'
}

$qtPrefix = (Resolve-Path $qtPrefix).Path  # normalize separators for PATH comparison
$qtBinDir = Join-Path $qtPrefix 'bin'
if ($env:PATH.Split(';') -notcontains $qtBinDir) {
    Write-Host "Prefixing Qt binary path to env [$qtBinDir]"
    $env:PATH = $qtBinDir + ';' + $env:PATH
} else {
    Write-Host "Qt binary path already in env [$qtBinDir]"
}

# Export for CMake: plain presets (debug-msvc, ...) pick this up without any
# CMakeUserPresets.json. APPEND so prefixes provided by an outer environment
# (e.g. a superbuild's init script) keep priority; skip entirely when Qt came
# from the environment — it is already resolvable there.
if (-not $qtFromEnvironment) {
    $env:CMAKE_PREFIX_PATH = if ($env:CMAKE_PREFIX_PATH) { "$env:CMAKE_PREFIX_PATH;$qtPrefix" } else { $qtPrefix }
}

# --- Verification footer ----------------------------------------------------

Write-Host ''
Write-Host '--- Environment summary ---'
$clVersionLine = (& cl 2>&1 | Select-String 'Version' | Select-Object -First 1)
Write-Host "cl     : $clVersionLine"
$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
Write-Host "cmake  : $(if ($cmakeCmd) { "$(Get-CMakeVersion $cmakeCmd.Source) [$($cmakeCmd.Source)]" } else { 'NOT FOUND' })"
$ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
Write-Host "ninja  : $(if ($ninjaCmd) { "$(& ninja --version) [$($ninjaCmd.Source)]" } else { 'NOT FOUND — install with: winget install Ninja-build.Ninja' })"
$qmake = Join-Path $qtBinDir 'qmake.exe'
Write-Host "Qt     : $(if (Test-Path $qmake) { & $qmake -query QT_VERSION } else { '(qmake not found)' }) [$qtPrefix]"
Write-Host "CMAKE_PREFIX_PATH = $env:CMAKE_PREFIX_PATH"
Write-Host ''
Write-Host 'Ready. Example:'
Write-Host '  cmake --preset debug-msvc && cmake --build --preset debug-msvc'
Write-Host '  ctest --test-dir build-debug-msvc'
