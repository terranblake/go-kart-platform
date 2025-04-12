"""

This module provides a registry for the protocol definitions.
It is responsible for:
- loading the protocol definitions
- building the registry
- providing a higher-level API for the protocol definitions

It is not responsible for:
- sending and receiving CAN messages
- processing the messages
- storing state
- starting the CANInterface
- stopping the CANInterface

todo: the structure of the registry puts commands under each component id
      which duplicates commands for each component id

"""

import importlib
import pkgutil
import sys
import logging
from typing import Dict, Any, Optional, List, Tuple
import os

class ProtocolRegistry:
    """Dynamic registry for Protocol Buffer definitions that adapts to new component types"""
    
    def __init__(self, pb_path: str = None):
        self.logger = logging.getLogger(__name__)
        self.pb_path = self._resolve_protocol_path(pb_path)
        self.modules = {}
        self.registry = {}
        self._load_modules()
        self._build_registry()
        
    def _resolve_protocol_path(self, pb_path: str = None) -> str:
        """Intelligently find the protocol directory"""
        
        # If a path is provided and exists, use it
        if pb_path and os.path.exists(pb_path):
            self.logger.debug(f"Using provided protocol path: {pb_path}")
            return pb_path
            
        # Try common locations based on project structure
        possible_paths = [
            # Provided path or default
            pb_path or "./protocol/generated/python",
            
            # Relative to current directory
            os.path.join(os.getcwd(), "protocol", "generated", "python"),
            
            # Relative to server directory 
            os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 
                         "protocol", "generated", "python"),
            
            # One level up from server directory (project root)
            os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))), 
                         "protocol", "generated", "python"),
                         
            # Raspberry Pi specific path
            "/home/pi/go-kart-platform/protocol/generated/python",
        ]
        
        # Try each path
        for path in possible_paths:
            if os.path.exists(path):
                self.logger.debug(f"Found protocol directory at: {path}")
                return path
                
        # If we get here, just return the provided path or default
        # The _load_modules will handle the error case
        self.logger.warning(f"Could not find protocol directory, using default: {pb_path or './protocol/generated/python'}")
        return pb_path or "./protocol/generated/python"
        
    def _load_modules(self) -> None:
        """Discover and load all Protocol Buffer modules"""
        
        self.logger.debug(f"Adding protocol path to sys.path: {self.pb_path}")
        sys.path.append(self.pb_path)
        
        # Check if the directory exists
        if not os.path.exists(self.pb_path):
            self.logger.error(f"Protocol directory does not exist: {self.pb_path}")
            return
            
        # List modules in the directory
        modules_in_dir = [name for _, name, _ in pkgutil.iter_modules([self.pb_path])]
        self.logger.debug(f"Found modules in {self.pb_path}: {modules_in_dir}")
        
        for _, name, _ in pkgutil.iter_modules([self.pb_path]):
            if name.endswith('_pb2'):
                try:
                    self.modules[name] = importlib.import_module(name)
                    self.logger.debug(f"Loaded protocol module: {name}")
                except ImportError as e:
                    self.logger.error(f"Failed to load {name}: {e}")
                    # Try with absolute import
                    try:
                        full_name = f"{os.path.basename(self.pb_path)}.{name}"
                        self.logger.debug(f"Trying absolute import: {full_name}")
                        self.modules[name] = importlib.import_module(full_name)
                        self.logger.debug(f"Loaded protocol module with absolute import: {name}")
                    except ImportError as e2:
                        self.logger.error(f"Absolute import also failed for {name}: {e2}")
    
    def _build_registry(self) -> None:
        """Build complete protocol registry from loaded modules"""
        # Initialize registry structure
        self.registry = {
            'message_types': {},
            'component_types': {},
            'value_types': {},
            'components': {},
            'commands': {}
        }
        
        # Extract enums from common module first (expected to have message/component/value types)
        common_module = self.modules.get('common_pb2')
        if common_module:
            self._extract_common_enums(common_module)
            
        # Extract component-specific enums from other modules
        for name, module in self.modules.items():
            if name != 'common_pb2':
                self._extract_component_enums(module)
        
        # Log the extracted registry for debugging
        self.logger.debug(f"Extracted message_types: {self.registry['message_types']}")
        self.logger.debug(f"Extracted component_types: {self.registry['component_types']}")
        self.logger.debug(f"Extracted value_types: {self.registry['value_types']}")
    
    def _extract_common_enums(self, module: Any) -> None:
        """Extract common message, component, and value type enums"""
        try:
            # Direct extraction of well-known enums
            if hasattr(module, 'MessageType') and hasattr(module.MessageType, 'items'):
                self.registry['message_types'] = dict(module.MessageType.items())
                self.logger.debug(f"Extracted MessageType enum: {self.registry['message_types']}")
                
            if hasattr(module, 'ComponentType') and hasattr(module.ComponentType, 'items'):
                self.registry['component_types'] = dict(module.ComponentType.items())
                self.logger.debug(f"Extracted ComponentType enum: {self.registry['component_types']}")
                
            if hasattr(module, 'ValueType') and hasattr(module.ValueType, 'items'):
                self.registry['value_types'] = dict(module.ValueType.items())
                self.logger.debug(f"Extracted ValueType enum: {self.registry['value_types']}")
        except Exception as e:
            self.logger.error(f"Error extracting common enums: {e}")
    
    def _extract_component_enums(self, module: Any) -> None:
        """Extract component-specific enums (component IDs, command IDs, values)"""
        try:
            # Determine component type from module name 
            component_type = self._determine_component_type(module)
            if not component_type:
                return
                
            # Initialize component structures if needed
            if component_type not in self.registry['components']:
                self.registry['components'][component_type] = {}
            if component_type not in self.registry['commands']:
                self.registry['commands'][component_type] = {}
            
            # First pass: collect all enums
            component_id_enum = None
            command_id_enum = None
            value_enums = {}
            
            # Log all enums for debugging
            enums_in_module = []
            for attr_name in dir(module):
                attr = getattr(module, attr_name)
                
                # Skip if not a valid protobuf enum with items method
                if not hasattr(attr, 'items') or not callable(attr.items):
                    continue
                    
                enums_in_module.append(attr_name)
                
                # ComponentId enums
                if attr_name.endswith('ComponentId'):
                    component_id_enum = attr
                    self.logger.debug(f"Found component ID enum: {attr_name}")
                
                # CommandId enums
                elif attr_name.endswith('CommandId'):
                    command_id_enum = attr
                    self.logger.debug(f"Found command ID enum: {attr_name}")
                
                # Value enums
                elif attr_name.endswith('Value'):
                    value_enums[attr_name] = attr
                    self.logger.debug(f"Found value enum: {attr_name} with values: {dict(attr.items())}")
            
            self.logger.debug(f"All enums in module {module.__name__}: {enums_in_module}")
            
            # Process component ID enum
            if component_id_enum:
                component_ids = dict(component_id_enum.items())
                self.logger.debug(f"Component IDs for {component_type}: {component_ids}")
                
                # For each component ID, create a component entry
                for comp_name, comp_id in component_ids.items():
                    # Skip special ALL component
                    if comp_name == 'ALL':
                        continue
                        
                    # Store component ID directly, not in a nested dictionary
                    self.registry['components'][component_type][comp_name] = comp_id
                    self.logger.debug(f"Added component {comp_name} (ID: {comp_id}) to {component_type}")
            
            # Process command ID enum
            if command_id_enum:
                command_ids = dict(command_id_enum.items())
                self.logger.debug(f"Command IDs for {component_type}: {command_ids}")
                
                command_to_value_map = {}
                component_prefix = component_type.title()  # e.g., "Light" for "lights"
                
                for cmd_name in command_ids.keys():
                    # Look for value enum with pattern [ComponentType][CommandName]Value
                    expected_enum_name = f"{component_prefix}{cmd_name}Value"
                    
                    # Try direct match first
                    if expected_enum_name in value_enums:
                        command_to_value_map[cmd_name] = value_enums[expected_enum_name]
                        self.logger.debug(f"Direct pattern match: {cmd_name} -> {expected_enum_name}")
                    else:
                        # Try case-insensitive match
                        for enum_name, enum_obj in value_enums.items():
                            # Try matching by command name in enum name
                            if cmd_name.lower() in enum_name.lower():
                                command_to_value_map[cmd_name] = enum_obj
                                self.logger.debug(f"Partial match: {cmd_name} -> {enum_name}")
                                break
                
                # Add commands to the top-level commands registry by component type
                for cmd_name, cmd_id in command_ids.items():
                    # Create the command entry at the top level instead of per component
                    self.registry['commands'][component_type][cmd_name] = {
                        'id': cmd_id,
                        'values': {}
                    }
                    
                    # If we have a value enum for this command, add the values
                    if cmd_name in command_to_value_map:
                        values_dict = dict(command_to_value_map[cmd_name].items())
                        self.registry['commands'][component_type][cmd_name]['values'] = values_dict
                        self.logger.debug(f"Added {len(values_dict)} values for {cmd_name}: {list(values_dict.keys())}")
                    else:
                        self.logger.debug(f"No value enum found for command {cmd_name}")
                
                self.logger.debug(f"Added {len(command_ids)} commands to component type {component_type}")
                
        except Exception as e:
            self.logger.error(f"Error extracting component enums: {e}", exc_info=True)
    
    def _determine_component_type(self, module: Any) -> Optional[str]:
        """Determine component type from module name or content"""
        module_name = module.__name__.lower()
        
        # If common module, return None (it's not a component module)
        if 'common' in module_name:
            return None
            
        # Extract component type directly from module name by removing _pb2 suffix
        if module_name.endswith('_pb2'):
            component_type = module_name.replace('_pb2', '')
            self.logger.debug(f"Determined component type {component_type} from module name {module_name}")
            return component_type
            
        # Extract from module name by checking if it contains a component type name
        for comp_type_name in self.registry['component_types'].keys():
            if comp_type_name.lower() in module_name:
                self.logger.debug(f"Determined component type {comp_type_name.lower()} from module name {module_name}")
                return comp_type_name.lower()
        
        # If we couldn't determine from the name, check if there's a ComponentId enum
        for attr_name in dir(module):
            if 'ComponentId' in attr_name and hasattr(module, attr_name) and hasattr(getattr(module, attr_name), 'items'):
                # Extract component type from the ComponentId enum name
                component_type = attr_name.replace('ComponentId', '').lower()
                self.logger.debug(f"Determined component type {component_type} from ComponentId enum {attr_name}")
                return component_type
            
        # If we get here, we couldn't determine the component type
        self.logger.warning(f"Could not determine component type for module {module_name}")
        return None
    
    def _extract_all_enums(self, obj: Any, path: str = "") -> Dict[str, Dict]:
        """Recursively extract all enum definitions from an object"""
        results = {}
        
        # Check if object is an enum
        if hasattr(obj, 'items') and callable(obj.items):
            return {path: {k: v for k, v in obj.items()}}
        
        # Explore attributes
        for name in dir(obj):
            if name.startswith('_'):
                continue
                
            try:
                attr = getattr(obj, name)
                new_path = f"{path}.{name}" if path else name
                
                # Only process objects with attributes
                if hasattr(attr, '__dict__'):
                    child_results = self._extract_all_enums(attr, new_path)
                    results.update(child_results)
            except Exception as e:
                pass
                
        return results
    
    def get_message_type(self, type_name: str) -> Optional[int]:
        """Get message type value by name"""
        return self.registry['message_types'].get(type_name)
    
    def get_component_type(self, type_name: str) -> Optional[int]:
        """Get component type value by name"""
        return self.registry['component_types'].get(type_name.upper())
    
    def get_value_type(self, type_name: str) -> Optional[int]:
        """Get value type by name"""
        return self.registry['value_types'].get(type_name.upper())
    
    def get_component_id(self, component_type: str, component_name: str) -> Optional[int]:
        """Get component ID by type and name"""
        component_type = component_type.lower()
        if component_type in self.registry['components'] and component_name in self.registry['components'][component_type]:
            return self.registry['components'][component_type][component_name]
        return None
    
    def get_command_id(self, component_type: str, command_name: str) -> Optional[int]:
        """Get command ID by component type and command name"""
        component_type = component_type.lower()
        if (component_type in self.registry['commands'] and 
            command_name in self.registry['commands'][component_type]):
            return self.registry['commands'][component_type][command_name].get('id')
        return None
    
    def get_command_value(self, component_type: str, command_name: str, value_name: str) -> Optional[int]:
        """Get command value by component type, command name, and value name"""
        component_type = component_type.lower()
        if (component_type in self.registry['commands'] and
            command_name in self.registry['commands'][component_type] and
            'values' in self.registry['commands'][component_type][command_name] and
            value_name in self.registry['commands'][component_type][command_name]['values']):
            return self.registry['commands'][component_type][command_name]['values'][value_name]
        return None
    
    def get_component_types(self) -> List[str]:
        """Get all registered component types"""
        return list(self.registry['components'].keys())
    
    def get_commands(self, component_type: str) -> List[str]:
        """Get all commands for a component type"""
        component_type = component_type.lower()
        return list(self.registry['commands'][component_type].keys())
    
    def get_command_values(self, component_type: str, command_name: str) -> Dict[str, int]:
        """Get all possible values for a command"""
        component_type = component_type.lower()
        return self.registry['commands'][component_type][command_name]['values']
    
    def create_message(self, message_type: str, component_type: str, 
                      component_name: str, command_name: str, value_name: str = None, 
                      value: int = None) -> Tuple[Optional[int], Optional[int], Optional[int], 
                                                 Optional[int], Optional[int], Optional[int]]:
        """Create a complete message tuple from high-level parameters"""
        msg_type = self.get_message_type(message_type)
        comp_type = self.get_component_type(component_type)
        comp_id = self.get_component_id(component_type, component_name)
        cmd_id = self.get_command_id(component_type, command_name)
        
        # Handle value - either named value or direct integer
        val_type = self.get_value_type("INT8")  # Default
        val = 0
        
        value_by_name = self.get_command_value(component_type, command_name, value_name)
        if value_by_name:
            val = value_by_name
        elif value is not None:
            val = value
        
        return (msg_type, comp_type, comp_id, cmd_id, val_type, val)
    
    def get_message_type_name(self, message_type: int) -> str:
        """Get message type name by value"""
        # iterate over the values in the message_types dictionary
        for key, value in self.registry['message_types'].items():
            if value == message_type:
                return key
        return None
    
    def get_component_type_name(self, component_type: int) -> str:
        """Get component type name by value"""
        # iterate over the values in the component_types dictionary
        for key, value in self.registry['component_types'].items():
            if value == component_type:
                return key
        return None
    
    def get_component_id_name(self, component_type: str, component_id: int) -> str:
        """Get component ID name by value"""
        component_type_name = self.get_component_type_name(component_type).lower()
        components_by_type = self.registry['components'].get(component_type_name)

        # iterate over the values in the components_by_type dictionary
        for key, value in components_by_type.items():
            if value == component_id:
                return key
        return None
    
    def get_command_name(self, component_type: str, command_id: int) -> str:
        """Get command name by value"""
        component_type_name = self.get_component_type_name(component_type).lower()

        # iterate over the commands for this types and check the id value matches the command id
        for key, value in self.registry['commands'][component_type_name].items():
            if value['id'] == command_id:
                return key
        return None
    
    def get_command_value_name(self, component_type: str, command_id: int, value_id: int) -> str:
        """Get command value name by value"""
        component_type_name = self.get_component_type_name(component_type).lower()
        command_name = self.get_command_name(component_type, command_id)

        # iterate over the values in the command's values dictionary
        for key, value in self.registry['commands'][component_type_name][command_name.upper()]['values'].items():
            if value == value_id:
                return key
        return None
    
    def get_value_type_name(self, value_type: int) -> str:
        """Get value type name by value"""
        # iterate over the values in the value_types dictionary
        for key, value in self.registry['value_types'].items():
            if value == value_type:
                return key
        return None
    
    def dump_registry(self) -> Dict:
        """Return a copy of the complete registry"""
        return self.registry.copy()