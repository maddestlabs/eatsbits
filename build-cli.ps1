<#
.SYNOPSIS
    Automated build & launch script for Eatsbits Terminal DAW (TUI Workstation).

.DESCRIPTION
    Configures and compiles the Eatsbits C++20 terminal workstation (eatsbits_tui)
    featuring double-buffered ANSI rendering, sub-pixel Braille visualization, and
    ultra-low latency WASAPI audio. Automatically launches the DAW in the current terminal.

.PARAMETER Clean
    Cleans previous build artifacts before configuring.

.PARAMETER Config
    Build configuration: "Release" (default) or "Debug".

.PARAMETER NoRun
    Only compiles the application without launching it.

.PARAMETER Legacy
    Compiles and launches the interactive CLI test harness (eatsbits_cli) instead of the TUI DAW.

.PARAMETER Test
    Runs automated unit tests before launching.

.EXAMPLE
    .\build-cli.ps1
    .\build-cli.ps1 -Clean
    .\build-cli.ps1 -NoRun
    .\build-cli.ps1 -Legacy
    .\build-cli.ps1 -Test
#>

[CmdletBinding()]
param (
    [switch]$Clean,
    [ValidateSet("Release", "Debug", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Release",
    [switch]$NoRun,
    [switch]$Legacy,
    [switch]$Test
)

$ErrorActionPreference = "Stop"

# Ensure script runs from project root directory
$rootDir = $PSScriptRoot
if (-not $rootDir) {
    $rootDir = Get-Location
}
Set-Location $rootDir

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Eatsbits Terminal DAW Automated Build & Launch" -ForegroundColor Cyan
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
} else {
    $configureArgs += "-DBUILD_TESTING=OFF"
}

Write-Host "    Executing: cmake $($configureArgs -join ' ')..." -ForegroundColor Gray
& cmake @configureArgs

if ($LASTEXITCODE -ne 0) {
    Write-Error "CMake configuration failed!"
    exit 1
}

# ------------------------------------------------------------------
# STEP 3: Compile Target
# ------------------------------------------------------------------
$targetName = if ($Legacy) { "eatsbits_cli" } else { "eatsbits_tui" }
Write-Host "`n[*] STEP 3: Building $targetName ($Config mode)..." -ForegroundColor Yellow

$buildArgs = @("--build", "build", "--config", $Config, "--target", $targetName)
if ($Test) {
    $buildArgs += @("--target", "test_tui_surface")
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

# ------------------------------------------------------------------
# STEP 4: Run Tests (Optional)
# ------------------------------------------------------------------
if ($Test) {
    Write-Host "`n[*] STEP 4: Running automated TUI test suite..." -ForegroundColor Yellow
    $testExe = Join-Path (Join-Path $buildDir $Config) "test_tui_surface.exe"
    if (-not (Test-Path $testExe)) {
        $testExe = Join-Path $buildDir "test_tui_surface.exe"
    }
    if (Test-Path $testExe) {
        & $testExe
    } else {
        Write-Warning "test_tui_surface.exe not found."
    }
}

# ------------------------------------------------------------------
# STEP 5: Locate & Launch Binary
# ------------------------------------------------------------------
$outDir = Join-Path $buildDir $Config
if (-not (Test-Path $outDir)) {
    $outDir = $buildDir
}

$targetExe = Join-Path $outDir "$targetName.exe"

if (-not $NoRun) {
    if (Test-Path $targetExe) {
        $sizeKb = [math]::Round((Get-Item $targetExe).Length / 1KB, 1)
        Write-Host "`n============================================================" -ForegroundColor Cyan
        Write-Host "  Launching $targetName ($sizeKb KB)" -ForegroundColor Green
        Write-Host "  Press [SPACE] for Play/Stop, [TAB] for pane switch, [Q] to quit" -ForegroundColor Gray
        Write-Host "============================================================`n" -ForegroundColor Cyan
        Start-Sleep -Milliseconds 400
        & $targetExe
    } else {
        Write-Error "Executable not found at: $targetExe"
    }
} else {
    Write-Host "`n[*] Binary compiled: $targetExe (-NoRun specified, skipping launch)." -ForegroundColor Gray
}
