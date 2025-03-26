#!/usr/bin/env python3
"""
Unit tests for the CAN interface binary data functionality
"""

import unittest
import logging
import time
import sys
import os
from pathlib import Path
from unittest.mock import patch, MagicMock

# Configure logging
logging.basicConfig(level=logging.INFO,
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Add the server directory to the path so we can import the modules
server_dir = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(server_dir))

# Import the CAN interface wrapper
try:
    from lib.can.interface import CANInterfaceWrapper, MockCANInterface
except ImportError as e:
    logger.error(f"Failed to import CAN interface: {e}")
    raise

class TestBinaryDataTransmission(unittest.TestCase):
    """Test suite for binary data transmission through the CAN Interface"""

    def setUp(self):
        """Set up each test"""
        # Create a CAN interface with a mocked low-level interface
        with patch('lib.can.interface.ProtocolRegistry') as MockProtocolRegistry:
            # Setup mock protocol registry with necessary values
            mock_registry = MagicMock()
            mock_registry.registry = {
                'message_types': {'COMMAND': 0, 'STATUS': 1},
                'component_types': {'LIGHTS': 2, 'MOTORS': 3},
                'value_types': {'BOOLEAN': 1, 'UINT8': 4, 'INT8': 5}
            }
            mock_registry.get_component_type.return_value = 2  # LIGHTS
            mock_registry.get_component_id.return_value = 255  # ALL
            mock_registry.get_command_id.return_value = 10  # ANIMATION_FRAME
            MockProtocolRegistry.return_value = mock_registry
            
            # Create the interface with our mocked registry
            self.can_interface = CANInterfaceWrapper(node_id=0x01, channel='can0', baudrate=500000)
            self.can_interface.protocol_registry = mock_registry
            
            # Replace the _can_interface with our own mock
            self.mock_interface = MagicMock()
            self.can_interface._can_interface = self.mock_interface
            self.can_interface.has_can_hardware = False  # Force use of mock interface

    def tearDown(self):
        """Clean up after each test"""
        if hasattr(self, 'can_interface'):
            self.can_interface.stop_processing()
            # Give it time to shut down
            time.sleep(0.1)

    def test_send_binary_data(self):
        """Test sending binary data through the interface"""
        # Create some test binary data
        test_data = bytes([0x01, 0x02, 0x03, 0x04, 0x05])
        
        # Set up the mock to return success
        self.mock_interface.send_binary_data.return_value = True
        
        # Send binary data through the high-level API
        result = self.can_interface.send_binary_data(
            component_type_name='LIGHTS',
            component_name='ALL',
            command_name='ANIMATION_FRAME',
            value_type='UINT8',
            data=test_data
        )
        
        # Verify the result
        self.assertTrue(result)
        
        # Verify the mock was called with expected parameters
        self.mock_interface.send_binary_data.assert_called_once()
        
        # Get the call arguments for verification
        args = self.mock_interface.send_binary_data.call_args[0]
        
        # Check that message type is COMMAND (0)
        self.assertEqual(args[0], 0)  # COMMAND message type
        
        # Check that component type is LIGHTS (2)
        self.assertEqual(args[1], 2)  # LIGHTS component type
        
        # Component ID for ALL should be 255
        self.assertEqual(args[2], 255)  # ALL component ID
        
        # Check command ID for ANIMATION_FRAME (10)
        self.assertEqual(args[3], 10)  # ANIMATION_FRAME command ID
        
        # Check value type is UINT8 (4)
        self.assertEqual(args[4], 4)  # UINT8 value type
        
        # Check data content
        self.assertEqual(len(args[5]), len(test_data))

    def test_register_binary_handler(self):
        """Test registering a binary data handler"""
        # Create a test handler function
        def test_handler(msg_type, comp_type, comp_id, cmd_id, val_type, data, data_size):
            pass
        
        # Mock the register_binary_handler method
        self.mock_interface.register_binary_handler.return_value = True
        
        # Register the handler
        with patch.dict(self.can_interface.protocol_registry.registry["message_types"], {"COMMAND": 0}):
            result = self.can_interface.register_binary_handler(
                message_type='COMMAND',
                comp_type=2,  # LIGHTS
                comp_id=255,  # ALL
                cmd_id=10,    # ANIMATION_FRAME
                handler=test_handler
            )
        
        # Verify registration was successful
        self.assertTrue(result)
        
        # Verify the mock was called
        self.mock_interface.register_binary_handler.assert_called_once()

    def test_mock_interface_binary_data(self):
        """Test that the MockCANInterface properly handles binary data"""
        # Create a MockCANInterface directly
        mock_interface = MockCANInterface(node_id=0x01)
        
        # Test registering a binary handler
        def test_handler(msg_type, comp_type, comp_id, cmd_id, val_type, data, data_size):
            pass
        
        result = mock_interface.register_binary_handler(2, 255, 10, test_handler)
        self.assertTrue(result)
        
        # Verify the handler was stored
        self.assertIn((2, 255, 10), mock_interface.binary_handlers)
        
        # Test sending binary data
        test_data = bytes([0x01, 0x02, 0x03, 0x04, 0x05])
        result = mock_interface.send_binary_data(0, 2, 255, 10, 4, test_data, len(test_data))
        self.assertTrue(result)

if __name__ == '__main__':
    unittest.main() 