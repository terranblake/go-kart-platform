#!/usr/bin/env python3
"""
Test script for CrossPlatformCAN Python interface

This script demonstrates how to use the Python CFFI interface to communicate
with the CAN bus using the CrossPlatformCAN library.

Usage:
    python3 test_can_interface.py [--node-id=ID] [--device=can0]
"""

import os
import sys
import time
import argparse
from can_interface import CANInterface, MessageType, ComponentType, ValueType

def message_handler(msg_type, comp_type, component_id, command_id, value_type, value):
    """Handler for incoming CAN messages"""
    
    # Get string representations for easier printing
    msg_type_str = "COMMAND" if msg_type == MessageType.COMMAND else "STATUS"
    comp_type_str = "LIGHTS" if comp_type == ComponentType.LIGHTS else "CONTROLS" if comp_type == ComponentType.CONTROLS else str(comp_type)
    value_type_str = "BOOLEAN" if value_type == ValueType.BOOLEAN else "INT" if value_type in (ValueType.INT8, ValueType.INT16, ValueType.INT24) else "UINT"
    
    print(f"Received: {msg_type_str} for {comp_type_str}[{component_id}], CMD={command_id}, {value_type_str}={value}")
    
    # Respond to specific commands if needed
    # Example: If we receive a light location command, respond with a status
    if comp_type == ComponentType.LIGHTS and command_id == 3:
        print(f"Light location set to: {value}")

def main():
    """Main test function"""
    parser = argparse.ArgumentParser(description='Test the CrossPlatformCAN Python interface')
    parser.add_argument('--node-id', type=int, default=0x01, help='CAN node ID')
    parser.add_argument('--device', default='can0', help='CAN device name')
    parser.add_argument('--lib-path', help='Path to the CrossPlatformCAN library')
    args = parser.parse_args()
    
    try:
        # Create CAN interface
        print(f"Creating CAN interface with node ID 0x{args.node_id:X} on device {args.device}")
        can = CANInterface(args.node_id, args.lib_path)
        
        # Initialize CAN interface
        if not can.begin(500000, args.device):
            print("Failed to initialize CAN interface")
            return 1
        print("CAN interface initialized successfully")
        
        # Register message handlers
        can.register_handler(
            ComponentType.LIGHTS, 
            0x01,  # Component ID
            0x03,  # Command ID for location
            message_handler
        )
        
        can.register_handler(
            ComponentType.CONTROLS, 
            0x01,  # Component ID
            0x07,  # Command ID for diagnostic mode
            message_handler
        )
        
        # Start automatic message processing
        print("Starting message processing...")
        can.start_processing()
        
        # Test 1: Set lights location
        print("\n--- Test 1: Setting Lights Location ---")
        locations = {0: "FRONT", 1: "REAR", 2: "LEFT", 3: "RIGHT", 4: "CENTER"}
        
        for loc_value, loc_name in locations.items():
            print(f"\nSetting location to {loc_name}")
            success = can.set_lights_location(loc_value)
            print(f"Send {'succeeded' if success else 'failed'}")
            time.sleep(0.5)  # Wait for response
        
        # Test 2: Set controls diagnostic mode
        print("\n--- Test 2: Controls Diagnostic Mode ---")
        print("\nEnabling TEST mode")
        success = can.set_controls_diagnostic_mode(True)
        print(f"Send {'succeeded' if success else 'failed'}")
        time.sleep(0.5)
        
        print("\nDisabling TEST mode")
        success = can.set_controls_diagnostic_mode(False)
        print(f"Send {'succeeded' if success else 'failed'}")
        time.sleep(0.5)
        
        # Keep running to process incoming messages
        print("\nRunning message loop. Press Ctrl+C to exit...")
        while True:
            time.sleep(1)
            
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"Error: {e}")
        return 1
    finally:
        if 'can' in locals():
            can.stop_processing()
    
    print("Test completed")
    return 0

if __name__ == "__main__":
    sys.exit(main()) 