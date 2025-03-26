#!/usr/bin/env python3
"""
Test protocol registry to verify animation commands and ALL component are properly included
"""
import sys
import os
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, 
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Add server directory to path
server_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'dashboard', 'server')
sys.path.insert(0, server_dir)

# Import protocol registry
from lib.can.protocol_registry import ProtocolRegistry

def main():
    """Test the protocol registry"""
    logger.info("Creating protocol registry...")
    registry = ProtocolRegistry()
    
    # Check for animation commands
    logger.info("Checking for animation commands in registry...")
    if 'lights' in registry.registry['commands']:
        lights_commands = registry.registry['commands']['lights']
        animation_commands = [cmd for cmd in lights_commands.keys() if cmd.startswith('ANIMATION_')]
        
        if animation_commands:
            logger.info(f"Found animation commands: {animation_commands}")
            for cmd in animation_commands:
                logger.info(f"  {cmd}: ID = {lights_commands[cmd]['id']}")
        else:
            logger.error("No animation commands found in lights component!")
    else:
        logger.error("No lights component found in registry!")
    
    # Check for ALL component ID
    logger.info("Checking for ALL component ID in registry...")
    if 'lights' in registry.registry['components']:
        light_components = registry.registry['components']['lights']
        if 'ALL' in light_components:
            logger.info(f"Found ALL component: ID = {light_components['ALL']}")
        else:
            logger.error("No ALL component found in lights components!")
    else:
        logger.error("No lights component found in registry!")
    
    # Try creating messages with animation commands
    logger.info("Testing message creation with animation commands...")
    
    animation_start_msg = registry.create_message(
        message_type="COMMAND",
        component_type="LIGHTS",
        component_name="ALL",
        command_name="ANIMATION_START",
        value=1
    )
    
    if animation_start_msg and None not in animation_start_msg[:4]:
        logger.info(f"Successfully created ANIMATION_START message: {animation_start_msg}")
    else:
        logger.error(f"Failed to create ANIMATION_START message: {animation_start_msg}")
    
    # Return success or failure
    if animation_commands and 'ALL' in light_components:
        logger.info("Protocol registry test PASSED!")
        return 0
    else:
        logger.error("Protocol registry test FAILED!")
        return 1

if __name__ == "__main__":
    sys.exit(main()) 