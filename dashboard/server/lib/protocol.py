# defining the protocol used to define the commands and telemetry for the go-kart
import os
import json
import logging

logger = logging.getLogger(__name__)

def load_protocol_definitions(filename):
    """Load protocol definitions from JSON file"""
    try:
        # Protocol definitions JSON path
        json_path = os.path.join(os.path.dirname(os.path.dirname(__file__)), filename)
        logger.info(f"Loading protocol from: {json_path}")
        
        # Load JSON
        with open(json_path, 'r') as f:
            protocol = json.load(f)
        
        # Debug print some key parts of the protocol
        if 'kart' in protocol:
            light_modes = protocol.get('kart', {}).get('commands', {}).get('lights', {}).get('mode', {}).get('values', {})
            logger.info(f"Found light modes in protocol: {list(light_modes.keys())}")
        else:
            logger.warning("Protocol doesn't contain 'kart' key")
        
        logger.info("Kart protocol definitions loaded successfully")
        return protocol
    except FileNotFoundError:
        logger.error(f"Protocol file not found: {filename}")
        return {"kart": {}}
    except json.JSONDecodeError:
        logger.error(f"Invalid JSON in protocol file: {filename}")
        return {"kart": {}}

def get_command_value(protocol, command_type, command, value_name):
    """Get numeric value for a command value by name."""
    try:
        value_hex = protocol["kart"]["commands"][command_type][command]["values"][value_name]
        return int(value_hex, 16)
    except (KeyError, ValueError):
        logger.error(f"Invalid command value: {command_type}.{command}.{value_name}")
        return 0

def get_command_values(protocol, command_type, command):
    """Get all possible values for a command."""
    try:
        return protocol["kart"]["commands"][command_type][command]["values"]
    except KeyError:
        logger.error(f"Invalid command: {command_type}.{command}")
        return {}

def get_component_type_id(protocol, component_type):
    """Get numeric ID for a component type."""
    try:
        type_id_hex = protocol["kart"]["component_types"][component_type]["id"]
        return int(type_id_hex, 16)
    except (KeyError, ValueError):
        logger.error(f"Invalid component type: {component_type}")
        return 0

def get_component_id(protocol, component_type, component):
    """Get numeric ID for a component."""
    try:
        component_id_hex = protocol["kart"]["components"][component_type][component]["id"]
        return int(component_id_hex, 16)
    except (KeyError, ValueError):
        logger.error(f"Invalid component: {component_type}.{component}")
        return 0

def get_command_id(protocol, command_type, command):
    """Get numeric ID for a command."""
    try:
        command_id_hex = protocol["kart"]["commands"][command_type][command]["id"]
        return int(command_id_hex, 16)
    except (KeyError, ValueError):
        logger.error(f"Invalid command: {command_type}.{command}")
        return 0

def get_message_header(protocol, command_type, command):
    """Generate message header for a command."""
    component_type_id = get_component_type_id(protocol, command_type)
    command_id = get_command_id(protocol, command_type, command)
    
    if component_type_id is None or command_id is None:
        return None
        
    return (component_type_id << 4) | command_id
