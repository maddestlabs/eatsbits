# tools/fetch_filament.ps1
param(
    [string]$Version = "v1.77.0"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = (Resolve-Path "$scriptDir\..").Path
$filamentDir = "$rootDir\third_party\filament"
$tarFile = "$rootDir\third_party\filament-$Version-windows.tgz"
$downloadUrl = "https://github.com/google/filament/releases/download/$Version/filament-$Version-windows.tgz"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host " Fetching Google Filament Windows SDK ($Version)" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

if (Test-Path "$filamentDir\include\filament\Engine.h") {
    Write-Host "[+] Filament SDK is already installed in: $filamentDir" -ForegroundColor Green
    exit 0
}

if (-not (Test-Path "$rootDir\third_party")) {
    New-Item -ItemType Directory -Path "$rootDir\third_party" | Out-Null
}

if (-not (Test-Path $tarFile)) {
    Write-Host "[*] Downloading $downloadUrl to $tarFile..." -ForegroundColor Yellow
    & curl.exe -L --progress-bar $downloadUrl -o $tarFile
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to download Filament release archive."
        exit 1
    }
}

Write-Host "[*] Extracting $tarFile into $filamentDir..." -ForegroundColor Yellow
if (-not (Test-Path $filamentDir)) {
    New-Item -ItemType Directory -Path $filamentDir | Out-Null
}

& tar.exe -xzf $tarFile -C $filamentDir
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to extract Filament release archive."
    exit 1
}

Write-Host "[+] Cleaning up archive..." -ForegroundColor Yellow
Remove-Item -Force $tarFile -ErrorAction SilentlyContinue

Write-Host "[+] Google Filament SDK installed successfully in $filamentDir" -ForegroundColor Green
