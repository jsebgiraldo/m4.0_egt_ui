#!/bin/bash
# Cross-compile EGT App for SAMA5D27 using Yocto Docker container
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
SDK_DIR="/home/sebas/workspace/suntek/m4/m4.0-core-operation"
DOCKER_IMAGE="suntek-yocto:latest"
CONTAINER_NAME="suntek-build"
MACHINE="sama5d27-wlsom1-ek-sd"

echo "=== Suntek EGT App Cross-Compilation ==="
echo "Project: $PROJECT_DIR"
echo "SDK:     $SDK_DIR"
echo ""

# Check if container exists and is running
if docker ps -q -f name="$CONTAINER_NAME" | grep -q .; then
    echo "Container '$CONTAINER_NAME' is already running."
elif docker ps -aq -f name="$CONTAINER_NAME" | grep -q .; then
    echo "Starting existing container '$CONTAINER_NAME'..."
    docker start "$CONTAINER_NAME"
else
    echo "ERROR: Container '$CONTAINER_NAME' not found. Run the Yocto build first."
    exit 1
fi

echo "=== Building my-egt-app via bitbake ==="

# Copy updated source into meta-layer externalsrc or use devtool
docker exec -u yocto "$CONTAINER_NAME" bash -c "
    # Source build environment
    export TEMPLATECONF=/opt/yocto/meta-atmel/conf/templates/default
    source /opt/yocto/poky/oe-init-build-env /opt/sdk/build-microchip

    # Build the EGT app recipe
    bitbake my-egt-app -c cleansstate 2>/dev/null || true
    bitbake my-egt-app
"

echo ""
echo "=== Build Complete ==="
echo "Binary should be available in the Yocto deploy directory"
