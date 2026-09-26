#!/usr/bin/env bash
# ==============================================================================
# Eatsbits Automated Build & Launch Script for Terminal DAW (POSIX)
# ==============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONFIG="Release"
DO_CLEAN=false
DO_TEST=false
NO_RUN=false
USE_LEGACY=false

print_usage() {
    echo "Usage: ./build-cli.sh [options]"
    echo "Options:"
    echo "  --clean            Clean build directory before compiling"
    echo "  --config <CONFIG>  Build config: Release (default) or Debug"
    echo "  --no-run           Compile only, do not launch executable"
    echo "  --legacy           Build and launch legacy CLI test harness (eatsbits_cli)"
    echo "  --test             Run automated test suite before launching"
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
        --no-run)
            NO_RUN=true
            shift
            ;;
        --legacy)
            USE_LEGACY=true
            shift
            ;;
        --test)
            DO_TEST=true
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

echo -e "\033[1;36m============================================================\033[0m"
echo -e "\033[1;36m  Eatsbits Terminal DAW Automated Build & Launch (POSIX)\033[0m"
echo -e "\033[1;36m============================================================\033[0m"

BUILD_DIR="$SCRIPT_DIR/build"

if [ "$DO_CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
    echo -e "\033[1;33m[*] Cleaning build directory...\033[0m"
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"

TARGET="eatsbits_tui"
if [ "$USE_LEGACY" = true ]; then
    TARGET="eatsbits_cli"
fi

echo -e "\033[1;33m[*] Configuring CMake for $TARGET ($CONFIG)...\033[0m"
CMAKE_TEST_ARG="-DBUILD_TESTING=OFF"
if [ "$DO_TEST" = true ]; then
    CMAKE_TEST_ARG="-DBUILD_TESTING=ON"
fi

cmake -B "$BUILD_DIR" -S "$SCRIPT_DIR" -DCMAKE_BUILD_TYPE="$CONFIG" "$CMAKE_TEST_ARG"

echo -e "\033[1;33m[*] Compiling $TARGET...\033[0m"
BUILD_TARGETS=("$TARGET")
if [ "$DO_TEST" = true ]; then
    BUILD_TARGETS+=("test_tui_surface")
fi

cmake --build "$BUILD_DIR" --target "${BUILD_TARGETS[@]}" --config "$CONFIG" -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

if [ "$DO_TEST" = true ]; then
    echo -e "\033[1;33m[*] Running TUI verification tests...\033[0m"
    "$BUILD_DIR/test_tui_surface"
fi

BIN_PATH="$BUILD_DIR/$TARGET"
if [ ! -f "$BIN_PATH" ]; then
    BIN_PATH="$BUILD_DIR/$CONFIG/$TARGET"
fi

if [ "$NO_RUN" = false ]; then
    echo -e "\033[1;32m[*] Launching $TARGET...\033[0m"
    "$BIN_PATH"
fi
