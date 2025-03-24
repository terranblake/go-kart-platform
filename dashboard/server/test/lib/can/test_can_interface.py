#!/usr/bin/env python3
"""
Tests for the CAN Interface Wrapper

These tests verify that the CAN interface correctly interacts with the hardware
on the Raspberry Pi. These tests require actual hardware and should be run
on the Raspberry Pi itself.

IMPORTANT: These tests use the actual hardware and do NOT use mocks.
They require:
  1. A Raspberry Pi with can0 interface configured
  2. The CrossPlatformCAN library built and installed
  3. The libcaninterface.so shared library in lib/can directory
  4. The actual generated protocol files from protobuf

To run these tests on the Raspberry Pi:
  1. SSH into the Raspberry Pi
  2. Navigate to the dashboard/server directory
  3. Run: python3 -m tests.test_can_interface
"""
import unittest
import os
import sys
import logging
import time
import subprocess
from pathlib import Path

# Set up logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)

# Check if we're running on a Raspberry Pi
def is_raspberry_pi():
    """Check if we're running on a Raspberry Pi"""
    try:
        with open('/proc/cpuinfo', 'r') as f:
            cpuinfo = f.read()
        return 'Raspberry Pi' in cpuinfo or 'BCM' in cpuinfo
    except:
        return False

# Check for the CAN interface
def check_can_interface():
    """Check if the can0 interface is available"""
    try:
        output = subprocess.check_output(['ip', 'link', 'show', 'can0'], stderr=subprocess.STDOUT).decode()
        return 'can0' in output
    except subprocess.CalledProcessError:
        return False

# Add the server directory to the path so we can import the modules
server_dir = Path(__file__).parent.parent
sys.path.insert(0, str(server_dir))

# Import the CAN interface wrapper and protocol registry
try:
    from lib.can.interface import CANInterfaceWrapper
    from lib.can.protocol_registry import ProtocolRegistry
except ImportError as e:
    logger.error(f"Failed to import CAN interface: {e}")
    raise

# Skip tests if not on a Raspberry Pi or no CAN interface
skip_tests = not is_raspberry_pi() or not check_can_interface()
skip_reason = "Not running on Raspberry Pi or can0 interface not available"

@unittest.skipIf(skip_tests, skip_reason)
class TestCANInterface(unittest.TestCase):
    """Test suite for the CAN Interface"""

    @classmethod
    def setUpClass(cls):
        """Set up the test class - reset the CAN interface"""
        if skip_tests:
            return
            
        try:
            # Reset the CAN interface to ensure it's in a good state
            subprocess.run(['sudo', 'ip', 'link', 'set', 'can0', 'down'], check=True)
            time.sleep(0.5)
            subprocess.run(['sudo', 'ip', 'link', 'set', 'can0', 'up', 'type', 'can', 'bitrate', '500000'], check=True)
            time.sleep(0.5)
            logger.info("CAN interface reset successfully")
        except subprocess.CalledProcessError as e:
            logger.error(f"Failed to reset CAN interface: {e}")
            cls.skipTest(cls, "Failed to reset CAN interface")
            
        # Find the protocol directory
        repo_root = Path(__file__).parent.parent.parent.parent
        cls.pb_path = repo_root / "protocol" / "generated" / "python"
        
        if not cls.pb_path.exists():
            logger.warning(f"Protocol directory not found at {cls.pb_path}")
            # Use the default path from the protocol registry
            cls.pb_path = "./protocol/generated/python"
        else:
            cls.pb_path = str(cls.pb_path)
            
        logger.info(f"Using protocol path: {cls.pb_path}")
        
        # Make sure the CAN interface library exists
        lib_path = Path(server_dir) / "lib" / "can" / "libcaninterface.so"
        if not lib_path.exists():
            logger.error(f"CAN interface library not found at {lib_path}")
            cls.skipTest(cls, "CAN interface library not found")

    def setUp(self):
        """Set up each test"""
        if skip_tests:
            self.skipTest(skip_reason)
            
        # Create a protocol registry
        self.protocol_registry = ProtocolRegistry(pb_path=self.pb_path)
        
        # Create a CAN interface
        self.can_interface = CANInterfaceWrapper(node_id=0x01, channel='can0', baudrate=500000)
        
        # Ensure the interface is ready
        time.sleep(0.5)

    def tearDown(self):
        """Clean up after each test"""
        if hasattr(self, 'can_interface'):
            self.can_interface.stop_processing()
            # Give it time to shut down
            time.sleep(0.5)

    def test_interface_initialization(self):
        """Test that the CAN interface initializes correctly"""
        self.assertIsNotNone(self.can_interface)
        self.assertEqual(self.can_interface.node_id, 0x01)
        self.assertEqual(self.can_interface.channel, 'can0')
        self.assertEqual(self.can_interface.baudrate, 500000)
        
        # Check that the protocol registry was loaded
        self.assertIsNotNone(self.can_interface.protocol_registry)
        self.assertIsNotNone(self.can_interface.protocol_registry.registry)
        
        # Check the lookup dictionaries
        self.assertIsNotNone(self.can_interface.message_types_by_value)
        self.assertIsNotNone(self.can_interface.component_types_by_value)
        self.assertIsNotNone(self.can_interface.value_types_by_value)
        
        # Check that we have some types in the registry
        self.assertGreater(len(self.can_interface.protocol_registry.registry['message_types']), 0)
        self.assertGreater(len(self.can_interface.protocol_registry.registry['component_types']), 0)

    def test_message_processing(self):
        """Test that the CAN interface can start and stop message processing"""
        # Start message processing
        self.can_interface.start_processing()
        self.assertTrue(self.can_interface.auto_process)
        self.assertIsNotNone(self.can_interface.process_thread)
        self.assertTrue(self.can_interface.process_thread.is_alive())
        
        # Wait a bit for the thread to run
        time.sleep(0.5)
        
        # Stop message processing
        self.can_interface.stop_processing()
        self.assertFalse(self.can_interface.auto_process)
        time.sleep(0.5)  # Give it time to actually stop
        
        # Thread might still be alive if it's blocked on joining
        # So we don't test process_thread.is_alive() here

    def test_send_message(self):
        """Test sending a CAN message"""
        # Get message type and component type values from the registry
        message_types = self.can_interface.protocol_registry.registry['message_types']
        component_types = self.can_interface.protocol_registry.registry['component_types']
        value_types = self.can_interface.protocol_registry.registry['value_types']
        
        if not message_types or not component_types or not value_types:
            self.skipTest("Protocol registry is missing required types")
            
        # Get the first message type, component type, and value type
        msg_type_name = list(message_types.keys())[0]
        comp_type_name = list(component_types.keys())[0]
        val_type_name = list(value_types.keys())[0]
        
        msg_type = message_types[msg_type_name]
        comp_type = component_types[comp_type_name]
        val_type = value_types[val_type_name]
        
        # Try to send a message
        result = self.can_interface.send_message(
            msg_type,      # Message type
            comp_type,     # Component type
            0x01,          # Component ID
            0x01,          # Command ID
            val_type,      # Value type
            1              # Value
        )
        
        # The result should be True if the message was sent successfully
        self.assertTrue(result)

    def test_send_protocol_message(self):
        """Test sending a message using the protocol registry"""
        # Get message type and component type values from the registry
        message_types = list(self.can_interface.protocol_registry.registry['message_types'].keys())
        component_types = list(self.can_interface.protocol_registry.registry['component_types'].keys())
        
        if not message_types or not component_types:
            self.skipTest("Protocol registry is missing required types")
            
        # Get the first message type and component type
        msg_type = message_types[0]
        comp_type = component_types[0]
        
        # Get component IDs for this component type
        comp_ids = {}
        if comp_type.lower() in self.can_interface.protocol_registry.registry['components']:
            comp_ids = self.can_interface.protocol_registry.registry['components'][comp_type.lower()]
        
        # Get commands for this component type
        cmd_ids = {}
        if comp_type.lower() in self.can_interface.protocol_registry.registry['commands']:
            cmd_ids = self.can_interface.protocol_registry.registry['commands'][comp_type.lower()]
        
        # If we have component IDs and commands, we can test send_protocol_message
        if comp_ids and cmd_ids:
            comp_name = list(comp_ids.keys())[0]
            cmd_name = list(cmd_ids.keys())[0]
            
            # Create a message specification
            msg_spec = {
                'message_type': msg_type,
                'component_type': comp_type,
                'component_name': comp_name,
                'command_name': cmd_name,
                'value': 1
            }
            
            # Try to send the message
            result = self.can_interface.send_protocol_message(msg_spec)
            
            # The result should be True if the message was sent successfully
            self.assertTrue(result)
        else:
            self.skipTest("Protocol registry is missing required component IDs or commands")

    def test_register_handler(self):
        """Test registering a message handler"""
        # Flag to track if the handler was called
        self.handler_called = False
        
        # Define a test handler
        def test_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            logger.info(f"Test handler called: {msg_type}, {comp_type}, {comp_id}, {cmd_id}, {val_type}, {value}")
            self.handler_called = True
        
        # Register the handler for a specific message type
        result = self.can_interface.register_handler(0, 0x01, 0x01, test_handler)
        self.assertTrue(result)
        
        # Make sure the handler was added to the callbacks list
        self.assertGreaterEqual(len(self.can_interface.callbacks), 1)

    def test_hardware_loopback(self):
        """Test sending and receiving messages (requires hardware loopback)"""
        # This test requires a hardware loopback setup or another device to echo messages
        logger.warning("Hardware loopback test requires physical CAN loopback or echo device")
        logger.warning("This test may fail if no loopback is present")
        
        # Flag to track if the handler was called
        self.handler_called = False
        self.received_msg = None
        
        # Define a test handler that captures the received message
        def test_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            logger.info(f"Received message: {msg_type}, {comp_type}, {comp_id}, {cmd_id}, {val_type}, {value}")
            self.handler_called = True
            self.received_msg = (msg_type, comp_type, comp_id, cmd_id, val_type, value)
        
        # Register the handler for a specific message type
        # We use 0xFF for component_id and command_id to match all messages
        self.can_interface.register_handler(0, 0xFF, 0xFF, test_handler)
        
        # Start message processing
        self.can_interface.start_processing()
        
        # Send a test message
        sent_msg = (0, 0, 0x01, 0x01, 0, 1)  # Message type, component type, component ID, command ID, value type, value
        self.can_interface.send_message(*sent_msg)
        
        # Give it time to be received (if loopback is present)
        time.sleep(1.0)
        
        # Stop message processing
        self.can_interface.stop_processing()
        
        # If no loopback is present, this test will be skipped
        if not self.handler_called:
            self.skipTest("No CAN loopback detected, skipping message reception test")
        else:
            # Check that the received message matches what we sent
            self.assertEqual(self.received_msg, sent_msg)

if __name__ == '__main__':
    unittest.main() 