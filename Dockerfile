# ==============================================================================
# audio-voyager: Hermetic Build & Execution Environment
# Target: C++20, Essentia C++, Real-Time Audio (ALSA/Pulse/JACK), GPU Compute
# Windows .exe Cross-Compilation Support: mingw-w64
# ==============================================================================

FROM ubuntu:24.04 AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Etc/UTC

# 1. System packages, modern compilers and build tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc \
    g++ \
    clang \
    lld \
    mingw-w64 \
    cmake \
    ninja-build \
    git \
    pkg-config \
    ca-certificates \
    curl \
    wget \
    python3 \
    python3-pip \
    python3-yaml \
    python3-numpy \
    libasound2-dev \
    libpulse-dev \
    libjack-jackd2-dev \
    libfftw3-dev \
    libyaml-dev \
    libsamplerate0-dev \
    libtag1-dev \
    libeigen3-dev \
    libavcodec-dev \
    libavformat-dev \
    libavutil-dev \
    libswresample-dev \
    libchromaprint-dev \
    libgl1-mesa-dev \
    libgl1-mesa-dri \
    libvulkan-dev \
    libglfw3-dev \
    libglew-dev \
    libglm-dev \
    libx11-dev \
    libxrandr-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    xvfb \
    pulseaudio-utils \
    alsa-utils \
    && rm -rf /var/lib/apt/lists/*

# 2. Build & Install Essentia C++ from official repository
WORKDIR /tmp/essentia_build
RUN git clone --depth 1 https://github.com/MTG/essentia.git . \
    && python3 waf configure --mode=release \
    && python3 waf \
    && python3 waf install \
    && ldconfig \
    && rm -rf /tmp/essentia_build

# 3. Setup workspace directory
WORKDIR /workspace

# Default command
CMD ["bash"]
