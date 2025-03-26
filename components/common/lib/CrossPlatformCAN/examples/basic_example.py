#!/usr/bin/env python3
"""
Basic example of using the CrossPlatformCAN Python interface.

This example demonstrates:
1. Creating a CAN interface
2. Registering a message handler
3. Sending various message types
4. Automatic message processing

Run this script as:
    python3 basic_example.py
"""

import sys
import time
import os

# Add the parent directory to the Python path to find the module
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from python.can_interface import CANInterface, MessageType, ComponentType, ValueType

def message_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
    """Handler for received CAN messages"""
    # Get message type name
    msg_type_name = "UNKNOWN"
    if msg_type == MessageType.COMMAND:
        msg_type_name = "COMMAND"
    elif msg_type == MessageType.STATUS:
        msg_type_name = "STATUS"
    
    # Get component type name
    comp_type_name = "UNKNOWN"
    if comp_type == ComponentType.LIGHTS:
        comp_type_name = "LIGHTS"
    elif comp_type == ComponentType.CONTROLS:
        comp_type_name = "CONTROLS"
    
    print(f"Received message: {msg_type_name}({msg_type}), {comp_type_name}({comp_type}), "
          f"Component ID: {comp_id}, Command ID: {cmd_id}, Value Type: {val_type}, Value: {value}")

def main():
    """Main function"""
    # Create a CAN interface with node ID 1
    can = CANInterface(node_id=0x01)
    
    # Initialize CAN interface
    if not can.begin(baudrate=500000, device="can0"):
        print("Failed to initialize CAN interface!")
        return 1
    
    print("CAN interface initialized successfully.")
    
    # Register handler for lights commands
    can.register_handler(
        MessageType.COMMAND,     # Message type
        ComponentType.LIGHTS,    # Component type
        0x01,                    # Component ID
        0x01,                    # Command ID
        message_handler        # Handler function
    )
    
    # Start automatic message processing in a background thread
    can.start_processing()
    print("Started automatic message processing.")
    
    try:
        # Send a command to the lights component
        print("\nSending COMMAND to LIGHTS component...")
        success = can.send_message(
            MessageType.COMMAND,     # Message type
            ComponentType.LIGHTS,    # Component type
            0x01,                    # Component ID
            0x01,                    # Command ID (e.g., toggle)
            ValueType.BOOLEAN,       # Value type
            1                        # Value (on)
        )
        print(f"Message sent successfully: {success}")
        
        # Wait for a moment
        time.sleep(1)
        
        # Send a command to the controls component
        print("\nSending COMMAND to CONTROLS component...")
        success = can.send_message(
            MessageType.COMMAND,     # Message type
            ComponentType.CONTROLS,  # Component type
            0x01,                    # Component ID
            0x03,                    # Command ID (e.g., diagnostic)
            ValueType.UINT8,         # Value type
            6                        # Value
        )
        print(f"Message sent successfully: {success}")
        
        # Keep running for a while to receive messages
        print("\nListening for messages for 10 seconds...")
        time.sleep(10)
        
    except KeyboardInterrupt:
        print("\nExiting due to keyboard interrupt.")
    finally:
        # Stop message processing
        can.stop_processing()
        print("Stopped automatic message processing.")
    
    return 0

if __name__ == "__main__":
    sys.exit(main()) 