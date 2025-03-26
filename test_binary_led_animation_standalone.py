#!/usr/bin/env python3
"""
Test script for sending binary LED animation data over CAN
This sends animation frames to the Arduino lights controller
This is a standalone version that uses Python-CAN directly
"""

import sys
import time
import os
import math
import can  # python-can library

# Enum values from the protocol
MSG_TYPE_COMMAND = 0  # kart_common_MessageType_COMMAND
COMP_TYPE_LIGHTS = 0  # kart_common_ComponentType_LIGHTS
COMP_ID_ALL = 255     # kart_lights_LightComponentId_ALL
CMD_ID_MODE = 0       # kart_lights_LightCommandId_MODE
CMD_ID_ANIMATION_START = 10  # kart_lights_LightCommandId_ANIMATION_START
CMD_ID_ANIMATION_FRAME = 11  # kart_lights_LightCommandId_ANIMATION_FRAME
CMD_ID_ANIMATION_END = 12    # kart_lights_LightCommandId_ANIMATION_END
CMD_ID_ANIMATION_STOP = 13   # kart_lights_LightCommandId_ANIMATION_STOP
VAL_TYPE_BOOLEAN = 0  # kart_common_ValueType_BOOLEAN
VAL_TYPE_UINT8 = 2    # kart_common_ValueType_UINT8
MODE_OFF = 0          # kart_lights_LightModeValue_OFF
MODE_ON = 1           # kart_lights_LightModeValue_ON

def pack_header(msg_type, comp_type):
    """Pack message type and component type into a header byte"""
    return ((msg_type & 0x03) << 6) | ((comp_type & 0x07) << 3)

def pack_value(value_type, value):
    """Pack a value based on its type"""
    if value_type == VAL_TYPE_BOOLEAN:
        return 1 if value else 0
    return value & 0xFFFFFF  # 24 bits max

def create_animation_frames(num_frames=3, leds_per_frame=5):
    """
    Create some sample animation frame data.
    Each frame has the format:
    - 1 byte: frame index
    - 1 byte: number of LEDs
    - For each LED:
      - 1 byte: LED index (position)
      - 3 bytes: RGB color values
    """
    frames = []
    
    for frame_idx in range(num_frames):
        # Start with frame index and LED count
        frame_data = bytearray([frame_idx, leds_per_frame])
        
        for led_idx in range(leds_per_frame):
            # Add LED position (distributed across the strip)
            position = int(led_idx * (60 / leds_per_frame))
            frame_data.append(position)
            
            # Add random RGB values for this LED in this frame
            # Create a moving rainbow effect
            r = int(128 + 127 * math.sin(frame_idx/3.0 + led_idx/5.0))
            g = int(128 + 127 * math.sin(frame_idx/3.0 + led_idx/5.0 + 2))
            b = int(128 + 127 * math.sin(frame_idx/3.0 + led_idx/5.0 + 4))
            frame_data.extend([r, g, b])
        
        frames.append(bytes(frame_data))
    
    return frames

def send_message(bus, msg_type, comp_type, comp_id, cmd_id, value_type, value):
    """Send a CAN message with standard format"""
    # Pack the header byte
    header = pack_header(msg_type, comp_type)
    
    # Pack the value
    packed_value = pack_value(value_type, value)
    
    # Create the 8-byte message data
    data = [
        header,                    # Header byte
        0,                         # Reserved
        comp_id,                   # Component ID
        cmd_id,                    # Command ID
        (value_type & 0x0F) << 4,  # Value type in high nibble
        (packed_value >> 16) & 0xFF,  # Value byte 2 (MSB)
        (packed_value >> 8) & 0xFF,   # Value byte 1
        packed_value & 0xFF           # Value byte 0 (LSB)
    ]
    
    # Create and send the CAN message
    message = can.Message(
        arbitration_id=0x123,  # Our node ID
        data=data,
        is_extended_id=False
    )
    bus.send(message)
    print(f"Sent message: {list(message.data)}")
    return True

def send_binary_data(bus, msg_type, comp_type, comp_id, cmd_id, value_type, data):
    """Send binary data over CAN using multiple frames"""
    if len(data) > 1024:
        print(f"Binary data too large ({len(data)} > 1024 bytes)")
        return False
    
    # Pack the header byte
    header = pack_header(msg_type, comp_type)
    
    # Constants for binary data framing
    FIRST_FRAME_DATA_SIZE = 3
    NEXT_FRAME_DATA_SIZE = 3
    
    # Calculate total frames needed
    total_frames = 1
    remaining_data = len(data)
    
    if remaining_data > FIRST_FRAME_DATA_SIZE:
        remaining_data -= FIRST_FRAME_DATA_SIZE
        total_frames += (remaining_data + NEXT_FRAME_DATA_SIZE - 1) // NEXT_FRAME_DATA_SIZE
    
    print(f"Sending {len(data)} bytes in {total_frames} frames")
    
    # Send first frame
    first_frame_size = min(len(data), FIRST_FRAME_DATA_SIZE)
    first_frame = [
        header,                         # Header byte
        0x80,                           # Start flag in bit 7, seq 0
        comp_id,                        # Component ID
        cmd_id,                         # Command ID
        ((value_type & 0x0F) << 4) | (min(total_frames, 15)), # Value type + frames
        *(data[:first_frame_size]),     # Up to 3 bytes of data
        *(0 for _ in range(first_frame_size, FIRST_FRAME_DATA_SIZE)) # Zero padding
    ]
    
    # Create and send the first frame
    message = can.Message(
        arbitration_id=0x123,  # Our node ID
        data=first_frame,
        is_extended_id=False
    )
    bus.send(message)
    print(f"Sent binary frame 0: {list(message.data)}")
    
    # Send subsequent frames
    bytes_sent = first_frame_size
    seq_num = 1
    
    while bytes_sent < len(data):
        # Calculate frame size for this frame
        frame_size = min(len(data) - bytes_sent, NEXT_FRAME_DATA_SIZE)
        
        # Prepare flags/sequence byte
        flags_seq = seq_num % 16
        if bytes_sent + frame_size >= len(data):
            flags_seq |= 0x40  # End flag in bit 6
        
        # Prepare frame
        frame = [
            header,                       # Header byte
            flags_seq,                    # Flags and sequence
            comp_id,                      # Component ID
            cmd_id,                       # Command ID
            frame_size & 0xFF,            # Frame size
            *(data[bytes_sent:bytes_sent+frame_size]),  # Data bytes
            *(0 for _ in range(frame_size, NEXT_FRAME_DATA_SIZE))  # Zero padding
        ]
        
        # Create and send the frame
        message = can.Message(
            arbitration_id=0x123,  # Our node ID
            data=frame,
            is_extended_id=False
        )
        bus.send(message)
        print(f"Sent binary frame {seq_num}: {list(message.data)}")
        
        bytes_sent += frame_size
        seq_num += 1
        
        # Don't flood the bus
        time.sleep(0.01)
    
    print(f"Successfully sent {len(data)} bytes in {seq_num} frames")
    return True

def send_animation(bus):
    """Send an animation sequence over CAN"""
    print("Creating animation frames...")
    frames = create_animation_frames(3, 5)  # 3 frames, 5 LEDs per frame
    
    print(f"Sending animation with {len(frames)} frames")
    
    # Step 1: Send ANIMATION_START command
    print("Sending ANIMATION_START command...")
    send_message(
        bus,
        MSG_TYPE_COMMAND,
        COMP_TYPE_LIGHTS,
        COMP_ID_ALL,
        CMD_ID_ANIMATION_START,
        VAL_TYPE_BOOLEAN,
        True
    )
    time.sleep(0.1)
    
    # Step 2: Send each frame as binary data
    for i, frame_data in enumerate(frames):
        print(f"Sending frame {i} ({len(frame_data)} bytes)...")
        send_binary_data(
            bus,
            MSG_TYPE_COMMAND,
            COMP_TYPE_LIGHTS,
            COMP_ID_ALL,
            CMD_ID_ANIMATION_FRAME,
            VAL_TYPE_UINT8,
            frame_data
        )
        time.sleep(0.1)  # Give the Arduino time to process
    
    # Step 3: Send ANIMATION_END command with frame count
    print("Sending ANIMATION_END command...")
    send_message(
        bus,
        MSG_TYPE_COMMAND,
        COMP_TYPE_LIGHTS,
        COMP_ID_ALL,
        CMD_ID_ANIMATION_END,
        VAL_TYPE_UINT8,
        len(frames)
    )
    
    print("Animation sequence sent!")

def main():
    # Initialize CAN interface
    print("Initializing CAN interface...")
    try:
        bus = can.interface.Bus(channel='can0', bustype='socketcan')
    except Exception as e:
        print(f"Error initializing CAN bus: {e}")
        return
    
    try:
        # Test 1: Basic light command
        print("Sending basic light command...")
        send_message(
            bus,
            MSG_TYPE_COMMAND,
            COMP_TYPE_LIGHTS,
            COMP_ID_ALL,
            CMD_ID_MODE,
            VAL_TYPE_UINT8,
            MODE_ON
        )
        time.sleep(1)
        
        # Test 2: Send animation sequence
        send_animation(bus)
        
        # Wait a bit to see the animation run
        print("Waiting for animation to complete...")
        time.sleep(5)
        
        # Test 3: Turn lights off
        print("Turning lights off...")
        send_message(
            bus,
            MSG_TYPE_COMMAND,
            COMP_TYPE_LIGHTS,
            COMP_ID_ALL,
            CMD_ID_MODE,
            VAL_TYPE_UINT8,
            MODE_OFF
        )
        
        print("Test completed successfully!")
        
    except KeyboardInterrupt:
        print("Test interrupted by user.")
    finally:
        bus.shutdown()

if __name__ == "__main__":
    main() 