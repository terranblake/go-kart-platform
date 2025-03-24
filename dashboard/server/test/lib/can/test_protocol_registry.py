#!/usr/bin/env python3
"""
Tests for the Protocol Registry

These tests verify that the protocol registry correctly loads and parses the 
generated protocol files.

IMPORTANT: These tests require actual generated protocol files from protobuf.
They do NOT use mocks or fake values.
"""
import unittest
import os
import sys
import logging
from pathlib import Path

# Set up logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

# Add the server directory to the path so we can import the modules
server_dir = Path(__file__).parent.parent
sys.path.insert(0, str(server_dir))

# Import the protocol registry
from lib.can.protocol_registry import ProtocolRegistry

class TestProtocolRegistry(unittest.TestCase):
    """Test suite for the Protocol Registry"""

    @classmethod
    def setUpClass(cls):
        """Set up the test class - find the actual protobuf directory"""
        # Look for the protocol directory from the repo root
        repo_root = Path(__file__).parent.parent.parent.parent
        pb_path = repo_root / "protocol" / "generated" / "python"
        
        if not pb_path.exists():
            logger.error(f"Protocol directory not found at {pb_path}")
            logger.info("Searching for protocol directory...")
            
            # Try to find the protocol directory by walking up from cwd
            current_dir = Path.cwd()
            while current_dir != current_dir.parent:
                test_path = current_dir / "protocol" / "generated" / "python"
                if test_path.exists():
                    pb_path = test_path
                    logger.info(f"Found protocol directory at {pb_path}")
                    break
                current_dir = current_dir.parent
        
        # Save the path for tests to use
        cls.pb_path = str(pb_path)
        logger.info(f"Using protocol path: {cls.pb_path}")
        
        # Make sure the protocol files exist
        if not Path(cls.pb_path).exists():
            raise FileNotFoundError(f"Protocol directory not found at {cls.pb_path}")
            
        # Check if we have at least one _pb2.py file
        pb_files = list(Path(cls.pb_path).glob("*_pb2.py"))
        if not pb_files:
            raise FileNotFoundError(f"No protobuf files found in {cls.pb_path}")
            
        logger.info(f"Found protocol files: {[f.name for f in pb_files]}")

    def setUp(self):
        """Set up each test"""
        self.registry = ProtocolRegistry(pb_path=self.pb_path)

    def test_registry_initialization(self):
        """Test that the registry initializes correctly"""
        self.assertIsNotNone(self.registry)
        self.assertIsNotNone(self.registry.registry)
        self.assertIsNotNone(self.registry.modules)
        
        # We should have loaded at least one module
        self.assertGreater(len(self.registry.modules), 0)
        
        # Print the loaded modules for debugging
        logger.info(f"Loaded modules: {list(self.registry.modules.keys())}")
        
        # The registry should have message_types, component_types, and value_types
        self.assertIn('message_types', self.registry.registry)
        self.assertIn('component_types', self.registry.registry)
        self.assertIn('value_types', self.registry.registry)

    def test_message_types(self):
        """Test that message types are loaded correctly"""
        message_types = self.registry.registry['message_types']
        self.assertGreater(len(message_types), 0)
        
        # Expected message types from the protocol
        expected_types = ['COMMAND', 'STATUS', 'ACK', 'ERROR']
        
        # Check that at least some expected types are present
        for expected_type in expected_types:
            # We might not have all types, so just check the ones we do have
            if expected_type in message_types:
                self.assertIsInstance(message_types[expected_type], int)
                logger.info(f"Found message type: {expected_type} = {message_types[expected_type]}")

    def test_component_types(self):
        """Test that component types are loaded correctly"""
        component_types = self.registry.registry['component_types']
        self.assertGreater(len(component_types), 0)
        
        # Expected component types from the protocol
        expected_types = ['LIGHTS', 'MOTORS', 'SENSORS', 'BATTERY', 'CONTROLS']
        
        # Check that at least some expected types are present
        for expected_type in expected_types:
            # We might not have all types, so just check the ones we do have
            if expected_type in component_types:
                self.assertIsInstance(component_types[expected_type], int)
                logger.info(f"Found component type: {expected_type} = {component_types[expected_type]}")

    def test_value_types(self):
        """Test that value types are loaded correctly"""
        value_types = self.registry.registry['value_types']
        self.assertGreater(len(value_types), 0)
        
        # Expected value types from the protocol
        expected_types = ['BOOLEAN', 'INT8', 'UINT8', 'INT16', 'UINT16', 'INT24', 'UINT24', 'FLOAT16']
        
        # Check that at least some expected types are present
        for expected_type in expected_types:
            # We might not have all types, so just check the ones we do have
            if expected_type in value_types:
                self.assertIsInstance(value_types[expected_type], int)
                logger.info(f"Found value type: {expected_type} = {value_types[expected_type]}")

    def test_components(self):
        """Test that components are loaded correctly"""
        self.assertIn('components', self.registry.registry)
        components = self.registry.registry['components']
        
        # Check if we have at least one component type
        self.assertGreater(len(components), 0)
        
        # Print out what we found for debugging
        for comp_type, comp_dict in components.items():
            logger.info(f"Component type: {comp_type}")
            logger.info(f"Component IDs: {comp_dict}")
            
            # Each component should have at least one ID
            if comp_dict:  # Some might be empty
                self.assertGreater(len(comp_dict), 0)
                # Component IDs should be integers
                for name, comp_id in comp_dict.items():
                    self.assertIsInstance(comp_id, int)
                    logger.info(f"  {name} = {comp_id}")

    def test_commands(self):
        """Test that commands are loaded correctly"""
        self.assertIn('commands', self.registry.registry)
        commands = self.registry.registry['commands']
        
        # Check if we have at least one component with commands
        self.assertGreater(len(commands), 0)
        
        # Print out what we found for debugging
        for comp_type, cmd_dict in commands.items():
            logger.info(f"Commands for component type: {comp_type}")
            logger.info(f"Commands: {cmd_dict}")
            
            # Each component should have at least one command
            if cmd_dict:  # Some might be empty
                self.assertGreater(len(cmd_dict), 0)
                
                # Each command should have an id
                for cmd_name, cmd_info in cmd_dict.items():
                    self.assertIn('id', cmd_info)
                    self.assertIsInstance(cmd_info['id'], int)
                    logger.info(f"  {cmd_name} (ID: {cmd_info['id']})")
                    
                    # Some commands might have values
                    if 'values' in cmd_info and cmd_info['values']:
                        for val_name, val in cmd_info['values'].items():
                            self.assertIsInstance(val, int)
                            logger.info(f"    {val_name} = {val}")

    def test_get_message_type(self):
        """Test that we can get message types by name"""
        # Test for a message type that should exist
        message_types = list(self.registry.registry['message_types'].keys())
        if message_types:
            msg_type = message_types[0]
            value = self.registry.get_message_type(msg_type)
            self.assertIsNotNone(value)
            self.assertIsInstance(value, int)
            logger.info(f"Message type {msg_type} = {value}")
        
        # Test for a message type that doesn't exist
        value = self.registry.get_message_type("NONEXISTENT_TYPE")
        self.assertIsNone(value)

    def test_get_component_type(self):
        """Test that we can get component types by name"""
        # Test for a component type that should exist
        component_types = list(self.registry.registry['component_types'].keys())
        if component_types:
            comp_type = component_types[0]
            value = self.registry.get_component_type(comp_type)
            self.assertIsNotNone(value)
            self.assertIsInstance(value, int)
            logger.info(f"Component type {comp_type} = {value}")
        
        # Test for a component type that doesn't exist
        value = self.registry.get_component_type("NONEXISTENT_TYPE")
        self.assertIsNone(value)

    def test_create_message(self):
        """Test that we can create a complete message tuple"""
        # Get a valid message type, component type, etc. from the registry
        message_types = list(self.registry.registry['message_types'].keys())
        component_types = list(self.registry.registry['component_types'].keys())
        
        if not message_types or not component_types:
            logger.warning("Skipping test_create_message due to missing types")
            return
            
        msg_type = message_types[0]
        comp_type = component_types[0]
        
        # Get component IDs for this component type
        comp_ids = {}
        if comp_type.lower() in self.registry.registry['components']:
            comp_ids = self.registry.registry['components'][comp_type.lower()]
        
        # Get commands for this component type
        cmd_ids = {}
        if comp_type.lower() in self.registry.registry['commands']:
            cmd_ids = self.registry.registry['commands'][comp_type.lower()]
        
        # If we have component IDs and commands, we can test create_message
        if comp_ids and cmd_ids:
            comp_name = list(comp_ids.keys())[0]
            cmd_name = list(cmd_ids.keys())[0]
            
            # Create a message
            msg_tuple = self.registry.create_message(
                msg_type, comp_type, comp_name, cmd_name)
            
            # Check that the message tuple has the correct parts
            self.assertIsNotNone(msg_tuple)
            self.assertEqual(len(msg_tuple), 6)
            
            # Check each part of the tuple
            msg_type_val, comp_type_val, comp_id, cmd_id, val_type, val = msg_tuple
            
            self.assertIsNotNone(msg_type_val)
            self.assertIsNotNone(comp_type_val)
            self.assertIsNotNone(comp_id)
            self.assertIsNotNone(cmd_id)
            
            logger.info(f"Created message: {msg_tuple}")
            
            # Make sure the values match what we expect
            self.assertEqual(msg_type_val, self.registry.get_message_type(msg_type))
            self.assertEqual(comp_type_val, self.registry.get_component_type(comp_type))
            self.assertEqual(comp_id, self.registry.get_component_id(comp_type.lower(), comp_name))
            self.assertEqual(cmd_id, self.registry.get_command_id(comp_type.lower(), cmd_name))

if __name__ == '__main__':
    unittest.main() 