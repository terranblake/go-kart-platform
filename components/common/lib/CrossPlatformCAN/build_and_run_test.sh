#!/bin/bash
# Script to build and run the LED animation test

echo "==== LED Animation Test ===="
echo "Building test program..."

# Ensure platform is defined for Linux
export CXXFLAGS="-DPLATFORM_LINUX"

# Compile the test program with all necessary sources
g++ -std=c++11 -Wall ${CXXFLAGS} -I. \
    test_led_animation.cpp \
    CANInterface.cpp \
    ProtobufCANInterface.cpp \
    c_api.cpp \
    -o test_led_animation

if [ $? -eq 0 ]; then
    echo "Compilation successful. Running test..."
    sudo ./test_led_animation
else
    echo "Compilation failed."
    exit 1
fi 