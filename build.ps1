<#
.SYNOPSIS
    Automated build & test script for Eatsbits Modular DAW using PowerShell.

.DESCRIPTION
    Configures and compiles the Eatsbits C++20 workstation, CLI, and desktop GUI (GLFW + Filament/NanoVG).
    Supports clean rebuilds, Release/Debug configurations, test suite execution, and instant launching.

.PARAMETER Clean
    Cleans previous build artifacts before configuring.

.PARAMETER Config
    Build configuration: "Release" (default) or "Debug".

.PARAMETER Target
    Specific CMake target to build (default: "all", or "eatsbits_gui", "eatsbits_cli").

.PARAMETER Run
    Immediately launches eatsbits_gui.exe upon successful build.

.PARAMETER Cli
    Launches eatsbits_cli.exe instead of the GUI.

.PARAMETER Test
    Compiles the test suite and runs automated tests. By default, tests are omitted to maximize build speed.

.PARAMETER Open
    Opens Windows File Explorer in the directory containing the compiled executables.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Run
    .\build.ps1 -Clean -Run
    .\build.ps1 -Test
    .\build.ps1 -Target eatsbits_gui -Run
#>

[CmdletBinding()]
param (
    [switch]$Clean,
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [string]$Target = "all",
    [switch]$Run,
    [switch]$NoRun,
    [switch]$Cli,
    [switch]$Tui,
    [switch]$Test,
    [switch]$Open
)

$ErrorActionPreference = "Stop"

# Ensure script runs from project root directory
$rootDir = $PSScriptRoot
if (-not $rootDir) {
    $rootDir = Get-Location
}
Set-Location $rootDir

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Eatsbits C++ Modular DAW Automated Build Script" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# ------------------------------------------------------------------
# STEP 1: Verify Environment & Toolchain
# ------------------------------------------------------------------
Write-Host "`n[*] STEP 1: Verifying toolchain..." -ForegroundColor Yellow

if (-not (Get-Command "cmake" -ErrorAction SilentlyContinue)) {
    Write-Error "CMake not found in system PATH. Please install CMake (>= 3.20) and add it to your PATH."
    exit 1
}
$cmakeVersion = (cmake --version)[0]
Write-Host "    Found CMake: $cmakeVersion" -ForegroundColor Gray

$buildDir = Join-Path $rootDir "build"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "    Cleaning previous build directory ('$buildDir')..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

# ------------------------------------------------------------------
# STEP 2: Configure CMake Project
# ------------------------------------------------------------------
Write-Host "`n[*] STEP 2: Configuring CMake project..." -ForegroundColor Yellow

if (-not (Test-Path $buildDir)) {
    New-Item -ItemType Directory -Path $buildDir | Out-Null
}

$configureArgs = @("-B", "build", "-S", ".")
if ($Test) {
    $configureArgs += "-DBUILD_TESTING=ON"
    Write-Host "    Build Mode: Application + Test Suite (-Test active)" -ForegroundColor Gray
} else {
    $configureArgs += "-DBUILD_TESTING=OFF"
    Write-Host "    Build Mode: Applications only (Tests omitted for fast build)" -ForegroundColor Gray
}

Write-Host "    Executing: cmake $($configureArgs -join ' ')..." -ForegroundColor Gray
& cmake @configureArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
    exit 1
}

# ------------------------------------------------------------------
# STEP 3: Compile & Link Targets
# ------------------------------------------------------------------
Get-Process -Name "eatsbits_gui" -ErrorAction SilentlyContinue | Stop-Process -Force
Write-Host "`n[*] STEP 3: Building Eatsbits ($Config mode)..." -ForegroundColor Yellow

$buildArgs = @("--build", "build", "--config", $Config)
if ($Target -ne "all") {
    $buildArgs += @("--target", $Target)
    Write-Host "    Target: $Target" -ForegroundColor Gray
} else {
    if ($Test) {
        Write-Host "    Target: All targets (Core, GUI, CLI, Tests)" -ForegroundColor Gray
    } else {
        Write-Host "    Target: Application targets (eatsbits_gui, eatsbits_cli)" -ForegroundColor Gray
    }
}

Write-Host "    Executing: cmake $($buildArgs -join ' ')..." -ForegroundColor Gray
$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& cmake @buildArgs
$stopwatch.Stop()

if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed with exit code $LASTEXITCODE."
    exit 1
}

$elapsedSec = [math]::Round($stopwatch.Elapsed.TotalSeconds, 2)
Write-Host "`n[+] Build completed successfully in ${elapsedSec}s!" -ForegroundColor Green

# Locate output binaries
$outDir = Join-Path $buildDir $Config
if (-not (Test-Path $outDir)) {
    $outDir = $buildDir
}

$guiExe = Join-Path $outDir "eatsbits_gui.exe"
$cliExe = Join-Path $outDir "eatsbits_cli.exe"
$tuiExe = Join-Path $outDir "eatsbits_tui.exe"

# ------------------------------------------------------------------
# Native Desktop Binary Footprint & Bloat Analysis
# ------------------------------------------------------------------
Write-Host "`n========================================================================================" -ForegroundColor Cyan
Write-Host "  Eatsbits Native Binary Footprint Analysis ($Config Mode)" -ForegroundColor Cyan
Write-Host "========================================================================================" -ForegroundColor Cyan

$nativeBinaries = @()
if (Test-Path $guiExe) {
    $nativeBinaries += [PSCustomObject]@{ Target = "eatsbits_gui.exe"; Type = "GUI DAW Application"; Path = $guiExe }
}
if (Test-Path $cliExe) {
    $nativeBinaries += [PSCustomObject]@{ Target = "eatsbits_cli.exe"; Type = "CLI Test Harness"; Path = $cliExe }
}
if (Test-Path $tuiExe) {
    $nativeBinaries += [PSCustomObject]@{ Target = "eatsbits_tui.exe"; Type = "Terminal DAW Workstation"; Path = $tuiExe }
}
$testExes = Get-ChildItem -Path $outDir -Filter "test_*.exe" -ErrorAction SilentlyContinue
foreach ($t in $testExes) {
    $nativeBinaries += [PSCustomObject]@{ Target = $t.Name; Type = "Automated Unit Test"; Path = $t.FullName }
}

$totalBytes = 0
$rows = @()
foreach ($bin in $nativeBinaries) {
    $len = (Get-Item $bin.Path).Length
    $totalBytes += $len
    $rows += [PSCustomObject]@{
        Target = $bin.Target
        Type   = $bin.Type
        Bytes  = $len
    }
}

Write-Host ("  {0,-26} {1,-24} {2,14} {3,12} {4,10}" -f "Binary Target", "Category", "Size (Bytes)", "Size (MB)", "Share (%)") -ForegroundColor Yellow
Write-Host "  --------------------------------------------------------------------------------------" -ForegroundColor Gray

foreach ($r in $rows) {
    $sizeMb = [math]::Round($r.Bytes / 1MB, 2)
    $share = if ($totalBytes -gt 0) { [math]::Round(($r.Bytes / $totalBytes) * 100.0, 1) } else { 0.0 }
    Write-Host ("  {0,-26} {1,-24} {2,14:N0} {3,11:N2} MB {4,9:N1}%" -f $r.Target, $r.Type, $r.Bytes, $sizeMb, $share) -ForegroundColor White
}

Write-Host "  --------------------------------------------------------------------------------------" -ForegroundColor Gray
$totMb = [math]::Round($totalBytes / 1MB, 2)
Write-Host ("  {0,-26} {1,-24} {2,14:N0} {3,11:N2} MB {4,9:N1}%" -f "TOTAL NATIVE BINARIES", "All Executables", $totalBytes, $totMb, 100.0) -ForegroundColor Green
Write-Host "========================================================================================`n" -ForegroundColor Cyan

# ------------------------------------------------------------------
# STEP 4: Automated Testing (Optional)
# ------------------------------------------------------------------
if ($Test) {
    Write-Host "`n[*] STEP 4: Running automated test suite..." -ForegroundColor Yellow
    $testExes = Get-ChildItem -Path $outDir -Filter "test_*.exe"
    $passed = 0
    $failed = 0

    foreach ($t in $testExes) {
        Write-Host "    Running $($t.Name)..." -ForegroundColor Gray
        & $t.FullName
        if ($LASTEXITCODE -eq 0) {
            $passed++
        } else {
            $failed++
            Write-Host "    FAILED: $($t.Name)" -ForegroundColor Red
        }
    }

    Write-Host "`n[+] Test Summary: $passed passed, $failed failed." -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
    if ($failed -gt 0) {
        exit 1
    }
}

# ------------------------------------------------------------------
# STEP 5: Open Output Directory (Optional)
# ------------------------------------------------------------------
if ($Open) {
    Write-Host "`n[*] Opening output folder: $outDir..." -ForegroundColor Gray
    explorer.exe $outDir
}

# ------------------------------------------------------------------
# STEP 6: Run Executable (Default behavior unless -NoRun)
# ------------------------------------------------------------------
if (-not $NoRun) {
    if ($Tui) {
        if (Test-Path $tuiExe) {
            Write-Host "`n[*] Launching Eatsbits Terminal DAW..." -ForegroundColor Cyan
            & $tuiExe
        } else {
            Write-Error "TUI executable not found at: $tuiExe"
        }
    } elseif ($Cli) {
        if (Test-Path $cliExe) {
            Write-Host "`n[*] Launching Eatsbits CLI..." -ForegroundColor Cyan
            & $cliExe
        } else {
            Write-Error "CLI executable not found at: $cliExe"
        }
    } else {
        if (Test-Path $guiExe) {
            Write-Host "`n[*] Launching Eatsbits Desktop GUI..." -ForegroundColor Cyan
            Start-Process -FilePath $guiExe
        } else {
            Write-Error "GUI executable not found at: $guiExe"
        }
    }
} else {
    Write-Host "`n[*] Compilation finished. Launch skipped (-NoRun specified)." -ForegroundColor Gray
}
