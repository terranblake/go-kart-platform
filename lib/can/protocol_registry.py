import os
import importlib
import pkgutil
import sys
import re
import logging
from typing import Dict, Any, Optional, List, Set, Tuple

class ProtocolRegistry:
    """Dynamic registry for Protocol Buffer definitions"""
    
    def __init__(self):
        self.logger = logging.getLogger(self.__class__.__name__)
        # Get the absolute path to the protocol files
        base_dir = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
        self.pb_path = os.path.join(os.path.dirname(base_dir), "protocol", "generated", "python")
        self.logger.debug(f"Loading modules from: {self.pb_path}")

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
        all_enums = self._extract_all_enums()
        self._organize_registry(all_enums)
    
    def _extract_all_enums(self) -> Dict[str, Dict]:
        """Extract all enum definitions from loaded modules"""
        all_enums = {}
        
        for module_name, module in self.modules.items():
            enums = self._walk_object(module)
            all_enums.update(enums)
            
        return all_enums
    
    def _walk_object(self, obj: Any, path: str = "") -> Dict[str, Dict]:
        """Recursively walk object to find and extract enums"""
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
                    child_results = self._walk_object(attr, new_path)
                    results.update(child_results)
            except Exception as e:
                self.logger.debug(f"Error exploring {name}: {e}")
                
        return results
    
    def _organize_registry(self, all_enums: Dict[str, Dict]) -> None:
        """Organize enums into structured registry"""
        # Initialize registry structure
        self.registry = {
            'message_types': {},
            'component_types': {},
            'value_types': {},
            'components': {},
            'commands': {}
        }
        
        # Identify enum categories and organize them
        for path, enum_dict in all_enums.items():
            path_parts = path.split('.')
            
            # Extract possible component name
            component_candidates = [p.lower() for p in path_parts 
                                   if p.lower() in ['lights', 'controls', 'power']]
            component = component_candidates[0] if component_candidates else 'common'
            
            # Categorize based on name patterns
            if 'MessageType' in path:
                self.registry['message_types'] = enum_dict
            elif 'ComponentType' in path:
                self.registry['component_types'] = enum_dict
            elif 'ValueType' in path:
                self.registry['value_types'] = enum_dict
            elif 'ComponentId' in path:
                if component not in self.registry['components']:
                    self.registry['components'][component] = {}
                self.registry['components'][component] = enum_dict
            elif 'CommandId' in path:
                if component not in self.registry['commands']:
                    self.registry['commands'][component] = {}
                
                # Extract command category name
                command_name = path_parts[-1].replace('CommandId', '')
                
                if command_name not in self.registry['commands'][component]:
                    self.registry['commands'][component][command_name] = {'id': None, 'values': {}}
                
                # Store command IDs
                self.registry['commands'][component][command_name]['id'] = enum_dict
                
                # Look for corresponding value enums
                for value_path, value_enum in all_enums.items():
                    if command_name in value_path and 'Value' in value_path:
                        self.registry['commands'][component][command_name]['values'] = value_enum
    
    def get_message_type(self, type_name: str) -> Optional[int]:
        """Get message type value by name"""
        try:
            return self.registry['message_types'].get(type_name)
        except (KeyError, ValueError):
            self.logger.error(f"Invalid message type: {type_name}")
            return None
    
    def get_component_type(self, type_name: str) -> Optional[int]:
        """Get component type value by name"""
        try:
            return self.registry['component_types'].get(type_name)
        except (KeyError, ValueError):
            self.logger.error(f"Invalid component type: {type_name}")
            return None
    
    def get_value_type(self, type_name: str) -> Optional[int]:
        """Get value type by name"""
        try:
            return self.registry['value_types'].get(type_name)
        except (KeyError, ValueError):
            self.logger.error(f"Invalid value type: {type_name}")
            return None
    
    def get_component_id(self, component_type: str, component_name: str) -> Optional[int]:
        """Get component ID by type and name"""
        try:
            return self.registry['components'][component_type.lower()].get(component_name)
        except (KeyError, ValueError):
            self.logger.error(f"Invalid component: {component_type}.{component_name}")
            return None
    
    def get_command_id(self, component_type: str, command_name: str) -> Optional[int]:
        """Get command ID by component type and command name"""
        try:
            return self.registry['commands'][component_type.lower()][command_name]['id']
        except (KeyError, ValueError):
            self.logger.error(f"Invalid command: {component_type}.{command_name}")
            return None
    
    def get_command_value(self, component_type: str, command_name: str, value_name: str) -> Optional[int]:
        """Get command value by component type, command name, and value name"""
        try:
            return self.registry['commands'][component_type.lower()][command_name]['values'].get(value_name)
        except (KeyError, ValueError):
            self.logger.error(f"Invalid command value: {component_type}.{command_name}.{value_name}")
            return None
    
    def get_command_values(self, component_type: str, command_name: str) -> Dict[str, int]:
        """Get all possible values for a command by component type and command name"""
        try:
            return self.registry['commands'][component_type.lower()][command_name]['values']
        except KeyError:
            self.logger.error(f"Invalid command: {component_type}.{command_name}")
            return {}
    
    def get_message_header(self, component_type: str, command_name: str) -> Optional[int]:
        """Generate message header by combining component type and command"""
        comp_type_id = self.get_component_type(component_type)
        cmd_id = self.get_command_id(component_type, command_name)
        
        if comp_type_id is None or cmd_id is None:
            return None
            
        return (comp_type_id << 4) | cmd_id
    
    def get_all_component_types(self) -> List[str]:
        """Get list of all component types"""
        return list(self.registry['component_types'].keys())
    
    def get_all_components(self, component_type: str) -> List[str]:
        """Get list of all components for a specific type"""
        try:
            return list(self.registry['components'][component_type.lower()].keys())
        except KeyError:
            return []
    
    def get_all_commands(self, component_type: str) -> List[str]:
        """Get list of all commands for a specific component type"""
        try:
            return list(self.registry['commands'][component_type.lower()].keys())
        except KeyError:
            return []
        
    def get_component_by_name(self, component_name: str) -> Optional[Dict[str, Any]]:
        """Get component by name"""
        for _, components in self.registry['components'].items():
            if component_name in components:
                return components[component_name]
        return None
    
    def get_command_by_name(self, component_name: str, command_name: str) -> Optional[Dict[str, Any]]:
        """Get command by name"""
        for _, commands in self.registry['commands'].items():
            if component_name in commands:
                return commands[command_name]
        return None
    
    def create_message(self, component_type: str, command_name: str, value_name: str, value: int) -> Optional[bytes]:
        """Create a message from a component type, command name, value name, and value"""
        comp_type_id = self.get_component_type(component_type)
        cmd_id = self.get_command_id(component_type, command_name)
        value_type_id = self.get_value_type(value_name)
        
        if comp_type_id is None or cmd_id is None or value_type_id is None:
            return None
        
        header = self.get_message_header(component_type, command_name)
        if header is None:
            return None
        
        # Create message payload
        payload = struct.pack('>I', header)
        payload += struct.pack('>I', value_type_id)
        payload += struct.pack('>I', value)
        
        return payload
    
    # this needs to match the function at interface.py
    def create_message(self, message_type: str, component_type: str, component_name: str, command_name: str, value_name: str, value: int) -> Optional[bytes]:
        int_message_type = self.get_message_type(message_type)
        int_comp_type = self.get_component_type(component_type)
        int_comp_id = self.get_component_id(component_type, component_name)
        int_cmd_id = self.get_command_id(component_type, command_name)
        int_value_type = self.get_value_type(value_name)

        message = [int_message_type, int_comp_type, int_comp_id, int_cmd_id, int_value_type, value]
        return message
        
        
        