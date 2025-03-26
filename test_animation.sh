#!/bin/bash
# Script to compile and run the LED animation test

echo "==== LED Animation Test ===="
echo "Compiling test program..."
make -f Makefile.test clean
make -f Makefile.test

if [ $? -eq 0 ]; then
    echo "Compilation successful. Running test..."
    sudo ./test_led_animation
else
    echo "Compilation failed."
    exit 1
fi 