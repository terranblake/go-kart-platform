#!/usr/bin/env python3
"""
Direct CAN Animation Test
This script bypasses all libraries and directly formats CAN frames for the LED animation
Based on exact analysis of the Arduino code and CAN frame structure
"""

import time
import os
import struct
import can

# Constants from the Arduino code
MSG_TYPE_COMMAND = 0
COMP_TYPE_LIGHTS = 0
COMP_ID_ALL = 0xFF
CMD_ID_MODE = 0
CMD_ID_ANIMATION_START = 10
CMD_ID_ANIMATION_FRAME = 11
CMD_ID_ANIMATION_END = 12
VAL_TYPE_UINT8 = 2  # seen as 0x20 in byte 4 (shifted left by 4)
MODE_ON = 1
MODE_OFF = 0

# CAN ID used to send to the Arduino
CAN_ID = 0x001
CAN_INTERFACE = 'can0'

def send_command(bus, command_id, value=0):
    """Send a simple command message matching the exact format expected by the Arduino"""
    # Format based on observed CAN frames and Arduino code
    data = bytearray([
        0x00,          # Byte 0: Header
        0x00,          # Byte 1: Reserved
        COMP_ID_ALL,   # Byte 2: Component ID (0xFF)
        command_id,    # Byte 3: Command ID
        0x20,          # Byte 4: Value type (0x20 = VAL_TYPE_UINT8 << 4)
        0x00,          # Byte 5: Value high byte
        0x00,          # Byte 6: Value middle byte
        value & 0xFF   # Byte 7: Value low byte
    ])
    
    message = can.Message(
        arbitration_id=CAN_ID,
        data=data,
        is_extended_id=False
    )
    
    print(f"Sending command: ID={command_id}, value={value}")
    print(f"  Data: {' '.join([f'{b:02X}' for b in data])}")
    bus.send(message)
    time.sleep(0.1)

def send_animation_frame(bus, frame_data):
    """Send an animation frame using the binary data protocol"""
    # Debug output for the frame
    print(f"Sending animation frame: {len(frame_data)} bytes")
    print(f"  Frame data: {' '.join([f'{b:02X}' for b in frame_data])}")
    
    # The Arduino expects binary data to be sent in multiple CAN frames
    # First frame: START flag (0x80) in byte 1, frame header in data
    # Subsequent frames: sequence numbers in byte 1, chunks of data
    # Last frame: END flag (0x40) in byte 1
    
    # Constants based on the Arduino implementation
    FIRST_FRAME_DATA_SIZE = 3  # First frame can hold 3 bytes
    NEXT_FRAME_DATA_SIZE = 3   # Subsequent frames can hold 3 bytes
    
    # Send first frame with START flag
    first_frame = bytearray([
        0x00,          # Byte 0: Header
        0x80,          # Byte 1: Start flag
        COMP_ID_ALL,   # Byte 2: Component ID (0xFF)
        CMD_ID_ANIMATION_FRAME,  # Byte 3: Command ID
        0x25,          # Byte 4: Value type (observed as 0x25)
        # Next 3 bytes will be filled with data
    ])
    
    # Copy first 3 bytes of data
    bytes_to_copy = min(len(frame_data), FIRST_FRAME_DATA_SIZE)
    for i in range(bytes_to_copy):
        first_frame.append(frame_data[i])
        
    # Pad to 8 bytes
    while len(first_frame) < 8:
        first_frame.append(0)
    
    # Send first frame
    message = can.Message(
        arbitration_id=CAN_ID,
        data=first_frame,
        is_extended_id=False
    )
    
    print(f"  First frame (START): {' '.join([f'{b:02X}' for b in first_frame])}")
    bus.send(message)
    time.sleep(0.1)  # Use longer delay between frames
    
    # Send remaining data in chunks
    bytes_sent = bytes_to_copy
    seq_num = 1
    
    while bytes_sent < len(frame_data):
        # Check if this is the last frame
        remaining = len(frame_data) - bytes_sent
        is_last_frame = remaining <= NEXT_FRAME_DATA_SIZE
        
        # Create the frame header
        next_frame = bytearray([
            0x00,         # Byte 0: Header
            0x40 | seq_num if is_last_frame else seq_num,  # Byte 1: End flag (0x40) + seq or just seq
            COMP_ID_ALL,  # Byte 2: Component ID (0xFF)
            CMD_ID_ANIMATION_FRAME,  # Byte 3: Command ID
            min(remaining, NEXT_FRAME_DATA_SIZE)  # Byte 4: Number of bytes in this frame
        ])
        
        # Copy data for this frame
        bytes_this_frame = min(remaining, NEXT_FRAME_DATA_SIZE)
        for i in range(bytes_this_frame):
            next_frame.append(frame_data[bytes_sent + i])
            
        # Pad to 8 bytes
        while len(next_frame) < 8:
            next_frame.append(0)
        
        # Send this frame
        message = can.Message(
            arbitration_id=CAN_ID,
            data=next_frame,
            is_extended_id=False
        )
        
        end_marker = " (END)" if is_last_frame else ""
        print(f"  Frame {seq_num}{end_marker}: {' '.join([f'{b:02X}' for b in next_frame])}")
        bus.send(message)
        time.sleep(0.1)  # Use longer delay between frames
        
        # Update counters
        bytes_sent += bytes_this_frame
        seq_num = (seq_num + 1) & 0x0F  # Keep seq_num in range 0-15
    
    print(f"  Sent {seq_num} frames for animation data")

def create_simple_animation_frame():
    """Create a simple animation frame with basic color test"""
    frame = bytearray()
    frame.append(0)  # Frame index = 0
    frame.append(5)  # 5 LEDs - use more LEDs to make it more visible
    
    # LED 0: BRIGHT RED at position 0
    frame.append(0)      # Position
    frame.append(255)    # R
    frame.append(0)      # G
    frame.append(0)      # B
    
    # LED 1: BRIGHT GREEN at position 5
    frame.append(5)      # Position
    frame.append(0)      # R
    frame.append(255)    # G
    frame.append(0)      # B
    
    # LED 2: BRIGHT BLUE at position 10
    frame.append(10)     # Position
    frame.append(0)      # R
    frame.append(0)      # G
    frame.append(255)    # B
    
    # LED 3: BRIGHT YELLOW at position 15
    frame.append(15)     # Position
    frame.append(255)    # R
    frame.append(255)    # G
    frame.append(0)      # B
    
    # LED 4: BRIGHT MAGENTA at position 20
    frame.append(20)     # Position
    frame.append(255)    # R
    frame.append(0)      # G
    frame.append(255)    # B
    
    return frame

def main():
    print("===== Direct CAN Animation Test =====")
    print("This test will directly format CAN frames to match the Arduino code")
    
    try:
        # Open the CAN interface
        bus = can.interface.Bus(channel=CAN_INTERFACE, interface='socketcan')
        print(f"Connected to CAN interface: {CAN_INTERFACE}")
        
        # Step 1: Reset by turning lights OFF
        print("\nStep 1: Reset lights to OFF state")
        send_command(bus, CMD_ID_MODE, MODE_OFF)
        time.sleep(1)
        
        # Step 2: Turn ON the lights
        print("\nStep 2: Turn ON the lights")
        send_command(bus, CMD_ID_MODE, MODE_ON)
        time.sleep(1)
        
        # Step 3: Start animation sequence
        print("\nStep 3: Start animation sequence")
        send_command(bus, CMD_ID_ANIMATION_START, 0)
        time.sleep(1)
        
        # Step 4: Send animation frame
        print("\nStep 4: Send animation frame")
        frame = create_simple_animation_frame()
        send_animation_frame(bus, frame)
        time.sleep(1)
        
        # Step 5: End animation
        print("\nStep 5: End animation with frame count = 1")
        send_command(bus, CMD_ID_ANIMATION_END, 1)
        
        # Wait for animation to play
        print("\nWaiting 10 seconds for animation to play...")
        time.sleep(10)
        
        # Step 6: Turn OFF the lights
        print("\nStep 6: Turn OFF the lights")
        send_command(bus, CMD_ID_MODE, MODE_OFF)
        
        print("\nTest completed!")
        
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if 'bus' in locals():
            bus.shutdown()

if __name__ == "__main__":
    main() 