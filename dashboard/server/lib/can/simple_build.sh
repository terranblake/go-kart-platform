#!/bin/bash
# Simple build script for the CAN interface library
# This builds a simplified version that focuses just on animation commands

set -e  # Exit on any error

# Determine directories
PROJECT_ROOT="$(cd "$(dirname "$0")/../../../../" && pwd)"
CAN_LIB_DIR="${PROJECT_ROOT}/components/common/lib/CrossPlatformCAN"
OUTPUT_DIR="${PROJECT_ROOT}/dashboard/server/lib/can"

echo "Building simplified libcaninterface.so"
echo "Project root: ${PROJECT_ROOT}"
echo "CAN library source: ${CAN_LIB_DIR}"
echo "Output directory: ${OUTPUT_DIR}"

# Make sure output directory exists
mkdir -p "${OUTPUT_DIR}"

# Remove old library if it exists
if [ -f "${OUTPUT_DIR}/libcaninterface.so" ]; then
    echo "Removing old library"
    rm "${OUTPUT_DIR}/libcaninterface.so"
fi

# Compile the library
echo "Compiling library..."
g++ -std=c++11 -Wall -fPIC -shared \
    -o "${OUTPUT_DIR}/libcaninterface.so" \
    "${CAN_LIB_DIR}/c_api.cpp" \
    "${CAN_LIB_DIR}/CANInterface.cpp" \
    -I"${CAN_LIB_DIR}" \
    -I"${CAN_LIB_DIR}/include" \
    -lcanard

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful"
    echo "Library symbols:"
    nm -D "${OUTPUT_DIR}/libcaninterface.so" | grep -E 'can_interface|T _' | sort
    echo "Library size: $(du -h "${OUTPUT_DIR}/libcaninterface.so" | cut -f1)"
    chmod +x "${OUTPUT_DIR}/libcaninterface.so"
else
    echo "Build failed"
    exit 1
fi 