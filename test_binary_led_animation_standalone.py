#!/usr/bin/env python3
"""
Test script for sending binary LED animation data over CAN
This sends animation frames to the Arduino lights controller
This is a standalone version that uses Python-CAN directly
Based on actual CAN dumps observed from working implementation
"""

import sys
import time
import os
import math
import can  # python-can library
import struct

# Enum values from the protocol
MSG_TYPE_COMMAND = 0  # kart_common_MessageType_COMMAND
COMP_TYPE_LIGHTS = 0  # kart_common_ComponentType_LIGHTS
COMP_ID_ALL = 0xFF    # kart_lights_LightComponentId_ALL - observed as 0xFF in CAN dump
CMD_ID_MODE = 0       # kart_lights_LightCommandId_MODE
CMD_ID_ANIMATION_START = 10  # kart_lights_LightCommandId_ANIMATION_START (0x0A)
CMD_ID_ANIMATION_FRAME = 11  # kart_lights_LightCommandId_ANIMATION_FRAME (0x0B)
CMD_ID_ANIMATION_END = 12    # kart_lights_LightCommandId_ANIMATION_END (0x0C)
CMD_ID_ANIMATION_STOP = 13   # kart_lights_LightCommandId_ANIMATION_STOP (0x0D)
MODE_OFF = 0          # kart_lights_LightModeValue_OFF
MODE_ON = 1           # kart_lights_LightModeValue_ON

# CAN configuration - based on actual CAN dump
CAN_ID = 0x001       # Observed in CAN dump as 001
CAN_INTERFACE = 'can0'

# Animation configuration
ANIMATION_FRAMES = 3
LEDS_PER_FRAME = 5

def create_rainbow_color(position):
    """
    Generate a rainbow color based on position (0-255)
    Returns RGB tuple with values 0-255
    """
    position = position % 256
    
    if position < 85:
        return (255 - position * 3, position * 3, 0)
    elif position < 170:
        position -= 85
        return (0, 255 - position * 3, position * 3)
    else:
        position -= 170
        return (position * 3, 0, 255 - position * 3)

def create_animation_frames(num_frames=3, leds_per_frame=5):
    """
    Create some sample animation frame data.
    Each frame has the format:
    - 1 byte: frame index
    - 1 byte: number of LEDs
    - For each LED:
      - 1 byte: LED index (position)
      - 3 bytes: RGB color values (optimized format)
      
    The Arduino code is using OPTIMIZE_MEMORY flag which expects:
    - Each LED takes 4 bytes (x, r, g, b) instead of 5 bytes (x, y, r, g, b)
    """
    frames = []
    
    for frame_idx in range(num_frames):
        # Start a new frame with header
        frame_data = bytearray()
        frame_data.append(frame_idx)  # Frame index
        frame_data.append(leds_per_frame)  # Number of LEDs in this frame
        
        # Add LED data
        for led_idx in range(leds_per_frame):
            # Position the LEDs evenly along the strip, shifting by frame
            position = led_idx * 3  # Simple mapping to strip positions 0, 3, 6, 9, 12
            
            # Add LED index (position on the strip)
            frame_data.append(position)
            
            # Add RGB color (create rainbow effect)
            color_position = (led_idx * 30 + frame_idx * 85) % 256
            r, g, b = create_rainbow_color(color_position)
            frame_data.append(r)
            frame_data.append(g)
            frame_data.append(b)
        
        frames.append(frame_data)
    
    return frames

def create_simple_test_animation():
    """
    Create a very simple test animation with a single frame and 5 LEDs with solid colors
    This is for basic testing to verify the LED strip functionality
    """
    # Create a simple frame with 5 LEDs in different colors
    frame_data = bytearray()
    frame_data.append(0)  # Frame index 0
    frame_data.append(5)  # 5 LEDs
    
    # LED 0: Red at position 0
    frame_data.append(0)  # Position 0
    frame_data.append(255)  # R
    frame_data.append(0)    # G
    frame_data.append(0)    # B
    
    # LED 1: Green at position 5
    frame_data.append(5)  # Position 5
    frame_data.append(0)    # R
    frame_data.append(255)  # G
    frame_data.append(0)    # B
    
    # LED 2: Blue at position 10
    frame_data.append(10)  # Position 10
    frame_data.append(0)    # R
    frame_data.append(0)    # G
    frame_data.append(255)  # B
    
    # LED 3: Yellow at position 15
    frame_data.append(15)  # Position 15
    frame_data.append(255)  # R
    frame_data.append(255)  # G
    frame_data.append(0)    # B
    
    # LED 4: Purple at position 20
    frame_data.append(20)  # Position 20
    frame_data.append(255)  # R
    frame_data.append(0)    # G
    frame_data.append(255)  # B
    
    return [frame_data]  # Return as a list with one frame

def send_command(bus, command_id, value=0):
    """
    Send a simple command message - EXACT format based on CAN dump
    """
    # Format based on observed CAN frames:
    # can0  001   [8]  00 00 FF 00 20 00 00 01  (MODE_ON command)
    # can0  001   [8]  00 00 FF 0A 20 00 00 00  (ANIMATION_START command)
    data = bytearray([
        0x00,                   # Byte 0: Header (observed as 0x00)
        0x00,                   # Byte 1: Reserved
        COMP_ID_ALL,            # Byte 2: Component ID (observed as 0xFF)
        command_id,             # Byte 3: Command ID
        0x20,                   # Byte 4: Value type (observed as 0x20)
        0x00,                   # Byte 5: MSB of value
        0x00,                   # Byte 6: Middle byte
        value & 0xFF            # Byte 7: LSB of value
    ])
    
    # Create and send CAN message
    message = can.Message(
        arbitration_id=CAN_ID,  # 0x001 observed in CAN dump
        data=data,
        is_extended_id=False
    )
    
    print(f"Sending command: ID={command_id}, value={value}")
    print(f"  Data: {' '.join([f'{b:02X}' for b in data])}")
    bus.send(message)
    time.sleep(0.1)

def send_binary_data(bus, command_id, binary_data):
    """
    Send binary data using multi-frame format - EXACT format based on CAN dump
    Observed first frame: can0  001   [8]  00 80 FF 0B 25 00 05 00
    """
    print(f"Sending {len(binary_data)} bytes of binary data")
    print(f"Binary data: {' '.join([f'{b:02X}' for b in binary_data])}")
    
    if len(binary_data) >= 2:
        frame_idx = binary_data[0]
        num_leds = binary_data[1]
        print(f"  Frame index: {frame_idx}, Number of LEDs: {num_leds}")
        
        # Debug the LED data - each LED takes 4 bytes in optimized mode
        for i in range(num_leds):
            if 2 + i*4 + 3 < len(binary_data):  # Ensure we have enough data
                led_idx = binary_data[2 + i*4]
                r = binary_data[2 + i*4 + 1]
                g = binary_data[2 + i*4 + 2]
                b = binary_data[2 + i*4 + 3]
                print(f"  LED {i}: Position={led_idx}, Color=RGB({r},{g},{b})")
    
    # Frame format constants based on observed CAN frames
    HEADER_BYTE = 0x00          # Byte 0: Always 0x00 in observed frames
    START_FLAG = 0x80           # Byte 1: Start flag observed as 0x80
    END_FLAG = 0x40             # Byte 1: End flag bit
    COMPONENT_ID = 0xFF         # Byte 2: Component ID (ALL = 0xFF)
    FIRST_FRAME_VALUE_TYPE = 0x25  # Byte 4 for first frame: 0x25 observed
    
    # We need to carefully match the specific frame structure
    # Data constants
    FIRST_FRAME_DATA_SIZE = 3   # First frame can hold 3 bytes of data (bytes 5-7)
    NEXT_FRAME_DATA_SIZE = 3    # Subsequent frames can hold 3 bytes (bytes 5-7)
    
    # Send first frame with START flag format
    # Based on: can0  001   [8]  00 80 FF 0B 25 00 05 00
    first_frame_data = bytearray([
        HEADER_BYTE,            # Byte 0: Header (0x00)
        START_FLAG,             # Byte 1: Start flag (0x80)
        COMPONENT_ID,           # Byte 2: Component ID (0xFF)
        command_id,             # Byte 3: Command ID (0x0B for ANIMATION_FRAME)
        FIRST_FRAME_VALUE_TYPE, # Byte 4: Value type (0x25 observed)
        # Next 3 bytes will be the first 3 bytes of data
    ])
    
    # Add first 3 bytes of data (or less if data is smaller)
    bytes_to_copy = min(len(binary_data), FIRST_FRAME_DATA_SIZE)
    for i in range(bytes_to_copy):
        first_frame_data.append(binary_data[i])
    
    # Pad to 8 bytes
    while len(first_frame_data) < 8:
        first_frame_data.append(0)
    
    # Send first frame
    message = can.Message(
        arbitration_id=CAN_ID,
        data=first_frame_data,
        is_extended_id=False
    )
    
    print(f"Sending START frame: {' '.join([f'{b:02X}' for b in first_frame_data])}")
    bus.send(message)
    time.sleep(0.05)
    
    # Send remaining data in subsequent frames
    bytes_sent = bytes_to_copy
    seq_num = 1  # Start with seq 1 for subsequent frames
    
    while bytes_sent < len(binary_data):
        # Is this the last frame?
        is_last_frame = (bytes_sent + NEXT_FRAME_DATA_SIZE >= len(binary_data))
        
        # Create next frame - we need to match the format from the observed CAN frames
        next_frame = bytearray([
            HEADER_BYTE,                               # Byte 0: Header (0x00)
            (END_FLAG if is_last_frame else 0) | (seq_num & 0x0F),  # Byte 1: End flag or seq num
            COMPONENT_ID,                              # Byte 2: Component ID (0xFF)
            command_id,                                # Byte 3: Command ID
            min(len(binary_data) - bytes_sent, NEXT_FRAME_DATA_SIZE)  # Byte 4: Size of data in this frame
        ])
        
        # Add data bytes to this frame
        frame_data_size = min(len(binary_data) - bytes_sent, NEXT_FRAME_DATA_SIZE)
        for i in range(frame_data_size):
            next_frame.append(binary_data[bytes_sent + i])
        
        # Pad to 8 bytes
        while len(next_frame) < 8:
            next_frame.append(0)
        
        # Send frame
        message = can.Message(
            arbitration_id=CAN_ID,
            data=next_frame,
            is_extended_id=False
        )
        
        flag_str = " (END)" if is_last_frame else ""
        print(f"Sending frame {seq_num}{flag_str}: {' '.join([f'{b:02X}' for b in next_frame])}")
        bus.send(message)
        time.sleep(0.05)
        
        # Update counters
        bytes_sent += frame_data_size
        seq_num = (seq_num + 1) & 0x0F  # Keep sequence in 0-15 range
    
    print(f"Binary data sent successfully in {seq_num} frames")

def send_animation(bus, use_simple_animation=True):
    """Send an animation sequence over CAN"""
    if use_simple_animation:
        print("Creating simple test animation with solid colors...")
        frames = create_simple_test_animation()
    else:
        print("Creating rainbow animation frames...")
        frames = create_animation_frames(ANIMATION_FRAMES, LEDS_PER_FRAME)
    
    print(f"Sending animation with {len(frames)} frames")
    
    # Step 1: Send ANIMATION_START command
    print("Sending ANIMATION_START command...")
    send_command(bus, CMD_ID_ANIMATION_START, 0)
    time.sleep(0.5)
    
    # Step 2: Send each frame as binary data
    for i, frame_data in enumerate(frames):
        print(f"Sending frame {i+1}/{len(frames)}...")
        send_binary_data(bus, CMD_ID_ANIMATION_FRAME, frame_data)
        time.sleep(0.5)
    
    # Step 3: Send ANIMATION_END command with frame count
    print("Sending ANIMATION_END command...")
    send_command(bus, CMD_ID_ANIMATION_END, len(frames))
    
    print("Animation sequence sent!")

def main():
    print("Binary LED Animation Test Script")
    print("Using exact CAN frame format based on observed CAN dump")
    print("SIMPLIFIED MODE: Testing with basic solid colors")
    
    # Configure and open CAN interface
    try:
        bus = can.interface.Bus(channel=CAN_INTERFACE, interface='socketcan')
        print(f"Connected to CAN interface: {CAN_INTERFACE}")
    except Exception as e:
        print(f"Error connecting to CAN interface: {e}")
        return
    
    try:
        # Clear any existing state and turn lights on
        print("Sending MODE=OFF command to reset state...")
        send_command(bus, CMD_ID_MODE, MODE_OFF)
        time.sleep(0.5)
        
        print("Sending MODE=ON command...")
        send_command(bus, CMD_ID_MODE, MODE_ON)
        time.sleep(1)
        
        # Explicitly send animation mode command if needed
        # The Arduino might require this before accepting animation frames
        print("Explicitly activating animation mode...")
        animation_mode_cmd = 0x09  # Animation mode command (if it exists in protocol)
        animation_mode_value = 1    # Value 1 to enable animations
        send_command(bus, animation_mode_cmd, animation_mode_value)
        time.sleep(0.5)
        
        # Send animation sequence - simplified version with solid colors
        send_animation(bus, use_simple_animation=True)
        
        # Wait a bit to see the animation run
        print("Waiting for animation to complete...")
        time.sleep(5)
        
        # Turn off the lights
        print("Sending MODE=OFF command...")
        send_command(bus, CMD_ID_MODE, MODE_OFF)
        
        print("Test completed successfully!")
        
    except KeyboardInterrupt:
        print("Test interrupted by user.")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        bus.shutdown()
        print("CAN interface closed")

if __name__ == "__main__":
    main() 