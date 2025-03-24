#!/usr/bin/env python3
"""
Unit tests for the CAN interface wrapper
"""

import unittest
import logging
import time
import sys
import os
import platform
import subprocess
from pathlib import Path

# Configure logging
logging.basicConfig(level=logging.INFO,
                   format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

def is_raspberry_pi():
    """Check if running on a Raspberry Pi"""
    try:
        if platform.system() != 'Linux':
            return False
            
        # Check for Raspberry Pi-specific files
        if os.path.exists('/proc/device-tree/model'):
            with open('/proc/device-tree/model', 'r') as f:
                model = f.read()
                if 'Raspberry Pi' in model:
                    return True
        return False
    except Exception as e:
        logger.error(f"Error detecting Raspberry Pi: {e}")
        return False

def check_can_interface():
    """Check if the CAN interface is available"""
    try:
        # Check if can0 exists
        result = subprocess.run(
            ['ip', 'link', 'show', 'can0'], 
            check=False, 
            stdout=subprocess.PIPE, 
            stderr=subprocess.PIPE
        )
        return result.returncode == 0
    except Exception as e:
        logger.error(f"Error checking CAN interface: {e}")
        return False

# Add the server directory to the path so we can import the modules
server_dir = Path(__file__).parent.parent.parent.parent
sys.path.insert(0, str(server_dir))

# Import the CAN interface wrapper and protocol registry
try:
    from lib.can.interface import CANInterfaceWrapper, has_can_hardware
    from lib.can.protocol_registry import ProtocolRegistry
except ImportError as e:
    logger.error(f"Failed to import CAN interface: {e}")
    raise

# Skip hardware-specific tests if not on a Raspberry Pi or no CAN interface
skip_hardware_tests = not is_raspberry_pi() or not check_can_interface()
skip_hardware_reason = "Not running on Raspberry Pi or can0 interface not available"

class TestCANInterface(unittest.TestCase):
    """Test suite for the CAN Interface"""

    @classmethod
    def setUpClass(cls):
        """Set up the test class - reset the CAN interface if available"""
        # Only try to reset hardware if we're on a Raspberry Pi with CAN
        if not skip_hardware_tests:
            try:
                # Reset the CAN interface to ensure it's in a good state
                subprocess.run(['sudo', 'ip', 'link', 'set', 'can0', 'down'], check=True)
                time.sleep(0.5)
                subprocess.run(['sudo', 'ip', 'link', 'set', 'can0', 'up', 'type', 'can', 'bitrate', '500000'], check=True)
                time.sleep(0.5)
                logger.info("CAN interface reset successfully")
            except subprocess.CalledProcessError as e:
                logger.error(f"Failed to reset CAN interface: {e}")
                skip_hardware_tests = True
            
        # Find the protocol directory
        repo_root = Path(__file__).parent.parent.parent.parent.parent
        cls.pb_path = repo_root / "protocol" / "generated" / "python"
        
        if not cls.pb_path.exists():
            logger.warning(f"Protocol directory not found at {cls.pb_path}")
            # Use the default path from the protocol registry
            cls.pb_path = "./protocol/generated/python"
        else:
            cls.pb_path = str(cls.pb_path)
            
        logger.info(f"Using protocol path: {cls.pb_path}")
        
        # The library file check is now handled by the interface itself
        # with a fallback to mock mode if not available

    def setUp(self):
        """Set up each test"""
        # Create a protocol registry
        self.protocol_registry = ProtocolRegistry(pb_path=self.pb_path)
        
        # Create a CAN interface
        self.can_interface = CANInterfaceWrapper(node_id=0x01, channel='can0', baudrate=500000)
        
        # Ensure the interface is ready
        time.sleep(0.1)

    def tearDown(self):
        """Clean up after each test"""
        if hasattr(self, 'can_interface'):
            self.can_interface.stop_processing()
            # Give it time to shut down
            time.sleep(0.1)

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
        
        # Check if we're using the mock interface or real hardware
        print(f"Using mock interface: {self.can_interface.using_mock}")
        print(f"Has CAN hardware: {has_can_hardware}")

    def test_message_processing(self):
        """Test that the CAN interface can start and stop message processing"""
        # Start message processing
        self.can_interface.start_processing()
        self.assertTrue(self.can_interface.auto_process)
        self.assertIsNotNone(self.can_interface.process_thread)
        self.assertTrue(self.can_interface.process_thread.is_alive())
        
        # Wait a bit for the thread to run
        time.sleep(0.1)
        
        # Stop message processing
        self.can_interface.stop_processing()
        self.assertFalse(self.can_interface.auto_process)
        time.sleep(0.1)  # Give it time to actually stop
        
        # Thread might still be alive if it's blocked on joining
        # So we don't test process_thread.is_alive() here

    def test_send_message(self):
        """Test that messages can be sent"""
        # Get message type values from the registry
        message_types = self.protocol_registry.registry['message_types']
        component_types = self.protocol_registry.registry['component_types']
        value_types = self.protocol_registry.registry['value_types']
        
        command_type = message_types['COMMAND']
        lights_type = component_types['LIGHTS']
        boolean_type = value_types['BOOLEAN']
        
        # Send a command to turn on headlights
        front_light_id = 0  # Front lights component ID
        toggle_cmd_id = 6   # TOGGLE command ID
        
        result = self.can_interface.send_message(
            command_type, lights_type, front_light_id, toggle_cmd_id, boolean_type, 1
        )
        self.assertTrue(result)

    @unittest.skipIf(skip_hardware_tests, skip_hardware_reason)
    def test_hardware_communication(self):
        """Test communication with actual CAN hardware (skipped if not available)"""
        # This test requires actual CAN hardware
        # It will send a message and verify that it can be received
        
        # First, make sure we're not using the mock interface
        self.assertFalse(self.can_interface.using_mock)
        
        # Register a test callback
        message_received = [False]
        
        def message_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            message_received[0] = True
            
        motors_type = self.protocol_registry.registry['component_types']['MOTORS']
        self.can_interface.register_handler(motors_type, 0, 0, message_handler)
        
        # Send a test message (loopback should receive it if hardware is configured properly)
        command_type = self.protocol_registry.registry['message_types']['COMMAND']
        value_type = self.protocol_registry.registry['value_types']['UINT8']
        
        self.can_interface.send_message(command_type, motors_type, 0, 0, value_type, 42)
        
        # Start processing and wait for a message
        self.can_interface.start_processing()
        
        # Wait up to 1 second for a message
        start_time = time.time()
        while not message_received[0] and time.time() - start_time < 1.0:
            time.sleep(0.1)
            
        self.can_interface.stop_processing()
        
        # Note: This assertion may fail if loopback isn't enabled or there's hardware issues
        # We're only running this test on actual hardware, so it's expected to work
        self.assertTrue(message_received[0])
    
    def test_send_command(self):
        """Test sending a command using the high-level send_command method"""
        # Send a command to toggle headlights
        result = self.can_interface.send_command(
            component_path='lights.headlights',
            command_name='TOGGLE',
            direct_value=1
        )
        self.assertTrue(result)
        
        # Send a command to set motor speed
        result = self.can_interface.send_command(
            component_path='motors.main',
            command_name='SPEED',
            direct_value=50
        )
        self.assertTrue(result)

    def test_state_tracking(self):
        """Test that the CAN interface properly tracks system state"""
        # Get initial state
        state = self.can_interface.get_current_state()
        self.assertIsNotNone(state)
        
        # Check state structure
        self.assertIn('timestamp', state)
        self.assertIn('speed', state)
        self.assertIn('throttle', state)
        self.assertIn('battery', state)
        self.assertIn('lights', state)
        
        # Test updating the state through simulated messages
        self.can_interface._handle_motor_speed(0, 0, 0, 0, 0, 42)
        self.can_interface._handle_throttle(0, 0, 0, 0, 0, 75)
        self.can_interface._handle_battery_voltage(0, 0, 0, 0, 0, 125)  # 12.5V
        
        # Check that state was updated
        updated_state = self.can_interface.get_current_state()
        self.assertEqual(updated_state['speed'], 42)
        self.assertEqual(updated_state['throttle'], 75)
        self.assertEqual(updated_state['battery']['voltage'], 12.5)
        
        # Check history
        history = self.can_interface.get_history()
        self.assertGreater(len(history), 0)
        
        # Check that history has the updates we sent
        motor_entry = next((e for e in history if 'speed' in e), None)
        throttle_entry = next((e for e in history if 'throttle' in e), None)
        battery_entry = next((e for e in history if 'battery' in e), None)
        
        self.assertIsNotNone(motor_entry)
        self.assertIsNotNone(throttle_entry)
        self.assertIsNotNone(battery_entry)
        
        self.assertEqual(motor_entry['speed'], 42)
        self.assertEqual(throttle_entry['throttle'], 75)
        self.assertEqual(battery_entry['battery']['voltage'], 12.5)

if __name__ == "__main__":
    unittest.main() 