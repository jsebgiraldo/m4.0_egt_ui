#!/bin/bash
cd "$(dirname "$0")"

xhost +local:docker 2>/dev/null

docker run --rm -it \
    -v "$PWD:/app" -w /app/build \
    -e DISPLAY="$DISPLAY" \
    -e EGT_BACKEND=x11 \
    -e EGT_SCREEN_SIZE=800x480 \
    -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
    --network host \
    egt-app-dev \
    ./egt-app
