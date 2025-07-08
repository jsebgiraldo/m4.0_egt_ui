FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Install required build tools and dependencies
RUN apt-get update && apt-get install -y \
    git build-essential automake autoconf libtool pkg-config \
    cmake g++ \
    libdrm-dev libinput-dev libcairo-dev libjpeg-dev libmagic-dev gettext \
    librsvg2-dev liblua5.3-dev libcurl4-openssl-dev \
    libxkbcommon-dev xkb-data \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libplplot-dev plplot-driver-cairo \
    libasound2-dev libsndfile1-dev \
    sudo wget \
    xxd x11-apps

# Clone and build EGT
WORKDIR /opt
RUN git clone --recursive https://github.com/linux4sam/egt.git
WORKDIR /opt/egt
RUN ./autogen.sh && ./configure && make && sudo make install && sudo ldconfig

# Copy user application excluding build and CMakeCache.txt
COPY . /app
WORKDIR /app

# Cleanup CMakeCache.txt if exists
RUN rm -f CMakeCache.txt && rm -rf build

# Build user application
RUN mkdir -p build && cd build && cmake .. && make