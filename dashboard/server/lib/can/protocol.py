"""
Protocol definitions for the Go-Kart CAN network
"""

import os
import sys
import logging
import importlib.util
from ._can_interface import MessageType, ComponentType, ValueType

# Configure logging
logger = logging.getLogger(__name__)

# Import the generated Protocol Buffer modules
def import_pb_modules():
    """Import Protocol Buffer modules from the generated directory."""
    pb_modules = {}
    
    # Get the path to the generated Python Protocol Buffer files
    repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..'))
    pb_path = os.path.join(repo_root, 'protocol', 'generated', 'python')
    
    # Add to Python path if not already there
    if pb_path not in sys.path:
        sys.path.append(pb_path)
    
    # Import common, controls, and lights modules
    module_names = ['common_pb2', 'controls_pb2', 'lights_pb2']
    
    for module_name in module_names:
        try:
            if module_name in sys.modules:
                pb_modules[module_name] = sys.modules[module_name]
            else:
                spec = importlib.util.find_spec(module_name)
                if spec:
                    module = importlib.util.module_from_spec(spec)
                    spec.loader.exec_module(module)
                    pb_modules[module_name] = module
                else:
                    logger.warning(f"Could not find module: {module_name}")
        except Exception as e:
            logger.error(f"Error importing {module_name}: {e}")
    
    return pb_modules

# Map Protocol Buffer enums to dictionaries for easier access
def map_enum_to_dict(pb_enum):
    """Convert a Protocol Buffer enum to a dictionary."""
    return {name: value for name, value in pb_enum.items()}

def load_protocol_definitions():
    """Load protocol definitions from Protocol Buffer modules."""
    try:
        # Import Protocol Buffer modules
        pb_modules = import_pb_modules()
        
        if not pb_modules:
            logger.error("No Protocol Buffer modules could be imported")
            return {}
        
        # Initialize protocol structure
        protocol = {
            'message_types': map_enum_to_dict(pb_modules['common_pb2'].kart.common.MessageType.items()),
            'component_types': map_enum_to_dict(pb_modules['common_pb2'].kart.common.ComponentType.items()),
            'value_types': map_enum_to_dict(pb_modules['common_pb2'].kart.common.ValueType.items()),
            'components': {
                'lights': map_enum_to_dict(pb_modules['lights_pb2'].kart.lights.LightComponentId.items()),
                'controls': map_enum_to_dict(pb_modules['controls_pb2'].kart.controls.ControlComponentId.items())
            },
            'commands': {
                'lights': {
                    'mode': {
                        'id': pb_modules['lights_pb2'].kart.lights.LightCommandId.MODE,
                        'values': map_enum_to_dict(pb_modules['lights_pb2'].kart.lights.LightModeValue.items())
                    },
                    'signal': {
                        'id': pb_modules['lights_pb2'].kart.lights.LightCommandId.SIGNAL,
                        'values': map_enum_to_dict(pb_modules['lights_pb2'].kart.lights.LightSignalValue.items())
                    },
                    'brake': {
                        'id': pb_modules['lights_pb2'].kart.lights.LightCommandId.BRAKE,
                        'values': {'ON': 1, 'OFF': 0}
                    }
                },
                'controls': {
                    'emergency': {
                        'id': pb_modules['controls_pb2'].kart.controls.ControlCommandId.EMERGENCY,
                        'values': map_enum_to_dict(pb_modules['controls_pb2'].kart.controls.ControlEmergencyValue.items())
                    },
                    'mode': {
                        'id': pb_modules['controls_pb2'].kart.controls.ControlCommandId.MODE,
                        'values': map_enum_to_dict(pb_modules['controls_pb2'].kart.controls.ControlModeValue.items())
                    },
                    'parameter': {
                        'id': pb_modules['controls_pb2'].kart.controls.ControlCommandId.PARAMETER,
                        'values': {}  # Parameters are direct values
                    }
                }
            }
        }
        
        logger.info("Protocol definitions loaded from Protocol Buffer modules")
        return protocol
    except Exception as e:
        logger.error(f"Error loading protocol definitions: {e}")
        return {}

def get_command_by_name(protocol, component_type_name, component_name, command_name):
    """Get command information by name."""
    try:
        # Get component type
        if component_type_name == 'lights':
            component_type = ComponentType.LIGHTS
        elif component_type_name == 'controls':
            component_type = ComponentType.CONTROLS
        elif component_type_name == 'motors':
            component_type = ComponentType.MOTORS
        elif component_type_name == 'sensors':
            component_type = ComponentType.SENSORS
        elif component_type_name == 'battery':
            component_type = ComponentType.BATTERY
        else:
            logger.error(f"Unknown component type: {component_type_name}")
            return None
        
        # Get component ID
        component_id = protocol['components'].get(component_type_name, {}).get(component_name)
        if component_id is None:
            logger.error(f"Component not found: {component_type_name}.{component_name}")
            return None
        
        # Get command ID and values
        command = protocol['commands'].get(component_type_name, {}).get(command_name)
        if command is None:
            logger.error(f"Command not found: {component_type_name}.{command_name}")
            return None
        
        return {
            'message_type': MessageType.COMMAND,
            'component_type': component_type,
            'component_id': component_id,
            'command_id': command['id'],
            'value_type': ValueType.INT16,  # Default value type
            'values': command.get('values', {})
        }
    except Exception as e:
        logger.error(f"Error getting command by name: {e}")
        return None

def view_protocol_structure(protocol=None):
    """Generate a human-readable representation of the protocol structure."""
    if protocol is None:
        protocol = load_protocol_definitions()
    
    structure = {
        'message_types': protocol.get('message_types', {}),
        'component_types': protocol.get('component_types', {}),
        'value_types': protocol.get('value_types', {}),
        'components': protocol.get('components', {}),
        'commands': protocol.get('commands', {})
    }
    
    return structure 