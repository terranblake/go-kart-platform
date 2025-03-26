#!/usr/bin/env python3
"""
Test and fix protocol registry to verify animation commands are properly registered
"""
import logging

# Configure logging
logging.basicConfig(level=logging.INFO, 
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Import protocol registry
from lib.can.protocol_registry import ProtocolRegistry

def main():
    """Test and fix the protocol registry"""
    logger.info("Creating protocol registry...")
    registry = ProtocolRegistry()
    
    # Ensure components section exists
    if 'components' not in registry.registry:
        logger.info("Adding components section to registry")
        registry.registry['components'] = {}
    
    # Ensure lights component exists
    if 'lights' not in registry.registry.get('components', {}):
        logger.info("Adding lights component to registry")
        registry.registry['components']['lights'] = {}
    
    # Ensure ALL component is registered
    if 'ALL' not in registry.registry['components']['lights']:
        logger.info("Adding ALL component ID to lights components")
        registry.registry['components']['lights']['ALL'] = 0xFF  # 255
    
    # Ensure commands section exists
    if 'commands' not in registry.registry:
        logger.info("Adding commands section to registry")
        registry.registry['commands'] = {}
    
    # Ensure lights commands exist
    if 'lights' not in registry.registry.get('commands', {}):
        logger.info("Adding lights commands to registry")
        registry.registry['commands']['lights'] = {}
    
    # Animation commands to ensure are registered
    animation_commands = {
        'ANIMATION_START': 10,
        'ANIMATION_FRAME': 11,
        'ANIMATION_END': 12,
        'ANIMATION_STOP': 13
    }
    
    # Register animation commands
    for cmd_name, cmd_id in animation_commands.items():
        if cmd_name not in registry.registry['commands']['lights']:
            logger.info(f"Adding animation command {cmd_name} with ID {cmd_id}")
            registry.registry['commands']['lights'][cmd_name] = {
                'id': cmd_id,
                'values': {}
            }
        else:
            logger.info(f"Animation command {cmd_name} already registered with ID {registry.registry['commands']['lights'][cmd_name]['id']}")
    
    # Check all animation commands
    logger.info("Animation commands in registry:")
    for cmd_name in animation_commands:
        if cmd_name in registry.registry['commands']['lights']:
            cmd_id = registry.registry['commands']['lights'][cmd_name]['id']
            logger.info(f"  {cmd_name}: ID = {cmd_id}")
    
    # Test getting animation command IDs
    for cmd_name in animation_commands:
        cmd_id = registry.get_command_id('lights', cmd_name)
        if cmd_id is not None:
            logger.info(f"Successfully got command ID for {cmd_name}: {cmd_id}")
        else:
            logger.error(f"Failed to get command ID for {cmd_name}")
    
    logger.info("Protocol registry test completed")

if __name__ == "__main__":
    main() 