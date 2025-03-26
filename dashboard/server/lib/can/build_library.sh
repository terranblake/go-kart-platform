#!/bin/bash
# Script to build the libcaninterface.so shared library
# Run this on the Raspberry Pi after making changes to the C API code

# Set the project root directory
PROJECT_ROOT="$( cd "$(dirname "$0")/../../../../" && pwd )"
CAN_LIB_DIR="$PROJECT_ROOT/components/common/lib/CrossPlatformCAN"
OUTPUT_DIR="$PROJECT_ROOT/dashboard/server/lib/can"

echo "Building libcaninterface.so from CrossPlatformCAN sources"
echo "Project root: $PROJECT_ROOT"
echo "CAN library sources: $CAN_LIB_DIR"
echo "Output directory: $OUTPUT_DIR"

# Create a clean build
rm -f $OUTPUT_DIR/libcaninterface.so

# Build the shared library
g++ -shared -o $OUTPUT_DIR/libcaninterface.so -fPIC \
    $CAN_LIB_DIR/c_api.cpp \
    $CAN_LIB_DIR/ProtobufCANInterface.cpp \
    $CAN_LIB_DIR/CANInterface.cpp \
    -I$CAN_LIB_DIR \
    -I$CAN_LIB_DIR/include \
    -lcanard \
    -std=c++11

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "✅ Build successful: $(ls -la $OUTPUT_DIR/libcaninterface.so)"
    echo "Library symbols:"
    nm -D $OUTPUT_DIR/libcaninterface.so | grep can_interface
else
    echo "❌ Build failed"
fi 