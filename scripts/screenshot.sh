#!/bin/bash
# Capture screenshot from SAMA5D27 target via DRM framebuffer
# Usage: ./screenshot.sh [output.png]
set -e
TARGET=${TARGET:-root@192.168.1.125}
OUTPUT=${1:-/tmp/target-screen.png}
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Capturing screen from $TARGET..."
# Copy capture script if not present, then run it
ssh $TARGET "test -f /usr/bin/capture-screen.py || true"
scp -q "$SCRIPT_DIR/capture-screen.py" $TARGET:/usr/bin/capture-screen.py
ssh $TARGET "python3 /usr/bin/capture-screen.py /tmp/screen.ppm"

echo "Downloading..."
scp -q $TARGET:/tmp/screen.ppm /tmp/target-screen.ppm

# Convert PPM to PNG if ImageMagick is available
if command -v convert &>/dev/null; then
    convert /tmp/target-screen.ppm "$OUTPUT"
    rm -f /tmp/target-screen.ppm
    echo "Saved: $OUTPUT"
else
    OUTPUT="${OUTPUT%.png}.ppm"
    mv /tmp/target-screen.ppm "$OUTPUT"
    echo "Saved: $OUTPUT (install ImageMagick for PNG)"
fi
