#!/bin/bash
set -e

echo "================================================================================"
echo "   AUDIO-VOYAGER // WINDOWS 64-BIT PORTABLE RELEASE BUILDER                     "
echo "================================================================================"

BUILD_DIR="/root/build_win"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "[1/3] Configuring CMake for Windows x86_64..."
cmake /workspace \
    -DCMAKE_TOOLCHAIN_FILE=/workspace/toolchain-mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    -GNinja

echo "[2/3] Compiling Procedural Gyroid SDF & AGC Semantic Engine..."
cmake --build "$BUILD_DIR"

echo "[3/3] Assembling Standalone Portable Release Package..."
DIST_DIR="/workspace/dist/audio-voyager-windows"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/shaders"

cp -f "$BUILD_DIR/bin/audio_voyager.exe" "$DIST_DIR/audio_voyager.exe"
cp -r /workspace/shaders/* "$DIST_DIR/shaders/"

# Attempt copy to root workspace
cp -f "$BUILD_DIR/bin/audio_voyager.exe" /workspace/audio_voyager.exe 2>/dev/null || true
cp -f "$BUILD_DIR/bin/audio_voyager.exe" /workspace/bin/audio_voyager.exe 2>/dev/null || true

# Create single-click Windows batch launcher
cat << 'EOF' > "$DIST_DIR/run_audio_voyager.bat"
@echo off
title AUDIO-VOYAGER // PROCEDURAL GYROID SDF & AGC
cd /d "%~dp0"
echo ================================================================================
echo    AUDIO-VOYAGER // Starting Procedural Gyroid Hyperspace Flight...
echo    Listening to system audio loopback in real-time.
echo    Shortcuts: [F11] Fullscreen | [F12] Debug HUD | [ESC] Exit
echo ================================================================================
audio_voyager.exe
EOF

echo "================================================================================"
echo "   PORTABLE RELEASE READY!                                                     "
echo "   Root Executable: ./audio_voyager.exe                                        "
echo "   Portable Folder: ./dist/audio-voyager-windows/                              "
echo "================================================================================"
ls -lah /workspace/audio_voyager.exe
ls -lah "$DIST_DIR"
