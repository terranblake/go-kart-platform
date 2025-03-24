#!/bin/bash
# Run tests on Raspberry Pi
# This script sets up the necessary environment and runs the tests on a Raspberry Pi

set -e  # Exit on error

# Check if we're running on a Raspberry Pi
if ! grep -q "Raspberry Pi\|BCM" /proc/cpuinfo; then
    echo "Error: This script must be run on a Raspberry Pi"
    exit 1
fi

# Check if we have sudo access
if ! sudo -v; then
    echo "Error: This script requires sudo access to configure the CAN interface"
    exit 1
fi

# Check if the CAN interface exists
if ! ip link show can0 &>/dev/null; then
    echo "Warning: CAN interface 'can0' not found"
    echo "Attempting to create and configure CAN interface..."
    
    # Try to load the CAN modules
    sudo modprobe can
    sudo modprobe can_raw
    sudo modprobe can_dev
    
    # Try to create the CAN interface (assumes MCP2515 on SPI0.0)
    # This may fail if the hardware is different - adjust as needed
    sudo ip link add dev can0 type can bitrate 500000
    
    # Check if it worked
    if ! ip link show can0 &>/dev/null; then
        echo "Error: Failed to create CAN interface. Check your hardware configuration."
        echo "You may need to add 'dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=25' to /boot/config.txt"
        exit 1
    fi
fi

# Show CAN interface status
echo "CAN interface status:"
ip -details link show can0

# Make sure the libcaninterface.so exists
if [ ! -f "lib/can/libcaninterface.so" ]; then
    echo "Error: libcaninterface.so not found in lib/can/"
    echo "You need to build the CrossPlatformCAN library first"
    exit 1
fi

# Check if the protocol files exist
if [ ! -d "../protocol/generated/python" ]; then
    echo "Warning: Protocol files not found in expected location ../protocol/generated/python"
    echo "Tests will use the default path from the protocol registry"
fi

# Run the protocol registry tests
echo ""
echo "===== Running Protocol Registry Tests ====="
python3 -m tests.test_protocol_registry

# Run the CAN interface tests
echo ""
echo "===== Running CAN Interface Tests ====="
python3 -m tests.test_can_interface

echo ""
echo "Tests completed." 