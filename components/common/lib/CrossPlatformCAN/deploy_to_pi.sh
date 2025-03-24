#!/bin/bash
# Simple deployment script for CrossPlatformCAN to Raspberry Pi

# Configuration
PI_USER="pi"
PI_HOST="192.168.1.154"
PI_DIR="~/go-kart-platform/components/common/lib/CrossPlatformCAN"

# Make sure we're in the right directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Deploying CrossPlatformCAN files to Raspberry Pi..."

# Create source paths
SRC_FILES=(
    "CANInterface.h"
    "CANInterface.cpp"
    "ProtobufCANInterface.h"
    "ProtobufCANInterface.cpp"
    "raspi_build.sh"
    "python/test_can_library.py"
    "python/simple_can_interface.py"
)

# Create target directory structure
echo "Creating target directories..."
ssh $PI_USER@$PI_HOST "mkdir -p $PI_DIR/python $PI_DIR/include $PI_DIR/include/nanopb"

# Copy files
echo "Copying source files..."
for file in "${SRC_FILES[@]}"; do
    echo "  Copying $file..."
    scp "$file" "$PI_USER@$PI_HOST:$PI_DIR/$file"
done

# Copy protocol buffer header files
echo "Copying protocol buffer headers..."
PROTO_SRC="../ProtobufCANInterface/include"
PROTO_FILES=(
    "common.pb.h"
    "lights.pb.h"
    "controls.pb.h"
    "pb.h"
)

for file in "${PROTO_FILES[@]}"; do
    if [ -f "$PROTO_SRC/$file" ]; then
        echo "  Copying $file..."
        scp "$PROTO_SRC/$file" "$PI_USER@$PI_HOST:$PI_DIR/include/$file"
    else
        echo "  WARNING: $PROTO_SRC/$file not found"
    fi
done

# Copy nanopb files
echo "Copying nanopb headers..."
NANOPB_SRC="../ProtobufCANInterface/include/nanopb"
NANOPB_FILES=(
    "pb.h"
    "pb_common.h"
    "pb_encode.h"
    "pb_decode.h"
)

for file in "${NANOPB_FILES[@]}"; do
    if [ -f "$NANOPB_SRC/$file" ]; then
        echo "  Copying nanopb/$file..."
        scp "$NANOPB_SRC/$file" "$PI_USER@$PI_HOST:$PI_DIR/include/nanopb/$file"
    else
        echo "  WARNING: $NANOPB_SRC/$file not found"
    fi
done

# Make scripts executable on the Pi
echo "Making scripts executable..."
ssh $PI_USER@$PI_HOST "chmod +x $PI_DIR/raspi_build.sh $PI_DIR/python/test_can_library.py $PI_DIR/python/simple_can_interface.py"

echo "Deployment complete!"
echo
echo "To build and test on the Raspberry Pi:"
echo "  1. SSH to the Pi: ssh $PI_USER@$PI_HOST"
echo "  2. Navigate to the CrossPlatformCAN directory: cd $PI_DIR"
echo "  3. Build the library: ./raspi_build.sh"
echo "  4. Test the library: python3 python/test_can_library.py"
echo "  5. Run the example: python3 python/simple_can_interface.py" 