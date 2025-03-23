import pytest
import time
from datetime import datetime
import can
from unittest.mock import patch, Mock
import sys
import os
from pathlib import Path
import json
from unittest.mock import mock_open

# Add the server directory to Python path
server_dir = Path(__file__).parent.parent.parent
if str(server_dir) not in sys.path:
    sys.path.insert(0, str(server_dir))

from app import CANCommandGenerator, GoKartState

@pytest.fixture
def mock_can_bus():
    with patch('can.interface.Bus') as mock_bus:
        yield mock_bus

@pytest.fixture
def can_generator(mock_can_bus):
    # Mock protocol data for tests
    mock_protocol = {
        "kart": {
            "devices": {
                "nano": {"id": "0x2"}
            },
            "component_types": {
                "lights": {"id": "0x1"}
            },
            "components": {
                "lights": {
                    "front": {"id": "0x1"},
                    "rear": {"id": "0x2"}
                }
            },
            "commands": {
                "lights": {
                    "mode": {"id": "0x1", "values": {"off": "0x0", "low": "0x1", "high": "0x2", "hazard": "0x3"}},
                    "signal": {"id": "0x2", "values": {"off": "0x0", "left": "0x1", "right": "0x2"}},
                    "brake": {"id": "0x3", "values": {"off": "0x0", "on": "0xFF"}},
                    "test": {"id": "0x4", "values": {"off": "0x0", "on": "0xFF"}}
                }
            }
        }
    }
    
    # Use patch to mock the protocol loading and os.system
    with patch('os.system'), patch('builtins.open', mock_open(read_data=json.dumps(mock_protocol))):
        generator = CANCommandGenerator(channel='vcan0', bitrate=500000)
        yield generator

def test_gokart_state_initialization():
    """Test GoKartState initialization"""
    state = GoKartState()
    
    assert state.speed == 0.0
    assert state.throttle == 0
    assert state.battery_voltage == 0.0
    assert state.motor_temp == 0.0
    assert state.brake_pressure == 0.0
    assert state.steering_angle == 0.0
    assert isinstance(state.timestamp, float)

def test_gokart_state_to_dict():
    """Test GoKartState to dictionary conversion"""
    state = GoKartState()
    state.speed = 25.5
    state.throttle = 50
    state.battery_voltage = 12.6
    state.motor_temp = 45.2
    state.brake_pressure = 75.0
    state.steering_angle = -15.5
    
    state_dict = state.to_dict()
    
    assert state_dict['speed'] == 25.5
    assert state_dict['throttle'] == 50
    assert state_dict['battery_voltage'] == 12.6
    assert state_dict['motor_temp'] == 45.2
    assert state_dict['brake_pressure'] == 75.0
    assert state_dict['steering_angle'] == -15.5
    assert 'timestamp' in state_dict

def test_telemetry_speed_throttle(can_generator):
    """Test parsing speed and throttle telemetry"""
    # Create a speed/throttle message (30 km/h, 50% throttle)
    message = can.Message(
        arbitration_id=0x100,
        data=[0x02, 0x01, 0x2C, 0x32],  # 300 (30.0 km/h), 50%
        is_extended_id=False
    )
    
    can_generator.bus.recv.return_value = message
    can_generator.listen_telemetry(test_mode=True)
    
    assert can_generator.state.speed == 30.0
    assert can_generator.state.throttle == 50

def test_telemetry_battery_motor(can_generator):
    """Test parsing battery and motor temperature telemetry"""
    # Create a battery/motor message (12.5V, 45.5°C)
    message = can.Message(
        arbitration_id=0x101,
        data=[0x02, 0x00, 0x7D, 0x01, 0xC7],  # 125 (12.5V), 455 (45.5°C)
        is_extended_id=False
    )
    
    can_generator.bus.recv.return_value = message
    can_generator.listen_telemetry(test_mode=True)
    
    assert can_generator.state.battery_voltage == 12.5
    assert can_generator.state.motor_temp == 45.5

def test_telemetry_brake_steering(can_generator):
    """Test parsing brake pressure and steering angle telemetry"""
    # Create a brake/steering message (60% brake, 25.5° steering)
    message = can.Message(
        arbitration_id=0x102,
        data=[0x02, 0x02, 0x58, 0x00, 0xFF],  # 600 (60.0%), 255 (25.5°)
        is_extended_id=False
    )
    
    can_generator.bus.recv.return_value = message
    can_generator.listen_telemetry(test_mode=True)
    
    assert can_generator.state.brake_pressure == 60.0
    assert can_generator.state.steering_angle == 25.5

def test_telemetry_history(can_generator):
    """Test telemetry history management"""
    # Send multiple telemetry messages
    messages = [
        can.Message(arbitration_id=0x100, data=[0x02, 0x01, 0x2C, 0x32]),  # Speed/Throttle
        can.Message(arbitration_id=0x101, data=[0x02, 0x00, 0x7D, 0x01, 0xC7]),  # Battery/Motor
        can.Message(arbitration_id=0x102, data=[0x02, 0x02, 0x58, 0x00, 0xFF])  # Brake/Steering
    ]
    
    for message in messages:
        can_generator.bus.recv.return_value = message
        can_generator.listen_telemetry(test_mode=True)
    
    history = can_generator.get_history()
    
    assert len(history) > 0
    assert isinstance(history, list)
    assert all(isinstance(entry, dict) for entry in history)
    
    # Verify history size limit
    assert len(history) <= can_generator.history.maxlen

def test_telemetry_invalid_message(can_generator):
    """Test handling of invalid telemetry messages"""
    # Test message with invalid device ID but with enough data for struct.unpack
    message = can.Message(
        arbitration_id=0x100,
        data=[0x03, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00],  # Wrong device ID with enough data
        is_extended_id=False
    )
    
    can_generator.bus.recv.return_value = message
    can_generator.listen_telemetry(test_mode=True)
    
    # State should remain unchanged
    assert can_generator.state.speed == 0.0
    assert can_generator.state.throttle == 0

def test_telemetry_timeout(can_generator):
    """Test telemetry timeout handling"""
    # Set the mock bus to return None (timeout)
    can_generator.bus.recv.return_value = None
    
    # Save initial state values
    initial_speed = can_generator.state.speed
    initial_throttle = can_generator.state.throttle
    
    # Call with test mode to exit after one iteration
    can_generator.listen_telemetry(test_mode=True)
    
    # State should remain unchanged after a timeout
    assert can_generator.state.speed == initial_speed
    assert can_generator.state.throttle == initial_throttle

def test_get_current_state(can_generator):
    """Test getting current state with light states"""
    # Set some light states
    can_generator.light_states = {
        'lights_mode': 1,  # Low beam
        'lights_signal': 2,  # Right signal
        'lights_brake': can_generator.get_command_value('lights', 'brake', 'on'),
        'lights_test': can_generator.get_command_value('lights', 'test', 'off')
    }
    
    state = can_generator.get_current_state()
    
    # Check telemetry state
    assert 'speed' in state
    assert 'throttle' in state
    assert 'battery_voltage' in state
    assert 'motor_temp' in state
    assert 'brake_pressure' in state
    assert 'steering_angle' in state
    assert 'timestamp' in state
    
    # Check light states
    assert state['light_mode'] == 1
    assert state['light_signal'] == 2
    assert state['light_brake'] == 1  # On
    assert state['light_test'] == 0   # Off 