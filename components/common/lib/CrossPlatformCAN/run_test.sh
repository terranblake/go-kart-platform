#!/bin/bash
# Script to build the library using raspi_build.sh and run the LED animation test

echo "==== LED Animation Test ===="
echo "Building the CrossPlatformCAN library first..."

# Ensure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# First build the library with the raspi_build.sh script
if [ -x "./raspi_build.sh" ]; then
    ./raspi_build.sh
else
    echo "Error: raspi_build.sh not found or not executable"
    exit 1
fi

# Now compile our test with the same include paths
echo "Building test program..."
g++ -std=c++11 -Wall -fPIC \
    -I. \
    -I./include \
    -D__linux__ \
    -DPLATFORM_LINUX \
    -DDEBUG_MODE=1 \
    test_led_animation.cpp \
    -L./build \
    -lCrossPlatformCAN \
    -o test_led_animation

if [ $? -eq 0 ]; then
    echo "Compilation successful. Running test..."
    
    # Copy the library to /usr/local/lib where it can be found
    echo "Copying library to /usr/local/lib for execution..."
    sudo cp build/libCrossPlatformCAN.so /usr/local/lib/
    sudo ldconfig
    
    # Run the test
    sudo LD_LIBRARY_PATH=$SCRIPT_DIR/build:$LD_LIBRARY_PATH ./test_led_animation
else
    echo "Compilation failed."
    exit 1
fi 