#!/usr/bin/env bash
# Capture the running x86 simulator window to a PNG.
#
# Usage:
#   ./scripts/screenshot-sim.sh <output-path>
#   ./scripts/screenshot-sim.sh docs/10-reports/m4-18-wifi-init-figma-match/before.png
#
# Finds the egt-app process, gets its X11 window via xdotool, and uses
# ImageMagick `import` to capture just that window (no manual click).

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "usage: $0 <output.png>" >&2
    exit 2
fi

OUT="$1"
mkdir -p "$(dirname "$OUT")"

for bin in xdotool import; do
    if ! command -v "$bin" >/dev/null; then
        echo "error: $bin not installed" >&2
        exit 1
    fi
done

if ! pidof egt-app >/dev/null; then
    echo "error: egt-app is not running. Launch the simulator first (Run / Build & Run)." >&2
    exit 1
fi

# EGT doesn't set _NET_WM_PID. Two windows are typically named "EGT":
# the outer window-manager frame (with title bar / borders) and the inner
# content window EGT actually draws into. We want the inner one - it has
# dimensions exactly matching EGT_SCREEN_SIZE (default 800x480).
SIZE="${EGT_SCREEN_SIZE:-800x480}"
EGT_W="${SIZE%x*}"
EGT_H="${SIZE#*x}"

mapfile -t WINDOWS < <(xdotool search --name '^EGT$' 2>/dev/null || true)
if [[ ${#WINDOWS[@]} -eq 0 ]]; then
    echo "error: no X11 window named 'EGT' found." >&2
    exit 1
fi

BEST_WID=""
for wid in "${WINDOWS[@]}"; do
    geom="$(xdotool getwindowgeometry --shell "$wid" 2>/dev/null || true)"
    [[ -z "$geom" ]] && continue
    eval "$geom"
    if (( WIDTH == EGT_W && HEIGHT == EGT_H )); then
        BEST_WID=$wid
        break
    fi
done

# Fallback: smallest EGT window (inner content is always smaller than WM frame).
if [[ -z "$BEST_WID" ]]; then
    BEST_AREA=0
    for wid in "${WINDOWS[@]}"; do
        geom="$(xdotool getwindowgeometry --shell "$wid" 2>/dev/null || true)"
        [[ -z "$geom" ]] && continue
        eval "$geom"
        area=$(( WIDTH * HEIGHT ))
        if (( BEST_AREA == 0 || area < BEST_AREA )); then
            BEST_AREA=$area
            BEST_WID=$wid
        fi
    done
fi

if [[ -z "$BEST_WID" ]]; then
    echo "error: no measurable EGT window" >&2
    exit 1
fi

eval "$(xdotool getwindowgeometry --shell "$BEST_WID")"
import -window "$BEST_WID" "$OUT"
echo "[screenshot-sim] saved $OUT  (${WIDTH}x${HEIGHT} from window $BEST_WID)"
