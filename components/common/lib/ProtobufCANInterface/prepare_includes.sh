#!/bin/bash

# prepare_includes.sh
# Script to prepare include files for building the ProtobufCANInterface
# This script should be run from the go-kart-platform root directory

# Create include directories if they don't exist
mkdir -p components/common/lib/ProtobufCANInterface/include
mkdir -p components/common/lib/ProtobufCANInterface/include/nanopb
mkdir -p components/common/lib/ProtobufCANInterface/include/arduino

# Copy nanopb headers
echo "Copying nanopb headers..."
cp protocol/nanopb/pb.h components/common/lib/ProtobufCANInterface/include/
cp protocol/nanopb/*.h components/common/lib/ProtobufCANInterface/include/nanopb/

echo "Copying CAN headers..."
# ask user for the path to the CAN.h file
read -p "Enter the path to the CAN.h file: " CAN_H_FILE
cp $CAN_H_FILE/*.h components/common/lib/ProtobufCANInterface/include/CAN/

# Download SPI.h if it doesn't exist
if [ ! -f components/common/lib/ProtobufCANInterface/include/arduino/SPI.h ]; then
    echo "SPI.h not found, downloading..."
    curl -o components/common/lib/ProtobufCANInterface/include/arduino/SPI.h https://raw.githubusercontent.com/arduino/ArduinoCore-avr/master/libraries/SPI/src/SPI.h
fi

# Copy protocol buffer headers
echo "Copying protocol buffer headers..."
cp protocol/generated/nanopb/*.pb.h components/common/lib/ProtobufCANInterface/include/

echo "Done. Include files are ready for compilation."
echo "When compiling, add '-I components/common/lib/ProtobufCANInterface/include' to your compile flags." 