#!/usr/bin/env python3
"""
Test script for sending binary LED animation data over CAN
This sends animation frames to the Arduino lights controller
"""

import sys
import time
import os
import random
from dashboard.server.lib.can.interface import CANInterfaceWrapper
from dashboard.server.lib.can.protocol_registry import ProtocolRegistry

# Ensure we're in the right directory
project_root = os.path.abspath(os.path.join(os.path.dirname(__file__)))
os.chdir(project_root)

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

def send_animation(can_interface):
    """Send an animation sequence over CAN"""
    print("Creating animation frames...")
    frames = create_animation_frames(3, 5)  # 3 frames, 5 LEDs per frame
    
    print(f"Sending animation with {len(frames)} frames")
    
    # Step 1: Send ANIMATION_START command
    print("Sending ANIMATION_START command...")
    can_interface.send_command(
        component_type_name="LIGHTS",
        component_name="ALL",
        command_name="ANIMATION_START",
        value_name="ON"
    )
    time.sleep(0.1)
    
    # Step 2: Send each frame as binary data
    for i, frame_data in enumerate(frames):
        print(f"Sending frame {i} ({len(frame_data)} bytes)...")
        can_interface.send_binary_data(
            component_type_name="LIGHTS",
            component_name="ALL",
            command_name="ANIMATION_FRAME",
            value_type="UINT8",
            data=frame_data
        )
        time.sleep(0.1)  # Give the Arduino time to process
    
    # Step 3: Send ANIMATION_END command with frame count
    print("Sending ANIMATION_END command...")
    can_interface.send_command(
        component_type_name="LIGHTS",
        component_name="ALL",
        command_name="ANIMATION_END",
        direct_value=len(frames)
    )
    
    print("Animation sequence sent!")

def main():
    # Initialize CAN interface
    print("Initializing CAN interface...")
    can_interface = CANInterfaceWrapper(
        node_id=0x01,  # Server node ID
        channel='can0',
        baudrate=500000
    )
    
    # Register a message handler to see responses
    def message_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
        print(f"Received message: {msg_type}, {comp_type}, {comp_id}, {cmd_id}, {val_type}, {value}")
    
    # Start automated message processing
    can_interface.start_processing()
    
    try:
        # Test 1: Basic light command
        print("Sending basic light command...")
        can_interface.send_command(
            component_type_name="LIGHTS",
            component_name="ALL", 
            command_name="MODE",
            value_name="ON"
        )
        time.sleep(1)
        
        # Test 2: Send animation sequence
        send_animation(can_interface)
        
        # Wait a bit to see the animation run
        print("Waiting for animation to complete...")
        time.sleep(5)
        
        # Test 3: Turn lights off
        print("Turning lights off...")
        can_interface.send_command(
            component_type_name="LIGHTS",
            component_name="ALL", 
            command_name="MODE",
            value_name="OFF"
        )
        
        print("Test completed successfully!")
        
    except KeyboardInterrupt:
        print("Test interrupted by user.")
    finally:
        # Stop message processing
        can_interface.stop_processing()

if __name__ == "__main__":
    # Import math here to avoid polluting the global namespace
    import math
    main() 