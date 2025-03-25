#!/bin/bash
# Simple build script for CrossPlatformCAN library on Raspberry Pi

echo "Building CrossPlatformCAN for Raspberry Pi..."

# Ensure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Make sure our output directories exist
mkdir -p build
mkdir -p python

# Set library name
LIBRARY="libCrossPlatformCAN.so"

# Update the header file to use local includes
echo "Updating include paths..."
sed -i 's|#include "common.pb.h"|#include "include/common.pb.h"|g' ProtobufCANInterface.h
sed -i 's|#include "nanopb/pb.h"|#include "include/nanopb/pb.h"|g' include/*.pb.h

# Compile with proper flags for shared library and exported symbols
echo "Compiling library with DEBUG_MODE=1 and PLATFORM_LINUX defined..."
g++ -shared -fPIC -Wall -Wextra \
    -I. \
    -I./include \
    -D__linux__ \
    -DPLATFORM_LINUX \
    -DDEBUG_MODE=1 \
    -o build/$LIBRARY \
    CANInterface.cpp \
    ProtobufCANInterface.cpp \
    c_api.cpp \
    -lstdc++

# Restore the header file
echo "Restoring original files..."
sed -i 's|#include "include/common.pb.h"|#include "common.pb.h"|g' ProtobufCANInterface.h
sed -i 's|#include "include/nanopb/pb.h"|#include "nanopb/pb.h"|g' include/*.pb.h

# Check if compilation succeeded
if [ $? -eq 0 ]; then
    echo "Compilation successful!"
    
    # Copy to python directory
    cp build/$LIBRARY python/
    echo "Copied library to python directory"
    
    # Show exported symbols
    echo "Exported symbols:"
    nm -D build/$LIBRARY | grep can_interface
    
    # Check if our C API functions are exported
    if nm -D build/$LIBRARY | grep -q "can_interface_create"; then
        echo "Library successfully exports C API functions!"
        echo "C API Functions:"
        nm -D build/$LIBRARY | grep " T " | grep can_interface
    else
        echo "WARNING: Library does not export C API functions!"
    fi
    
    echo "Build complete. Library available at:"
    echo "  $(pwd)/build/$LIBRARY"
    echo "  $(pwd)/python/$LIBRARY"
else
    echo "Compilation failed!"
fi 