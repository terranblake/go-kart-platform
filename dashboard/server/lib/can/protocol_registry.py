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

class ProtocolRegistry:
    """Dynamic registry for Protocol Buffer definitions that adapts to new component types"""
    
    def __init__(self, pb_path: str = "./protocol/generated/python"):
        self.logger = logging.getLogger(self.__class__.__name__)
        self.pb_path = pb_path
        self.modules = {}
        self.registry = {}
        self._load_modules()
        self._build_registry()
        
    def _load_modules(self) -> None:
        """Discover and load all Protocol Buffer modules"""
        sys.path.append(self.pb_path)
        
        for _, name, _ in pkgutil.iter_modules([self.pb_path]):
            if name.endswith('_pb2'):
                try:
                    self.modules[name] = importlib.import_module(name)
                    self.logger.debug(f"Loaded protocol module: {name}")
                except ImportError as e:
                    self.logger.error(f"Failed to load {name}: {e}")
    
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
    
    def _extract_common_enums(self, module: Any) -> None:
        """Extract common message, component, and value type enums"""
        common_enums = self._extract_all_enums(module)
        
        for path, enum_dict in common_enums.items():
            if 'MessageType' in path:
                self.registry['message_types'] = enum_dict
            elif 'ComponentType' in path:
                self.registry['component_types'] = enum_dict
            elif 'ValueType' in path:
                self.registry['value_types'] = enum_dict
    
    def _extract_component_enums(self, module: Any) -> None:
        """Extract component-specific enums (component IDs, command IDs, values)"""
        component_enums = self._extract_all_enums(module)
        
        # Determine component type from module name or content
        component_type = self._determine_component_type(module)
        if not component_type:
            return
            
        # Initialize component structures if needed
        if component_type not in self.registry['components']:
            self.registry['components'][component_type] = {}
        if component_type not in self.registry['commands']:
            self.registry['commands'][component_type] = {}
        
        # Process component-specific enums
        for path, enum_dict in component_enums.items():
            if 'ComponentId' in path:
                self.registry['components'][component_type] = enum_dict
            elif 'CommandId' in path:
                # Extract command definitions and prepare command structure
                for cmd_name, cmd_id in enum_dict.items():
                    if cmd_name not in self.registry['commands'][component_type]:
                        self.registry['commands'][component_type][cmd_name] = {
                            'id': cmd_id,
                            'values': {}
                        }
                    else:
                        self.registry['commands'][component_type][cmd_name]['id'] = cmd_id
        
        # Second pass to find command values
        for path, enum_dict in component_enums.items():
            if 'Value' in path and 'ValueType' not in path:
                # Try to match value enum to a command
                for cmd_name in self.registry['commands'][component_type]:
                    if cmd_name in path:
                        self.registry['commands'][component_type][cmd_name]['values'] = enum_dict
                        break
    
    def _determine_component_type(self, module: Any) -> Optional[str]:
        """Determine component type from module name or content"""
        module_name = module.__name__.lower()
        
        # Extract from module name
        for comp_type in self.registry['component_types']:
            if comp_type.lower() in module_name:
                return comp_type.lower()
        
        # Try to extract from enums inside module
        enums = self._extract_all_enums(module)
        for path in enums:
            for comp_type in self.registry['component_types']:
                if comp_type.lower() in path.lower():
                    return comp_type.lower()
        
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