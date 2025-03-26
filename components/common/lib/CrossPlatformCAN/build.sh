#!/bin/bash

# CrossPlatformCAN build script for Linux/Raspberry Pi
# This script will build the library, examples and tests

# Ensure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Build tests by default
BUILD_TESTS=1

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --no-tests)
      BUILD_TESTS=0
      shift
      ;;
    *)
      # Unknown option
      shift
      ;;
  esac
done

# Clean previous build
echo "Cleaning previous build..."
rm -rf build
mkdir -p build
cd build

# Set CMake options
CMAKE_OPTIONS="-DBUILD_SHARED_LIBS=ON"
if [ $BUILD_TESTS -eq 1 ]; then
  CMAKE_OPTIONS="$CMAKE_OPTIONS -DBUILD_TESTS=ON"
else
  CMAKE_OPTIONS="$CMAKE_OPTIONS -DBUILD_TESTS=OFF"
fi

# Run CMake with shared library enabled (required for Python CFFI)
echo "Configuring build with CMake with options: $CMAKE_OPTIONS"
cmake .. $CMAKE_OPTIONS

# Build the library, examples and tests
echo "Building library, examples and tests..."
make VERBOSE=1

# Check if build was successful
if [ $? -eq 0 ]; then
    echo "Build successful!"
    echo ""
    echo "Library built at: $(pwd)/libCrossPlatformCAN.so"
    
    # Create a symlink for Python in the python directory
    mkdir -p ../python
    
    # Remove any existing symlink
    rm -f "../python/libCrossPlatformCAN.so"
    
    # Create a new symlink
    ln -sf "$(pwd)/libCrossPlatformCAN.so" "../python/libCrossPlatformCAN.so"
    echo "Created symlink to shared library in python directory"
    
    # Verify exported symbols
    echo ""
    echo "Verifying exported symbols:"
    nm -D libCrossPlatformCAN.so | grep can_interface
    
    if [ -f rpi_example ]; then
        echo "Raspberry Pi example built at: $(pwd)/rpi_example"
    fi
    
    # Run tests if enabled
    if [ $BUILD_TESTS -eq 1 ] && [ -f test/can_tests ]; then
        echo ""
        echo "Running tests..."
        CTEST_OUTPUT_ON_FAILURE=1 make test
        
        if [ $? -eq 0 ]; then
            echo "All tests passed!"
        else
            echo "Some tests failed!"
            exit 1
        fi
    fi
    
    echo ""
    echo "To run the Raspberry Pi example, make sure your CAN interface is configured:"
    echo "  sudo ip link set can0 type can bitrate 500000"
    echo "  sudo ip link set up can0"
    echo ""
    echo "Then run:"
    echo "  ./rpi_example"
    echo ""
    echo "To run the Python example:"
    echo "  cd ../python"
    echo "  python3 test_can_interface.py"
else
    echo "Build failed!"
    exit 1
fi 