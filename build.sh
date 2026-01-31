#!/bin/bash

# PassFlow Build Script

set -e

echo "================================"
echo "PassFlow System Build Script"
echo "================================"
echo ""

# Check for required tools
echo "Checking dependencies..."

if ! command -v cmake &> /dev/null; then
    echo "ERROR: CMake is not installed"
    echo "Install with: sudo apt-get install cmake"
    exit 1
fi

if ! command -v g++ &> /dev/null; then
    echo "ERROR: g++ is not installed"
    echo "Install with: sudo apt-get install build-essential"
    exit 1
fi

if ! command -v ffmpeg &> /dev/null; then
    echo "WARNING: FFmpeg is not installed"
    echo "Install with: sudo apt-get install ffmpeg"
fi

echo "All required tools found!"
echo ""

# Create directory structure
echo "Creating directory structure..."
mkdir -p ~/PassFlow/{Log,Cam0Source,Cam0,Cam1Source,Cam1}
echo "Directories created in ~/PassFlow/"
echo ""

# Build the project
echo "Building project..."
mkdir -p build
cd build

# Clean previous build if exists
if [ -f "Makefile" ]; then
    echo "Cleaning previous build..."
    make clean 2>/dev/null || true
fi

# Run CMake
echo "Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Compile
echo "Compiling..."
make -j$(nproc)

echo ""
echo "================================"
echo "Build completed successfully!"
echo "================================"
echo ""
echo "Executable: build/passflow"
echo ""
echo "To run the application:"
echo "  ./build/passflow"
echo ""
echo "To install system-wide:"
echo "  sudo make install"
echo ""
echo "Configuration file example:"
echo "  config.ini.example"
echo ""
