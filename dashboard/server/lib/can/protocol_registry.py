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
        self.logger = logging.getLogger(self.__class__.__name__)
        self.pb_path = self._resolve_protocol_path(pb_path)
        self.modules = {}
        self.registry = {}
        self._load_modules()
        self._build_registry()
        
    def _resolve_protocol_path(self, pb_path: str = None) -> str:
        """Intelligently find the protocol directory"""
        
        # If a path is provided and exists, use it
        if pb_path and os.path.exists(pb_path):
            self.logger.info(f"Using provided protocol path: {pb_path}")
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
        ]
        
        # Try each path
        for path in possible_paths:
            if os.path.exists(path):
                self.logger.info(f"Found protocol directory at: {path}")
                return path
                
        # If we get here, just return the provided path or default
        # The _load_modules will handle the error case
        self.logger.warning(f"Could not find protocol directory, using default: {pb_path or './protocol/generated/python'}")
        return pb_path or "./protocol/generated/python"
        
    def _load_modules(self) -> None:
        """Discover and load all Protocol Buffer modules"""
        
        self.logger.info(f"Adding protocol path to sys.path: {self.pb_path}")
        sys.path.append(self.pb_path)
        
        # Check if the directory exists
        if not os.path.exists(self.pb_path):
            self.logger.error(f"Protocol directory does not exist: {self.pb_path}")
            return
            
        # List modules in the directory
        modules_in_dir = [name for _, name, _ in pkgutil.iter_modules([self.pb_path])]
        self.logger.info(f"Found modules in {self.pb_path}: {modules_in_dir}")
        
        for _, name, _ in pkgutil.iter_modules([self.pb_path]):
            if name.endswith('_pb2'):
                try:
                    self.modules[name] = importlib.import_module(name)
                    self.logger.info(f"Loaded protocol module: {name}")
                except ImportError as e:
                    self.logger.error(f"Failed to load {name}: {e}")
                    # Try with absolute import
                    try:
                        full_name = f"{os.path.basename(self.pb_path)}.{name}"
                        self.logger.info(f"Trying absolute import: {full_name}")
                        self.modules[name] = importlib.import_module(full_name)
                        self.logger.info(f"Loaded protocol module with absolute import: {name}")
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
        self.logger.info(f"Extracted message_types: {self.registry['message_types']}")
        self.logger.info(f"Extracted component_types: {self.registry['component_types']}")
        self.logger.info(f"Extracted value_types: {self.registry['value_types']}")
    
    def _extract_common_enums(self, module: Any) -> None:
        """Extract common message, component, and value type enums"""
        try:
            # Direct extraction of well-known enums
            if hasattr(module, 'MessageType') and hasattr(module.MessageType, 'items'):
                self.registry['message_types'] = dict(module.MessageType.items())
                self.logger.info(f"Extracted MessageType enum: {self.registry['message_types']}")
                
            if hasattr(module, 'ComponentType') and hasattr(module.ComponentType, 'items'):
                self.registry['component_types'] = dict(module.ComponentType.items())
                self.logger.info(f"Extracted ComponentType enum: {self.registry['component_types']}")
                
            if hasattr(module, 'ValueType') and hasattr(module.ValueType, 'items'):
                self.registry['value_types'] = dict(module.ValueType.items())
                self.logger.info(f"Extracted ValueType enum: {self.registry['value_types']}")
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
            
            # Extract ComponentId enum if it exists
            if hasattr(module, 'ComponentId') and hasattr(module.ComponentId, 'items'):
                self.registry['components'][component_type] = dict(module.ComponentId.items())
                self.logger.info(f"Extracted {component_type} ComponentId enum: {self.registry['components'][component_type]}")
                
            # Extract CommandId enum if it exists
            if hasattr(module, 'CommandId') and hasattr(module.CommandId, 'items'):
                command_ids = dict(module.CommandId.items())
                
                # Create command entries
                for cmd_name, cmd_id in command_ids.items():
                    if cmd_name not in self.registry['commands'][component_type]:
                        self.registry['commands'][component_type][cmd_name] = {
                            'id': cmd_id,
                            'values': {}
                        }
                    else:
                        self.registry['commands'][component_type][cmd_name]['id'] = cmd_id
                        
                self.logger.info(f"Extracted {component_type} CommandId enum with {len(command_ids)} commands")
                
            # Look for command-specific value enums
            for attr_name in dir(module):
                if attr_name.endswith('Value') and hasattr(module, attr_name) and hasattr(getattr(module, attr_name), 'items'):
                    value_enum = getattr(module, attr_name)
                    cmd_name = attr_name.replace('Value', '')
                    
                    # Find matching command
                    for command in self.registry['commands'][component_type].keys():
                        if command.upper() == cmd_name.upper():
                            self.registry['commands'][component_type][command]['values'] = dict(value_enum.items())
                            self.logger.info(f"Added values for command {command}: {self.registry['commands'][component_type][command]['values']}")
                            break
        except Exception as e:
            self.logger.error(f"Error extracting component enums: {e}")
    
    def _determine_component_type(self, module: Any) -> Optional[str]:
        """Determine component type from module name or content"""
        module_name = module.__name__.lower()
        
        # If common module, return None (it's not a component module)
        if 'common' in module_name:
            return None
            
        # Extract from module name
        for comp_type_name, comp_type_value in self.registry['component_types'].items():
            if comp_type_name.lower() in module_name:
                self.logger.info(f"Determined component type {comp_type_name} from module name {module_name}")
                return comp_type_name.lower()
        
        # If we couldn't determine from the name, check if there's a ComponentId enum
        if hasattr(module, 'ComponentId'):
            # Try to find a matching component type
            for comp_type_name in self.registry['component_types'].keys():
                if comp_type_name.lower() in module_name:
                    self.logger.info(f"Determined component type {comp_type_name} from ComponentId in {module_name}")
                    return comp_type_name.lower()
            
            # If we got here, use the module name without _pb2
            comp_type = module_name.replace('_pb2', '').lower()
            self.logger.info(f"Using module name as component type: {comp_type}")
            return comp_type
        
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
        return self.registry['component_types'].get(type_name)
    
    def get_value_type(self, type_name: str) -> Optional[int]:
        """Get value type by name"""
        return self.registry['value_types'].get(type_name)
    
    def get_component_id(self, component_type: str, component_name: str) -> Optional[int]:
        """Get component ID by type and name"""
        component_type = component_type.lower()
        if component_type in self.registry['components']:
            return self.registry['components'][component_type].get(component_name)
        return None
    
    def get_command_id(self, component_type: str, command_name: str) -> Optional[int]:
        """Get command ID by component type and command name"""
        component_type = component_type.lower()
        if (component_type in self.registry['commands'] and 
            command_name in self.registry['commands'][component_type]):
            return self.registry['commands'][component_type][command_name]['id']
        return None
    
    def get_command_value(self, component_type: str, command_name: str, value_name: str) -> Optional[int]:
        """Get command value by component type, command name, and value name"""
        component_type = component_type.lower()
        if (component_type in self.registry['commands'] and 
            command_name in self.registry['commands'][component_type] and 
            'values' in self.registry['commands'][component_type][command_name]):
            return self.registry['commands'][component_type][command_name]['values'].get(value_name)
        return None
    
    def get_component_types(self) -> List[str]:
        """Get all registered component types"""
        return list(self.registry['components'].keys())
    
    def get_commands(self, component_type: str) -> List[str]:
        """Get all commands for a component type"""
        component_type = component_type.lower()
        if component_type in self.registry['commands']:
            return list(self.registry['commands'][component_type].keys())
        return []
    
    def get_command_values(self, component_type: str, command_name: str) -> Dict[str, int]:
        """Get all possible values for a command"""
        component_type = component_type.lower()
        if (component_type in self.registry['commands'] and 
            command_name in self.registry['commands'][component_type] and 
            'values' in self.registry['commands'][component_type][command_name]):
            return self.registry['commands'][component_type][command_name]['values']
        return {}
    
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
        val_type = self.get_value_type("INT32")  # Default
        val = 0
        
        if value_name and not value:
            val = self.get_command_value(component_type, command_name, value_name)
            # Try to determine value type from context
            for type_name, type_id in self.registry['value_types'].items():
                if type_name.startswith(("UINT", "INT", "BOOL")):
                    val_type = type_id
                    break
        elif value is not None:
            val = value
        
        return (msg_type, comp_type, comp_id, cmd_id, val_type, val)
    
    def dump_registry(self) -> Dict:
        """Return a copy of the complete registry"""
        return self.registry.copy()