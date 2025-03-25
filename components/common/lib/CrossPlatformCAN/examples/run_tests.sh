#!/bin/bash
# Script to run the CrossPlatformCAN tests

set -e  # Exit on any error

echo "Building and running CrossPlatformCAN tests..."

# Create a build directory if it doesn't exist
mkdir -p build
cd build

# Build using CMake
echo "Configuring with CMake..."
cmake .. -DBUILD_TESTS=ON
echo "Building tests..."
make

# Run tests
echo "Running tests..."
./header_test

echo "All tests completed successfully." 