#!/bin/bash

# Create include directory structure
mkdir -p include/nanopb
mkdir -p include/CAN
mkdir -p include/arduino

# Run the prepare_includes script to gather dependencies
./prepare_includes.sh

# Create build directory
mkdir -p build
cd build

# Run CMake
cmake ..

# Build
make

echo "Build completed. Library is in build/libProtobufCANInterface.a" 