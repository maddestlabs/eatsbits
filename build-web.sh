#!/usr/bin/env bash
# Automated WebAssembly & WebGPU build script for Eatsbits Modular DAW using Bash.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

echo "============================================================"
echo "  Eatsbits WebAssembly & WebGPU Automated Build Pipeline"
echo "============================================================"

if ! command -v emcc &> /dev/null; then
    echo "[!] emcc (Emscripten) not found in PATH."
    echo "    Please install or activate emsdk: source /path/to/emsdk/emsdk_env.sh"
    exit 1
fi

BUILD_DIR="${SCRIPT_DIR}/build-web"

if [ "$1" == "--clean" ]; then
    echo "[*] Cleaning previous web build directory..."
    rm -rf "${BUILD_DIR}"
fi

mkdir -p "${BUILD_DIR}"

echo "[*] Configuring CMake with Emscripten..."
emcmake cmake -B build-web -S . -DCMAKE_BUILD_TYPE=Release

echo "[*] Compiling eatsbits_web..."
cmake --build build-web --target eatsbits_web --config Release

echo "[+] Build complete: ${BUILD_DIR}/eatsbits.html"

NO_SERVE=false
for arg in "$@"; do
    if [ "$arg" == "--no-serve" ]; then
        NO_SERVE=true
    fi
done

# Copy all web & PWA assets (excluding raw index.html template)
if [ -d "web" ]; then
    find web -maxdepth 1 -type f ! -name "index.html" -exec cp -f {} "${BUILD_DIR}/" \;
fi
if [ -f "${BUILD_DIR}/eatsbits.html" ]; then
    cp -f "${BUILD_DIR}/eatsbits.html" "${BUILD_DIR}/index.html"
fi

if [ "$NO_SERVE" = false ]; then
    echo "[*] Ensuring existing web servers on port 8080 are closed..."
    if command -v lsof &> /dev/null; then
        EXISTING_PIDS=$(lsof -ti :8080 2>/dev/null || true)
        if [ -n "$EXISTING_PIDS" ]; then
            for p in $EXISTING_PIDS; do
                echo "    [!] Closing process on port 8080 (PID: $p)..."
                kill -9 "$p" 2>/dev/null || true
            done
            sleep 0.3
        fi
    elif command -v fuser &> /dev/null; then
        fuser -k 8080/tcp 2>/dev/null || true
        sleep 0.3
    fi

    echo "[*] Launching local server with COOP/COEP headers on port 8080..."
    echo "    [+] WebGPU DAW (PWA): http://localhost:8080/index.html"
    echo "    [+] Direct WebGPU:    http://localhost:8080/eatsbits.html"
    echo "    [+] Terminal DAW:     http://localhost:8080/tui.html"

    # Open browser on macOS or Linux
    if command -v open &> /dev/null; then
        open "http://localhost:8080/eatsbits.html" &
    elif command -v xdg-open &> /dev/null; then
        xdg-open "http://localhost:8080/eatsbits.html" &
    fi

    cd "${BUILD_DIR}"
    python3 -c "
import http.server, socketserver
class Handler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        super().end_headers()
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('', 8080), Handler) as httpd:
    print('Serving HTTP on 0.0.0.0 port 8080 with COOP/COEP headers...')
    httpd.serve_forever()
"
else
    echo "[*] Build completed. Server launch skipped (--no-serve specified)."
fi
