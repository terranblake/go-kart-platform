import pytest
import time
import os
from unittest.mock import MagicMock, patch

# Import components to test
from lib.can.interface import CANInterfaceWrapper

class TestRawMessageHandling:
    """Tests for raw message handling in the CAN interface."""

    @pytest.fixture
    def can_interface(self):
        """Create a test CAN interface with mock behavior."""
        # Create a mock implementation since we can't access real hardware
        with patch('lib.can.interface.has_can_hardware', False):
            interface = CANInterfaceWrapper(
                node_id=0x01,
                channel='vcan0',  # Use virtual CAN
            )
            yield interface

    def test_register_raw_handler(self, can_interface):
        """Test registration of raw message handlers."""
        # Create a mock handler function
        mock_handler = MagicMock()
        
        # Register the handler
        can_interface.register_raw_handler(0x700, mock_handler)
        
        # In mock mode, we can't really verify the handler was registered correctly,
        # but we can ensure the method executes without errors

    def test_send_raw_message(self, can_interface):
        """Test sending raw CAN messages."""
        # Create test data
        test_data = bytes([0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08])
        
        # Test sending
        result = can_interface.send_raw_message(0x700, test_data)
        
        # In mock mode, this should return success
        assert result is True, "send_raw_message should return True in mock mode"
        
    def test_raw_message_length_validation(self, can_interface):
        """Test validation of message length."""
        # Create oversized data (more than 8 bytes, which is invalid for CAN)
        oversized_data = bytes([i for i in range(10)])
        
        # Send message with oversized data
        result = can_interface.send_raw_message(0x700, oversized_data)
        
        # Should fail due to length validation
        assert result is False, "Oversized messages should be rejected"
        
    @pytest.mark.skipif(not os.environ.get('RUN_HARDWARE_TESTS'), 
                       reason="Hardware tests disabled")
    def test_integration_with_hardware(self):
        """
        Integration test with real hardware.
        
        This test is skipped by default and only runs when RUN_HARDWARE_TESTS
        environment variable is set, as it requires physical CAN hardware.
        """
        # Create real interface
        with patch('lib.can.interface.has_can_hardware', True):
            interface = CANInterfaceWrapper(
                node_id=0x01,
                channel='can0',  # Use real CAN
            )

            # Set up a mock to record received messages
            received_messages = []
            def on_raw_message(can_id, data, length):
                received_messages.append((can_id, data[:length]))
            
            # Register handler
            interface.register_raw_handler(0x700, on_raw_message)
            
            # Start processing
            interface.start_processing()
            
            try:
                # Send a test message
                test_data = bytes([0x01, 0x02, 0x03, 0x04, 0x05])
                interface.send_raw_message(0x700, test_data)
                
                # Wait for message to be processed
                start_time = time.time()
                timeout = 1.0  # 1 second timeout
                
                while time.time() - start_time < timeout and not received_messages:
                    time.sleep(0.01)
                    
                # Verify we received the message we sent
                assert len(received_messages) > 0, "No messages received"
                assert received_messages[0][0] == 0x700, "Unexpected CAN ID"
                assert received_messages[0][1] == test_data, "Message data mismatch"
                
            finally:
                # Clean up
                interface.stop_processing()
