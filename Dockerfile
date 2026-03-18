FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install required build tools and dependencies
RUN apt-get update && apt-get install -y \
    git build-essential automake autoconf libtool pkg-config \
    cmake g++ ninja-build \
    libdrm-dev libinput-dev libcairo-dev libjpeg-dev libmagic-dev gettext \
    librsvg2-dev liblua5.3-dev libcurl4-openssl-dev \
    libxkbcommon-dev xkb-data \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libplplot-dev plplot-driver-cairo \
    libasound2-dev libsndfile1-dev \
    sudo wget \
    xxd x11-apps network-manager dbus \
    libnm-dev \
    libdbus-1-dev \
    libglib2.0-bin \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

# Clone and build EGT (v1.12 uses CMake)
WORKDIR /opt
RUN git clone --recursive https://github.com/linux4sam/egt.git
WORKDIR /opt/egt
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Set working directory for the application
WORKDIR /app

CMD ["bash"]
