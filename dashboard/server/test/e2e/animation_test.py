#!/usr/bin/env python3
"""
End-to-end test for animation transmission to Arduino
This test sends animations to the Arduino-based lights controller
"""

import sys
import time
import os
from pathlib import Path
import json
import logging
import argparse

# Configure logging
logging.basicConfig(level=logging.INFO, 
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Add the server directory to the path so we can import the modules
server_dir = Path(__file__).parent.parent.parent
sys.path.insert(0, str(server_dir))

# Import necessary modules
from lib.can.interface import CANInterfaceWrapper
from lib.can.protocol_registry import ProtocolRegistry

# Test configuration
NODE_ID = 0x01
CHANNEL = 'can0'  # CAN interface on Linux/RPi
BAUDRATE = 500000  # 500kbps
# Update the animation directory to include both possible locations
ANIMATION_DIR = os.path.join(server_dir, '../..', 'tools', 'animations')
API_ANIMATIONS_DIR = os.path.join(server_dir, 'api', 'animations')

def load_animation(animation_id):
    """
    Load an animation from the animations directory
    """
    # First check the API animations directory (where we created our animation)
    api_animation_path = os.path.join(API_ANIMATIONS_DIR, animation_id)
    if os.path.exists(api_animation_path):
        metadata_file = os.path.join(api_animation_path, 'metadata.json')
        if os.path.exists(metadata_file):
            try:
                with open(metadata_file, 'r') as f:
                    metadata = json.load(f)
                
                # Load the frames
                frames_dir = os.path.join(api_animation_path, 'frames')
                if os.path.exists(frames_dir):
                    frames = []
                    frame_files = sorted([f for f in os.listdir(frames_dir) if f.startswith('frame') and f.endswith('.json')])
                    for frame_file in frame_files:
                        with open(os.path.join(frames_dir, frame_file), 'r') as f:
                            frame_data = json.load(f)
                            frames.append(frame_data)
                    
                    metadata['frames'] = frames
                    return metadata
            except Exception as e:
                logger.warning(f"Failed to load animation from {api_animation_path}: {e}")
    
    # Check for car animations directory (original code)
    car_animations_path = os.path.join(ANIMATION_DIR, 'car')
    if os.path.exists(car_animations_path):
        car_animations_file = os.path.join(car_animations_path, 'animations.json')
        if os.path.exists(car_animations_file):
            with open(car_animations_file, 'r') as f:
                car_animations = json.load(f)
                for anim in car_animations:
                    if anim.get('id') == animation_id:
                        return anim
    
    # If not found in car animations, look in other directories
    for root, dirs, files in os.walk(ANIMATION_DIR):
        for file in files:
            if file.endswith('.json'):
                filepath = os.path.join(root, file)
                try:
                    with open(filepath, 'r') as f:
                        animation = json.load(f)
                        if isinstance(animation, dict) and animation.get('id') == animation_id:
                            return animation
                        elif isinstance(animation, list):
                            for anim in animation:
                                if anim.get('id') == animation_id:
                                    return anim
                except Exception as e:
                    logger.warning(f"Failed to load animation from {filepath}: {e}")
    
    logger.error(f"Animation {animation_id} not found")
    return None

def prepare_animation_frame(frame, frame_index):
    """
    Convert an animation frame to binary data for transmission
    """
    # Modified to handle both our frame format and the original format
    leds = []
    
    if 'leds' in frame:
        leds = frame.get('leds', [])
    elif 'frameIndex' in frame:
        # This is our format where the frame itself has frameIndex and leds
        leds = frame.get('leds', [])
    
    # Create binary packet for this frame
    packet = bytearray([frame_index, len(leds)])  # Frame index and LED count
    
    # Add LED data
    for led in leds:
        # Support both index-based and coordinate-based formats
        if 'index' in led:
            # Convert linear index to x,y
            width = 60  # Default width
            index = led['index']
            x = index % width
            y = index // width
        else:
            x = led.get('x', 0)
            y = led.get('y', 0)
        
        # Get RGB color
        if 'r' in led and 'g' in led and 'b' in led:
            # Direct RGB values
            r = led.get('r', 0)
            g = led.get('g', 0)
            b = led.get('b', 0)
        else:
            color = led.get('color', '#ff0000')
            if isinstance(color, str) and color.startswith('#'):
                # Parse hex color
                color = color.lstrip('#')
                if len(color) == 6:
                    r = int(color[0:2], 16)
                    g = int(color[2:4], 16)
                    b = int(color[4:6], 16)
                else:
                    # Default red for invalid colors
                    r, g, b = 255, 0, 0
            else:
                # Default red
                r, g, b = 255, 0, 0
        
        # Add to packet
        packet.extend([x, y, r, g, b])
    
    return bytes(packet)

def send_animation(can_interface, animation):
    """
    Send an animation to the lights controller
    """
    frames = animation.get('frames', [])
    if not frames:
        logger.error("Animation has no frames")
        return False
    
    # Use direct command IDs from the protocol definition
    # ANIMATION_START = 10, ANIMATION_FRAME = 11, ANIMATION_END = 12
    
    # First, send a start command to initialize animation mode
    logger.info("Sending animation start command")
    
    # Get numerical values for component type and component ID
    lights_component_type = can_interface.protocol_registry.get_component_type("LIGHTS")
    all_component_id = 0xFF  # 'ALL' component ID
    
    # Send ANIMATION_START directly using send_message
    result = can_interface.send_message(
        msg_type=0,  # COMMAND
        comp_type=lights_component_type,
        comp_id=all_component_id,
        cmd_id=10,   # ANIMATION_START
        value_type=2, # UINT8
        value=1
    )
    
    if not result:
        logger.error("Failed to send animation start command")
        return False
    
    # Small delay to give the controller time to prepare
    time.sleep(0.1)
    
    # Send each frame
    for i, frame in enumerate(frames):
        logger.info(f"Sending frame {i+1} of {len(frames)}")
        
        # Prepare the binary data for this frame
        binary_data = prepare_animation_frame(frame, i)
        
        # Send the binary frame data using direct command ID
        result = can_interface.send_binary_data(
            component_type_name="LIGHTS",
            component_name="ALL",
            command_name="ANIMATION_FRAME",
            value_type="UINT8",
            data=binary_data
        )
        
        if not result:
            logger.error(f"Failed to send frame {i}")
            return False
        
        # Small delay to prevent flooding the bus
        time.sleep(0.05)
    
    # Finally, send end command to start playback
    logger.info("Sending animation end command")
    result = can_interface.send_message(
        msg_type=0,  # COMMAND
        comp_type=lights_component_type,
        comp_id=all_component_id,
        cmd_id=12,   # ANIMATION_END
        value_type=2,  # UINT8
        value=len(frames)
    )
    
    if not result:
        logger.error("Failed to send animation end command")
        return False
    
    return True

def stop_animation(can_interface):
    """
    Stop the currently playing animation
    """
    logger.info("Stopping animation playback")
    
    # Get numerical values for component type and component ID
    lights_component_type = can_interface.protocol_registry.get_component_type("LIGHTS")
    all_component_id = 0xFF  # 'ALL' component ID
    
    # Send ANIMATION_STOP directly
    result = can_interface.send_message(
        msg_type=0,  # COMMAND
        comp_type=lights_component_type,
        comp_id=all_component_id,
        cmd_id=13,   # ANIMATION_STOP
        value_type=2,  # UINT8
        value=0
    )
    
    if not result:
        logger.error("Failed to send animation stop command")
        return False
    
    return True

def load_animation_frames(animation_name):
    """Load animation frames from JSON files."""
    animation_dir = os.path.join(server_dir, 'api/animations', animation_name, 'frames')
    frames = []
    
    if not os.path.exists(animation_dir):
        logger.error(f"Animation directory not found: {animation_dir}")
        return frames
    
    # Load all frame JSON files
    frame_files = sorted([f for f in os.listdir(animation_dir) if f.endswith('.json')])
    for frame_file in frame_files:
        frame_path = os.path.join(animation_dir, frame_file)
        try:
            with open(frame_path, 'r') as f:
                frame_data = json.load(f)
                frames.append(frame_data)
                logger.debug(f"Loaded frame: {frame_path}")
        except Exception as e:
            logger.error(f"Error loading frame {frame_path}: {e}")
    
    return frames

def send_animation_frame_individual_leds(can_interface, frame_data):
    """Send animation frame by sending each LED individually."""
    # Handle case where frame_data is an array containing a single object
    if isinstance(frame_data, list) and len(frame_data) > 0:
        frame_data = frame_data[0]
    
    frame_index = frame_data.get('frameIndex', 0)
    leds = frame_data.get('leds', [])
    
    # First send the frame index
    can_interface.send_command('LIGHTS', 'ALL', 'ANIMATION_FRAME', direct_value=frame_index)
    
    # Then send each LED
    for i, led in enumerate(leds):
        r = led.get('r', 0)
        g = led.get('g', 0)
        b = led.get('b', 0)
        
        # Pack RGB into a single value (assuming 8 bits per color)
        rgb_value = (r << 16) | (g << 8) | b
        
        # Send the LED value - we'll use the LED_VALUE command with the LED index
        # The LED_VALUE command is expected to be implemented in the protocol
        can_interface.send_command('LIGHTS', 'ALL', 'LED_VALUE', direct_value=rgb_value, command_data={'index': i})
        
        # Small delay to prevent bus flooding
        time.sleep(0.001)
    
    return True

def run_animation_test():
    """Run the animation test."""
    # Create CAN interface
    can_interface = CANInterfaceWrapper(node_id=0x01, channel='can0')
    
    # Start message processing
    can_interface.start_processing()
    
    try:
        # Animation name to test
        animation_name = 'car_knight_rider'
        logger.info(f"Sending animation: {animation_name}")
        
        # Load animation frames
        frames = load_animation_frames(animation_name)
        if not frames:
            logger.error(f"No frames found for animation {animation_name}")
            return False
        
        # Send animation start command
        logger.info("Sending animation start command")
        can_interface.send_command('LIGHTS', 'ALL', 'ANIMATION_START', direct_value=1)
        time.sleep(0.1)  # Give a little time for processing
        
        # Send each frame
        for i, frame in enumerate(frames):
            logger.info(f"Sending frame {i+1} of {len(frames)}")
            
            # Send frame data using individual LED messages
            send_animation_frame_individual_leds(can_interface, frame)
            
            # Delay between frames
            time.sleep(0.1)
        
        # Send animation end command
        logger.info("Sending animation end command")
        can_interface.send_command('LIGHTS', 'ALL', 'ANIMATION_END', direct_value=1)
        
        # Test successful
        logger.info("Animation test completed successfully")
        return True
        
    except Exception as e:
        logger.error(f"Test failed with error: {str(e)}")
        return False
    finally:
        # Stop message processing
        can_interface.stop_processing()

if __name__ == "__main__":
    # Run the test
    success = run_animation_test()
    sys.exit(0 if success else 1) 