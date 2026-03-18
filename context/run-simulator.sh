#!/bin/bash
# Run EGT App in X11 simulator mode (renders on host display)
# Usage: ./scripts/run-simulator.sh [--build] [--docker]
#
# Requires EGT installed natively:
#   sudo apt install libcairo-dev libdrm-dev libinput-dev ...
#   cd /opt/egt && mkdir build && cd build && cmake .. && make && sudo make install
#
# Or use --docker to run via Docker with X11 forwarding.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."
BUILD_DIR="$PROJECT_DIR/build-native"
IMAGE="egt-app-dev"

BUILD=false
USE_DOCKER=false

for arg in "$@"; do
    case $arg in
        --build)  BUILD=true ;;
        --docker) USE_DOCKER=true ;;
        *)        echo "Usage: $0 [--build] [--docker]"; exit 1 ;;
    esac
done

cd "$PROJECT_DIR"

if [ "$USE_DOCKER" = true ]; then
    # ── Docker mode ──────────────────────────────────────────────
    if [ "$BUILD" = true ] || [ ! -f build/egt-app ]; then
        echo "=== Building (Docker) ==="
        docker run --rm -v "$PWD:/app" -w /app "$IMAGE" \
            bash -c "mkdir -p build && cd build && cmake .. && make -j\$(nproc)"
    fi
    echo "=== Launching EGT Simulator (Docker + X11) ==="
    xhost +local:docker 2>/dev/null || true
    docker run --rm -it \
        --name egt-simulator \
        -v "$PWD:/app" -w /app/build \
        -e DISPLAY="${DISPLAY:-:0}" \
        -e EGT_BACKEND=x11 \
        -e EGT_SCREEN_SIZE=800x480 \
        -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
        -v /mnt/wslg:/mnt/wslg:rw 2>/dev/null \
        --network host \
        "$IMAGE" \
        ./egt-app
else
    # ── Native mode (recommended) ────────────────────────────────
    if [ "$BUILD" = true ] || [ ! -f "$BUILD_DIR/egt-app" ]; then
        echo "=== Building (Native) ==="
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
        make -j"$(nproc)"
        cd "$PROJECT_DIR"
    fi

    if [ ! -f "$BUILD_DIR/egt-app" ]; then
        echo "ERROR: Binary not found at $BUILD_DIR/egt-app"
        echo "       Run with --build to compile first."
        exit 1
    fi

    echo "=== Launching EGT Simulator (Native X11) ==="
    echo "    Resolution: 800x480"
    echo "    Press Ctrl+C to stop"
    echo ""
    cd "$BUILD_DIR"
    exec env EGT_BACKEND=x11 EGT_SCREEN_SIZE=800x480 ./egt-app
fi
