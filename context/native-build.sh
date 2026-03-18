#!/bin/bash
# Build EGT App natively (x86) using Docker for development/testing
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR/.."

echo "=== Building EGT App (Native x86) ==="

cd "$PROJECT_DIR"

# Build Docker image if needed
docker build -t egt-app-dev .

# Build the application inside the container
docker run --rm \
    -v "$PROJECT_DIR:/app" \
    -w /app \
    egt-app-dev \
    bash -c "
        rm -rf build-native && mkdir build-native && cd build-native
        cmake .. && make -j\$(nproc)
        echo '=== Build successful ==='
        ls -la egt-app
    "

echo ""
echo "=== Native build complete ==="
echo "Binary: build-native/egt-app"
