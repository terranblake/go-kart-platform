#!/bin/bash

# prepare_includes.sh
# Script to prepare include files for building the ProtobufCANInterface
# This script should be run from the go-kart-platform root directory

# Create include directories if they don't exist
mkdir -p components/common/lib/ProtobufCANInterface/include
mkdir -p components/common/lib/ProtobufCANInterface/include/nanopb

# Copy nanopb headers
echo "Copying nanopb headers..."
cp protocol/nanopb/pb.h components/common/lib/ProtobufCANInterface/include/
cp protocol/nanopb/*.h components/common/lib/ProtobufCANInterface/include/nanopb/

# Copy protocol buffer headers
echo "Copying protocol buffer headers..."
cp protocol/generated/nanopb/*.pb.h components/common/lib/ProtobufCANInterface/include/

echo "Done. Include files are ready for compilation."
echo "When compiling, add '-I components/common/lib/ProtobufCANInterface/include' to your compile flags." 