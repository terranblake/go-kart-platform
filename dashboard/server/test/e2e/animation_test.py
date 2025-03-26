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
ANIMATION_DIR = os.path.join(server_dir, '../..', 'tools', 'animations')

def load_animation(animation_id):
    """
    Load an animation from the animations directory
    """
    # Check for car animations directory
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
    
    # First, send a start command to initialize animation mode
    logger.info("Sending animation start command")
    result = can_interface.send_command(
        component_type='LIGHTS',
        component_name='ALL',
        command_name='ANIMATION_START',
        direct_value=1
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
        
        # Send the binary frame data
        result = can_interface.send_binary_data(
            component_type_name='LIGHTS',
            component_name='ALL',
            command_name='ANIMATION_FRAME',
            value_type='UINT8',
            data=binary_data
        )
        
        if not result:
            logger.error(f"Failed to send frame {i}")
            return False
        
        # Small delay to prevent flooding the bus
        time.sleep(0.05)
    
    # Finally, send end command to start playback
    logger.info("Sending animation end command")
    result = can_interface.send_command(
        component_type='LIGHTS',
        component_name='ALL',
        command_name='ANIMATION_END',
        direct_value=len(frames)
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
    result = can_interface.send_command(
        component_type='LIGHTS',
        component_name='ALL',
        command_name='ANIMATION_STOP',
        direct_value=0
    )
    
    if not result:
        logger.error("Failed to send animation stop command")
        return False
    
    return True

def test_animation_transmission():
    """
    End-to-end test of animation transmission
    """
    logger.info("Starting animation transmission test")
    
    # Create a CAN interface wrapper
    can_interface = CANInterfaceWrapper(node_id=NODE_ID, channel=CHANNEL, baudrate=BAUDRATE)
    
    # Start message processing
    can_interface.start_processing()
    
    try:
        # Test with a simple animation
        animation_id = "car_knight_rider"  # Knight Rider animation (light chaser)
        
        # Load the animation
        animation = load_animation(animation_id)
        if not animation:
            logger.error(f"Failed to load animation {animation_id}")
            return False
        
        # Send the animation
        logger.info(f"Sending animation: {animation.get('name', animation_id)}")
        if not send_animation(can_interface, animation):
            logger.error("Failed to send animation")
            return False
        
        # Let it play for a few seconds
        logger.info("Animation playing...")
        time.sleep(5)
        
        # Stop the animation
        if not stop_animation(can_interface):
            logger.error("Failed to stop animation")
            return False
        
        logger.info("Animation test completed successfully")
        return True
        
    except KeyboardInterrupt:
        logger.info("Test interrupted by user")
    except Exception as e:
        logger.error(f"Test failed with error: {e}")
    finally:
        # Stop message processing
        can_interface.stop_processing()
    
    return False

if __name__ == "__main__":
    # Run the test
    success = test_animation_transmission()
    sys.exit(0 if success else 1) 