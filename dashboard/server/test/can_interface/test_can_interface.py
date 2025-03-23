import pytest
import can
from unittest.mock import Mock, patch
import sys
import os
from pathlib import Path

# Add the server directory to Python path
server_dir = Path(__file__).parent.parent.parent
if str(server_dir) not in sys.path:
    sys.path.insert(0, str(server_dir))

from app import CANCommandGenerator

@pytest.fixture
def mock_can_bus():
    with patch('can.interface.Bus') as mock_bus:
        mock_bus.return_value = Mock()  # Create a mock bus instance
        yield mock_bus

@pytest.fixture
def can_generator(mock_can_bus):
    with patch('os.system'):
        generator = CANCommandGenerator(channel='vcan0', bitrate=500000)
        generator.bus = mock_can_bus.return_value  # Set the mock bus instance
        yield generator

def test_can_initialization(mock_can_bus):
    """Test CAN bus initialization"""
    generator = CANCommandGenerator(channel='vcan0', bitrate=500000)
    mock_can_bus.assert_called_once_with(
        channel='vcan0',
        bustype='socketcan',
        bitrate=500000
    )

def test_send_emergency_stop(can_generator):
    """Test sending emergency stop command"""
    can_generator.send_emergency_stop(True)
    
    # Verify message was sent with correct data
    message = can_generator.bus.send.call_args[0][0]
    assert message.arbitration_id == can_generator.COMMAND_ID
    assert message.data[0] == can_generator.DEVICE_NANO
    assert message.data[1] == can_generator.CMD_EMERGENCY_STOP
    assert message.data[2] == 0xFF

def test_send_speed_command(can_generator):
    """Test sending speed control command"""
    test_speed = 50
    can_generator.send_speed_command(test_speed)
    
    message = can_generator.bus.send.call_args[0][0]
    assert message.arbitration_id == can_generator.COMMAND_ID
    assert message.data[0] == can_generator.DEVICE_NANO
    assert message.data[1] == can_generator.CMD_SPEED_CONTROL
    assert message.data[2] == test_speed

def test_send_steering_command(can_generator):
    """Test sending steering control command"""
    test_angle = 30
    can_generator.send_steering_command(test_angle)
    
    message = can_generator.bus.send.call_args[0][0]
    assert message.arbitration_id == can_generator.COMMAND_ID
    assert message.data[0] == can_generator.DEVICE_NANO
    assert message.data[1] == can_generator.CMD_STEERING_CONTROL
    assert message.data[2] == test_angle + 45  # Check angle conversion

def test_send_brake_command(can_generator):
    """Test sending brake control command"""
    test_pressure = 75
    can_generator.send_brake_command(test_pressure)
    
    message = can_generator.bus.send.call_args[0][0]
    assert message.arbitration_id == can_generator.COMMAND_ID
    assert message.data[0] == can_generator.DEVICE_NANO
    assert message.data[1] == can_generator.CMD_BRAKE_CONTROL
    assert message.data[2] == test_pressure

def test_value_limits():
    """Test command value limits"""
    with patch('can.interface.Bus') as mock_bus:
        mock_bus.return_value = Mock()  # Create a mock bus instance
        generator = CANCommandGenerator(channel='vcan0', bitrate=500000)
        generator.bus = mock_bus.return_value  # Set the mock bus instance
        
        # Test speed limits
        generator.send_speed_command(150)  # Above max
        message = generator.bus.send.call_args[0][0]
        assert message.data[2] == 100  # Should be limited to 100
        
        # Test steering limits
        generator.send_steering_command(60)  # Above max
        message = generator.bus.send.call_args[0][0]
        assert message.data[2] == 90  # Should be limited to 45 + 45
        
        generator.send_steering_command(-60)  # Below min
        message = generator.bus.send.call_args[0][0]
        assert message.data[2] == 0  # Should be limited to -45 + 45
        
        # Test brake limits
        generator.send_brake_command(120)  # Above max
        message = generator.bus.send.call_args[0][0]
        assert message.data[2] == 100  # Should be limited to 100

@pytest.mark.parametrize("test_input,expected_value", [
    (can.Message(arbitration_id=0x100, data=[0x02, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00]), 5.0),  # Speed: 0x0032 = 50/10 = 5.0
    (can.Message(arbitration_id=0x101, data=[0x02, 0x00, 0x96, 0x00, 0x00, 0x28, 0x00, 0x00]), 15.0), # Battery: 0x0096 = 150/10 = 15.0
    (can.Message(arbitration_id=0x102, data=[0x02, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00]), 5.0)   # Brake: 0x0032 = 50/10 = 5.0
])
def test_telemetry_parsing(can_generator, test_input, expected_value):
    """Test parsing of telemetry messages with struct-packed values."""
    # Set mock bus to return our test message
    can_generator.bus.recv.return_value = test_input
    
    # Call the listen_telemetry method in test mode
    can_generator.listen_telemetry(test_mode=True)
    
    # Verify the parsed value based on arbitration ID
    if test_input.arbitration_id == 0x100:  # Speed
        assert can_generator.state.speed == expected_value
    elif test_input.arbitration_id == 0x101:  # Battery voltage
        assert can_generator.state.battery_voltage == expected_value
    elif test_input.arbitration_id == 0x102:  # Brake pressure
        assert can_generator.state.brake_pressure == expected_value 