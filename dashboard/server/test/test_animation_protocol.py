#!/usr/bin/env python3
"""
Test animation protocol functionality
This script verifies:
1. Animation commands are properly registered in the protocol registry
2. Binary data can be successfully sent through the CAN interface
"""

import os
import sys
import logging
from pathlib import Path

# Configure logging
logging.basicConfig(level=logging.INFO,
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Add the parent directory to the path so we can import the modules
script_dir = Path(__file__).parent
server_dir = script_dir.parent
sys.path.insert(0, str(server_dir))

# Import the required modules
from lib.can.protocol_registry import ProtocolRegistry
from lib.can.interface import CANInterfaceWrapper, MockCANInterface

def test_animation_registry():
    """Test that animation commands are properly registered in the protocol registry"""
    logger.info("Testing animation command registration...")
    registry = ProtocolRegistry()
    
    # Check if LIGHTS component exists
    if 'lights' not in registry.registry['components']:
        logger.error("LIGHTS component not found in registry")
        return False
        
    # Check if animation commands are registered
    animation_commands = ['ANIMATION_START', 'ANIMATION_FRAME', 'ANIMATION_END', 'ANIMATION_STOP']
    for cmd in animation_commands:
        cmd_id = registry.get_command_id('lights', cmd)
        if cmd_id is None:
            logger.error(f"Animation command {cmd} not registered")
            return False
        logger.info(f"Found animation command {cmd} with ID {cmd_id}")
        
    # Check if ALL component is registered
    all_component_id = registry.get_component_id('lights', 'ALL')
    if all_component_id is None:
        logger.error("ALL component not registered for LIGHTS")
        return False
    logger.info(f"Found ALL component with ID {all_component_id}")
    
    logger.info("Animation registry test PASSED")
    return True

def test_binary_data_sending():
    """Test binary data sending functionality"""
    logger.info("Testing binary data sending...")
    
    # Create a mock CAN interface
    mock_interface = MockCANInterface(node_id=0x01)
    can_interface = CANInterfaceWrapper(mock_interface)
    
    # Create some test animation data
    frame_index = 0
    num_leds = 3
    
    # Create a test frame with 3 LEDs
    test_data = bytearray([frame_index, num_leds])
    
    # Add LED data: (x, y, r, g, b) for each LED
    test_data.extend([0, 0, 255, 0, 0])  # LED 1: x=0, y=0, red
    test_data.extend([1, 0, 0, 255, 0])  # LED 2: x=1, y=0, green
    test_data.extend([2, 0, 0, 0, 255])  # LED 3: x=2, y=0, blue
    
    # Send the binary data
    result = can_interface.send_binary_data(
        component_type_name="LIGHTS",
        component_name="ALL",
        command_name="ANIMATION_FRAME",
        value_type="UINT8",
        data=bytes(test_data)
    )
    
    if not result:
        logger.error("Failed to send binary data")
        return False
    
    logger.info("Binary data sending test PASSED")
    return True

def main():
    """Run all tests"""
    registry_result = test_animation_registry()
    binary_result = test_binary_data_sending()
    
    if registry_result and binary_result:
        logger.info("All animation protocol tests PASSED")
        return 0
    else:
        logger.error("Some animation protocol tests FAILED")
        return 1

if __name__ == "__main__":
    sys.exit(main()) 