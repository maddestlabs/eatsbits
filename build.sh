#!/usr/bin/env bash
# ==============================================================================
# Eatsbits Automated Build & Test Script for Linux / macOS / POSIX
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONFIG="Release"
TARGET="all"
DO_CLEAN=false
DO_TEST=false
NO_RUN=false
DO_CLI=false
DO_TUI=false

print_usage() {
    echo "Usage: ./build.sh [options]"
    echo "Options:"
    echo "  --clean            Clean build directory before compiling"
    echo "  --config <CONFIG>  Build config: Release (default) or Debug"
    echo "  --target <TARGET>  Specific CMake target (default: all, or eatsbits_gui, eatsbits_tui, eatsbits_cli)"
    echo "  --test             Run automated test suite after compilation"
    echo "  --no-run           Compile only, do not launch executable"
    echo "  --tui              Launch eatsbits_tui terminal DAW upon success"
    echo "  --cli              Launch eatsbits_cli test harness upon success"
    echo "  --help             Display this help message"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --clean)
            DO_CLEAN=true
            shift
            ;;
        --config)
            CONFIG="$2"
            shift 2
            ;;
        --target)
            TARGET="$2"
            shift 2
            ;;
        --test)
            DO_TEST=true
            shift
            ;;
        --no-run)
            NO_RUN=true
            shift
            ;;
        --tui)
            DO_TUI=true
            shift
            ;;
        --cli)
            DO_CLI=true
            shift
            ;;
        --run)
            # Kept for compatibility
            shift
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done
            print_usage
            exit 1
            ;;
    esac
done

echo "============================================================"
echo "  Eatsbits C++ Modular DAW Automated Build Script (POSIX)   "
echo "============================================================"

# Step 1: Verify Toolchain
echo ""
echo "[*] STEP 1: Verifying toolchain..."
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is required but not found in PATH." >&2
    exit 1
fi
echo "    Found CMake: $(cmake --version | head -n 1)"

BUILD_DIR="$SCRIPT_DIR/build"
if [ "$DO_CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
    echo "    Cleaning build directory ('$BUILD_DIR')..."
    rm -rf "$BUILD_DIR"
fi

# Step 2: Configure CMake
echo ""
echo "[*] STEP 2: Configuring CMake project..."
mkdir -p "$BUILD_DIR"
if [ "$DO_TEST" = true ]; then
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE="$CONFIG" -DBUILD_TESTING=ON
else
    cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE="$CONFIG" -DBUILD_TESTING=OFF
fi

# Step 3: Build Targets
echo ""
echo "[*] STEP 3: Building Eatsbits ($CONFIG mode)..."
if [ "$TARGET" != "all" ]; then
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$TARGET" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
else
    cmake --build "$BUILD_DIR" --config "$CONFIG" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"
fi

echo ""
echo "[+] Build completed successfully!"

# Locate binaries
OUT_DIR="$BUILD_DIR"
if [ -d "$BUILD_DIR/$CONFIG" ]; then
    OUT_DIR="$BUILD_DIR/$CONFIG"
fi

GUI_BIN="$OUT_DIR/eatsbits_gui"
CLI_BIN="$OUT_DIR/eatsbits_cli"
TUI_BIN="$OUT_DIR/eatsbits_tui"

# Step 4: Run Tests
if [ "$DO_TEST" = true ]; then
    echo ""
    echo "[*] STEP 4: Running test suite..."
    ctest --test-dir "$BUILD_DIR" -C "$CONFIG" --output-on-failure
fi

# Step 5: Launch Executable (Default unless --no-run)
if [ "$NO_RUN" = false ]; then
    if [ "$DO_TUI" = true ]; then
        if [ -x "$TUI_BIN" ]; then
            echo ""
            echo "[*] Launching Eatsbits Terminal DAW..."
            "$TUI_BIN"
        else
            echo "Error: TUI binary not found at $TUI_BIN" >&2
            exit 1
        fi
    elif [ "$DO_CLI" = true ]; then
        if [ -x "$CLI_BIN" ]; then
            echo ""
            echo "[*] Launching Eatsbits CLI..."
            "$CLI_BIN"
        else
            echo "Error: CLI binary not found at $CLI_BIN" >&2
            exit 1
        fi
    else
        if [ -x "$GUI_BIN" ]; then
            echo ""
            echo "[*] Launching Eatsbits Desktop GUI..."
            "$GUI_BIN" &
        else
            echo "Error: GUI binary not found at $GUI_BIN" >&2
            exit 1
        fi
    fi
else
    echo "[*] Build completed. Launch skipped (--no-run specified)."
fi
