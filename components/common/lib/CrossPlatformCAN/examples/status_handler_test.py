#!/usr/bin/env python3
"""
Test for STATUS message handlers in the CrossPlatformCAN Python interface.

This example demonstrates:
1. Creating a CAN interface
2. Registering handlers for both COMMAND and STATUS message types
3. Sending both types of messages
4. Verifying that both handlers are called appropriately

Run this script as:
    python3 status_handler_test.py
"""

import sys
import time
import os

# Add the parent directory to the Python path to find the module
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from python.can_interface import CANInterface, MessageType, ComponentType, ValueType

# Track which handlers were called
command_handler_called = False
status_handler_called = False

def command_message_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
    """Handler for COMMAND messages"""
    global command_handler_called
    print(f"COMMAND handler called: {msg_type}, {comp_type}, {comp_id}, {cmd_id}, {val_type}, {value}")
    assert msg_type == MessageType.COMMAND, "Expected COMMAND message type in command handler"
    command_handler_called = True

def status_message_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
    """Handler for STATUS messages"""
    global status_handler_called
    print(f"STATUS handler called: {msg_type}, {comp_type}, {comp_id}, {cmd_id}, {val_type}, {value}")
    assert msg_type == MessageType.STATUS, "Expected STATUS message type in status handler"
    status_handler_called = True

def main():
    """Main function"""
    # Create a CAN interface with node ID 1
    can = CANInterface(node_id=0x01)
    
    # Initialize CAN interface
    if not can.begin(baudrate=500000, device="can0"):
        print("Failed to initialize CAN interface!")
        return 1
    
    print("CAN interface initialized successfully.")
    
    # Register handler for COMMAND messages
    can.register_handler(
        MessageType.COMMAND,     # Message type
        ComponentType.LIGHTS,    # Component type
        0x01,                    # Component ID
        0x01,                    # Command ID
        command_message_handler  # Handler function
    )
    
    # Register handler for STATUS messages
    can.register_handler(
        MessageType.STATUS,      # Message type
        ComponentType.LIGHTS,    # Component type
        0x01,                    # Component ID
        0x01,                    # Command ID
        status_message_handler   # Handler function
    )
    
    # Start automatic message processing in a background thread
    can.start_processing()
    print("Started automatic message processing.")
    
    try:
        # Send a COMMAND message to the lights component
        print("\nSending COMMAND message to LIGHTS component...")
        success = can.send_message(
            MessageType.COMMAND,     # Message type
            ComponentType.LIGHTS,    # Component type
            0x01,                    # Component ID
            0x01,                    # Command ID
            ValueType.BOOLEAN,       # Value type
            1                        # Value (on)
        )
        print(f"COMMAND message sent successfully: {success}")
        
        # Wait for a moment
        time.sleep(1)
        
        # Send a STATUS message to the lights component
        print("\nSending STATUS message to LIGHTS component...")
        success = can.send_message(
            MessageType.STATUS,      # Message type
            ComponentType.LIGHTS,    # Component type
            0x01,                    # Component ID
            0x01,                    # Command ID
            ValueType.BOOLEAN,       # Value type
            1                        # Value (on)
        )
        print(f"STATUS message sent successfully: {success}")
        
        # Keep running for a while to receive messages
        print("\nListening for messages for 3 seconds...")
        time.sleep(3)
        
        # Check if both handlers were called
        print("\nTest results:")
        print(f"COMMAND handler called: {command_handler_called}")
        print(f"STATUS handler called: {status_handler_called}")
        
        if command_handler_called and status_handler_called:
            print("TEST PASSED: Both COMMAND and STATUS handlers were called.")
        else:
            print("TEST FAILED: Not all handlers were called.")
            return 1
            
    except KeyboardInterrupt:
        print("\nExiting due to keyboard interrupt.")
    finally:
        # Stop message processing
        can.stop_processing()
        print("Stopped automatic message processing.")
    
    return 0

if __name__ == "__main__":
    sys.exit(main()) 