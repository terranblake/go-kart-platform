#!/bin/bash
# Simple script to compile and run the header test

set -e  # Exit on any error

echo "Compiling header_test.cpp..."

g++ -o header_test header_test.cpp ../ProtobufCANInterface.cpp ../CANInterface.cpp \
    -I.. -I../include -DPLATFORM_LINUX -std=c++11

echo "Running header_test..."
./header_test

echo "Test completed." 