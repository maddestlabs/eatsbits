<#
.SYNOPSIS
    Automated WebAssembly & WebGPU build & test script for Eatsbits Modular DAW.

.DESCRIPTION
    Configures and compiles the Eatsbits C++20 workstation into WebAssembly + WebGPU (WGSL) using Emscripten.
    Optionally serves the built application locally with required COOP/COEP HTTP headers for multi-threading.

.PARAMETER Clean
    Cleans previous web build artifacts before configuring.

.PARAMETER NoServe
    Compiles only, skips starting the local dev server and browser launch.

.PARAMETER Serve
    Explicitly starts the local server (kept for backwards compatibility).

.PARAMETER Port
    Port for the local server (default: 8080).

.PARAMETER BootstrapEmsdk
    Automatically clones and activates the official Emscripten SDK if not found in PATH.

.EXAMPLE
    .\build-web.ps1
    .\build-web.ps1 -NoServe
    .\build-web.ps1 -Port 9000
    .\build-web.ps1 -BootstrapEmsdk
#>

[CmdletBinding()]
param (
    [switch]$Clean,
    [switch]$NoServe,
    [switch]$Serve,
    [int]$Port = 8080,
    [switch]$BootstrapEmsdk
)

$ErrorActionPreference = "Stop"

$rootDir = $PSScriptRoot
if (-not $rootDir) {
    $rootDir = Get-Location
}
Set-Location $rootDir

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Eatsbits WebAssembly & WebGPU Automated Build Pipeline" -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# ------------------------------------------------------------------
# Function: Ensure existing web servers on $Port are stopped
# ------------------------------------------------------------------
function Stop-ExistingWebServer {
    param([int]$TargetPort = 8080)
    Write-Host "`n[*] Ensuring existing web servers on port $TargetPort are closed..." -ForegroundColor Yellow
    $found = $false

    # 1. Terminate any process bound to the target TCP port
    try {
        $connections = Get-NetTCPConnection -LocalPort $TargetPort -ErrorAction SilentlyContinue
        if ($connections) {
            $pidsToKill = $connections | Select-Object -ExpandProperty OwningProcess -Unique | Where-Object { $_ -gt 0 -and $_ -ne $PID }
            foreach ($p in $pidsToKill) {
                $pName = (Get-Process -Id $p -ErrorAction SilentlyContinue).ProcessName
                Write-Host "    [!] Terminating existing process on port $TargetPort (PID: $p, Name: $pName)..." -ForegroundColor Yellow
                Stop-Process -Id $p -Force -ErrorAction SilentlyContinue
                $found = $true
            }
        }
    } catch {}

    # 2. Terminate any orphaned Python process running serve.py for eatsbits
    try {
        $serveProcesses = Get-CimInstance Win32_Process -Filter "CommandLine LIKE '%serve.py%'" -ErrorAction SilentlyContinue
        if ($serveProcesses) {
            foreach ($proc in $serveProcesses) {
                if ($proc.ProcessId -ne $PID) {
                    Write-Host "    [!] Terminating orphaned serve.py server (PID: $($proc.ProcessId))..." -ForegroundColor Yellow
                    Stop-Process -Id $proc.ProcessId -Force -ErrorAction SilentlyContinue
                    $found = $true
                }
            }
        }
    } catch {}

    if (-not $found) {
        Write-Host "    [+] Port $TargetPort is clear. No conflicting servers running." -ForegroundColor Green
    } else {
        Start-Sleep -Milliseconds 300
    }
}

# Close any running web servers upfront
Stop-ExistingWebServer -TargetPort $Port

# ------------------------------------------------------------------
# STEP 1: Verify Emscripten Toolchain
# ------------------------------------------------------------------
Write-Host "`n[*] STEP 1: Verifying Emscripten (emcc) toolchain..." -ForegroundColor Yellow

$emsdkCandidatePaths = @(
    "C:\emsdk",
    "$env:USERPROFILE\emsdk",
    (Join-Path $rootDir "emsdk")
)

# Check if emcc is in PATH
if (-not (Get-Command "emcc" -ErrorAction SilentlyContinue)) {
    $foundEmsdk = $null
    foreach ($cand in $emsdkCandidatePaths) {
        if (Test-Path $cand) {
            $foundEmsdk = $cand
            break
        }
    }

    if ($foundEmsdk) {
        Write-Host "    Found Emscripten SDK at: $foundEmsdk" -ForegroundColor Green
        Write-Host "    Activating emsdk environment variables..." -ForegroundColor Gray
        $batScript = Join-Path $foundEmsdk "emsdk_env.bat"
        if (Test-Path $batScript) {
            cmd.exe /c "call `"$batScript`" > nul && set" | ForEach-Object {
                if ($_ -match '^(.*?)=(.*)$') {
                    [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
                }
            }
        }
    } elseif ($BootstrapEmsdk) {
        $emsdkDir = Join-Path $rootDir "emsdk"
        if (-not (Test-Path $emsdkDir)) {
            Write-Host "    Cloning Emscripten SDK into '$emsdkDir'..." -ForegroundColor Cyan
            git clone https://github.com/emscripten-core/emsdk.git $emsdkDir
        }
        Set-Location $emsdkDir
        .\emsdk.bat install latest
        .\emsdk.bat activate latest
        . .\emsdk_env.ps1
        Set-Location $rootDir
    } else {
        Write-Host "    [!] Emscripten compiler (emcc) was not found in PATH." -ForegroundColor Red
        Write-Host "    To install or activate Emscripten:" -ForegroundColor Yellow
        Write-Host "      1. Run: .\build-web.ps1 -BootstrapEmsdk" -ForegroundColor White
        Write-Host "      OR download from: https://github.com/emscripten-core/emsdk" -ForegroundColor White
        Write-Host "      OR if already installed, activate via: path\to\emsdk_env.ps1" -ForegroundColor White
        Write-Host "`n    Continuing checks..." -ForegroundColor Gray
    }
}

if (Get-Command "emcc" -ErrorAction SilentlyContinue) {
    $emccVer = (emcc --version)[0]
    Write-Host "    Found Emscripten: $emccVer" -ForegroundColor Green
}

# ------------------------------------------------------------------
# STEP 2: Configure & Build Web Target
# ------------------------------------------------------------------
$buildDir = Join-Path $rootDir "build-web"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "`n[*] Cleaning previous web build directory ('$buildDir')..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

if (Get-Command "emcmake" -ErrorAction SilentlyContinue) {
    Write-Host "`n[*] STEP 2: Configuring CMake with Emscripten..." -ForegroundColor Yellow
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir | Out-Null
    }

    try {
        $ninjaDir = python -c "import ninja; print(ninja.BIN_DIR)" 2>$null
        if ($ninjaDir -and (Test-Path $ninjaDir)) {
            $env:PATH = "$ninjaDir;$env:PATH"
        }
        $userBase = python -m site --user-base 2>$null
        if ($userBase) {
            $userScripts = Join-Path $userBase "Scripts"
            if (Test-Path $userScripts) {
                $env:PATH = "$userScripts;$env:PATH"
            }
        }
    } catch {}

    emcmake cmake -B build-web -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed."
        exit $LASTEXITCODE
    }

    Write-Host "`n[*] STEP 3: Compiling eatsbits_web (WebGPU + WASM + AudioWorklet)..." -ForegroundColor Yellow
    cmake --build build-web --target eatsbits_web --config Release
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation failed."
        exit $LASTEXITCODE
    }

    Write-Host "`n[+] Build successful!" -ForegroundColor Green

    # ------------------------------------------------------------------
    # Payload & Binary Size Analysis (Raw vs Gzip Wire Transfer)
    # ------------------------------------------------------------------
    Write-Host "`n========================================================================================" -ForegroundColor Cyan
    Write-Host "  Eatsbits WebAssembly & WebGPU Payload Analysis (Asset & Wire Footprint)" -ForegroundColor Cyan
    Write-Host "========================================================================================" -ForegroundColor Cyan

    $webArtifacts = @(
        @{ Name = "eatsbits.wasm"; Desc = "WebAssembly Core Binary" },
        @{ Name = "eatsbits.data"; Desc = "Preloaded Assets (Fonts/Presets)" },
        @{ Name = "eatsbits.js";   Desc = "Emscripten JS Runtime Bridge" },
        @{ Name = "eatsbits.html"; Desc = "HTML5 WebGPU Canvas Host" },
        @{ Name = "index.html";    Desc = "WebGPU Entry & PWA Host" },
        @{ Name = "sw.js";         Desc = "Offline PWA Service Worker" },
        @{ Name = "manifest.json"; Desc = "Web App Manifest & Metadata" }
    )

    $totalRaw = 0
    $totalWire = 0
    $metrics = @()

    foreach ($item in $webArtifacts) {
        $p = Join-Path $buildDir $item.Name
        if (Test-Path $p) {
            $rawBytes = (Get-Item $p).Length
            $totalRaw += $rawBytes

            # Compute Gzip compressed size
            $rawBuffer = [System.IO.File]::ReadAllBytes($p)
            $ms = New-Object System.IO.MemoryStream
            $gz = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionMode]::Compress)
            $gz.Write($rawBuffer, 0, $rawBuffer.Length)
            $gz.Close()
            $wireBytes = $ms.ToArray().Length
            $ms.Close()
            $totalWire += $wireBytes

            $savedPct = if ($rawBytes -gt 0) { [math]::Round((1.0 - ($wireBytes / $rawBytes)) * 100.0, 1) } else { 0.0 }

            $metrics += [PSCustomObject]@{
                Artifact = $item.Name
                Desc     = $item.Desc
                RawBytes = $rawBytes
                WireBytes= $wireBytes
                SavedPct = $savedPct
            }
        }
    }

    Write-Host ("  {0,-15} {1,12} {2,14} {3,12}   {4,-30}" -f "Artifact", "Raw Size", "Wire (Gzip)", "Savings", "Description") -ForegroundColor Yellow
    Write-Host "  --------------------------------------------------------------------------------------" -ForegroundColor Gray

    foreach ($m in $metrics) {
        $rawStr = if ($m.RawBytes -ge 1MB) { "{0:N2} MB" -f ($m.RawBytes / 1MB) } else { "{0:N1} KB" -f ($m.RawBytes / 1KB) }
        $wireStr = if ($m.WireBytes -ge 1MB) { "{0:N2} MB" -f ($m.WireBytes / 1MB) } else { "{0:N1} KB" -f ($m.WireBytes / 1KB) }
        $savedStr = "-{0:N1}%" -f $m.SavedPct
        Write-Host ("  {0,-15} {1,12} {2,14} {3,12}   {4,-30}" -f $m.Artifact, $rawStr, $wireStr, $savedStr, $m.Desc) -ForegroundColor White
    }

    Write-Host "  --------------------------------------------------------------------------------------" -ForegroundColor Gray
    $totRawStr = if ($totalRaw -ge 1MB) { "{0:N2} MB" -f ($totalRaw / 1MB) } else { "{0:N1} KB" -f ($totalRaw / 1KB) }
    $totWireStr = if ($totalWire -ge 1MB) { "{0:N2} MB" -f ($totalWire / 1MB) } else { "{0:N1} KB" -f ($totalWire / 1KB) }
    $totSavedPct = if ($totalRaw -gt 0) { [math]::Round((1.0 - ($totalWire / $totalRaw)) * 100.0, 1) } else { 0.0 }
    $totSavedStr = "-{0:N1}%" -f $totSavedPct
    Write-Host ("  {0,-15} {1,12} {2,14} {3,12}   {4,-30}" -f "TOTAL PAYLOAD", $totRawStr, $totWireStr, $totSavedStr, "CDN Wire Transfer Footprint") -ForegroundColor Green
    Write-Host "========================================================================================`n" -ForegroundColor Cyan
} else {
    Write-Host "`n[*] Skipped direct build (emcmake not in active shell)." -ForegroundColor Yellow
}

# Copy all web & PWA assets to build directory (excluding raw index.html template)
if (Test-Path "web") {
    Get-ChildItem -Path "web" -Exclude "index.html" | ForEach-Object {
        Copy-Item $_.FullName -Destination $buildDir -Force -ErrorAction SilentlyContinue
    }
}
if (Test-Path (Join-Path $buildDir "eatsbits.html")) {
    Copy-Item (Join-Path $buildDir "eatsbits.html") -Destination (Join-Path $buildDir "index.html") -Force -ErrorAction SilentlyContinue
}

# ------------------------------------------------------------------
# STEP 4: Local Dev Server with COOP / COEP Headers (Default unless -NoServe)
# ------------------------------------------------------------------
if (-not $NoServe) {
    Stop-ExistingWebServer -TargetPort $Port
    Write-Host "`n[*] STEP 4: Starting local HTTP server on port $Port..." -ForegroundColor Yellow
    Write-Host "    Serving headers for SharedArrayBuffer & WebGPU:" -ForegroundColor Gray
    Write-Host "      Cross-Origin-Opener-Policy: same-origin" -ForegroundColor Gray
    Write-Host "      Cross-Origin-Embedder-Policy: require-corp" -ForegroundColor Gray
    Write-Host "`n    [+] WebGPU DAW (PWA): http://localhost:$Port/index.html" -ForegroundColor Green
    Write-Host "    [+] Direct WebGPU:    http://localhost:$Port/eatsbits.html" -ForegroundColor Green
    Write-Host "    [+] Terminal DAW:     http://localhost:$Port/tui.html" -ForegroundColor Cyan

    $serverScript = @"
import sys, os, http.server, socketserver

web_dir = r'$buildDir'
if os.path.exists(web_dir):
    os.chdir(web_dir)

class CustomHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()

Handler = CustomHandler
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('', $Port), Handler) as httpd:
    print('Serving HTTP on 0.0.0.0 port $Port with COOP/COEP headers...')
    httpd.serve_forever()
"@

    $serverScriptPath = Join-Path $buildDir "serve.py"
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir | Out-Null
    }
    Set-Content -Path $serverScriptPath -Value $serverScript

    Write-Host "`n[*] Opening browser to http://localhost:$Port/eatsbits.html..." -ForegroundColor Cyan
    Start-Process "http://localhost:$Port/eatsbits.html"
    python $serverScriptPath
} else {
    Write-Host "`n[*] Build completed. Server launch skipped (-NoServe specified)." -ForegroundColor Gray
}
