#!/usr/bin/env bash
# Launch the EGT app in the x86 simulator (X11 window).
#
# Usage:
#   ./scripts/run-simulator.sh                # run (assumes already built)
#   ./scripts/run-simulator.sh --build        # configure + build, then run
#   ./scripts/run-simulator.sh --clean        # nuke build dir + reconfigure + build + run
#   ./scripts/run-simulator.sh --wifi-static  # show the wifi-connected screen directly
#
# Env overrides:
#   BUILD_DIR     default: build-x86
#   EGT_SCREEN_SIZE  default: 800x480 (matches SAMA target panel)
#   EGT_MOCK_WIFI default: 1 (dynamic mock with rotating APs)
#                 set to "connected" to force the WIFI_CONNECTED success screen
#                 unset to use real wpa_cli (requires wpa_supplicant on host — uncommon)
#
# Requires host libegt 1.12+ installed (see README for install steps).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build-x86}"
DO_BUILD=0
DO_CLEAN=0

for arg in "$@"; do
    case "$arg" in
        --build)         DO_BUILD=1 ;;
        --clean)         DO_CLEAN=1; DO_BUILD=1 ;;
        --wifi-static)   export EGT_MOCK_WIFI="connected" ;;
        -h|--help)
            sed -n '2,20p' "$0"
            exit 0
            ;;
        *)
            echo "Unknown arg: $arg" >&2
            exit 2
            ;;
    esac
done

if [[ $DO_CLEAN -eq 1 ]]; then
    echo "[run-simulator] removing $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

if [[ $DO_BUILD -eq 1 ]] || [[ ! -x "$BUILD_DIR/egt-app" ]]; then
    echo "[run-simulator] configuring + building in $BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"
    cmake "$ROOT_DIR" >/dev/null
    make -j"$(nproc)"
    cd "$ROOT_DIR"
fi

if ! pkg-config --exists libegt; then
    echo "ERROR: host libegt is not installed. See README for install steps." >&2
    exit 1
fi

: "${DISPLAY:=:0}"
: "${EGT_SCREEN_SIZE:=800x480}"
: "${EGT_MOCK_WIFI:=1}"
export DISPLAY EGT_SCREEN_SIZE EGT_MOCK_WIFI
export EGT_BACKEND="${EGT_BACKEND:-x11}"

echo "[run-simulator] DISPLAY=$DISPLAY  size=$EGT_SCREEN_SIZE  mock=$EGT_MOCK_WIFI"
echo "[run-simulator] launching $BUILD_DIR/egt-app"

exec "$BUILD_DIR/egt-app"
