import pytest
import json
import os
from unittest.mock import patch, mock_open
from pathlib import Path
import sys

# Add the server directory to Python path
server_dir = Path(__file__).parent.parent.parent
if str(server_dir) not in sys.path:
    sys.path.insert(0, str(server_dir))

from app import CANCommandGenerator

@pytest.fixture
def mock_protocol():
    return {
        "kart": {
            "component_types": {
                "lights": {"id": "0x1"},
                "motor": {"id": "0x2"}
            },
            "components": {
                "lights": {
                    "front": {"id": "0x1"},
                    "rear": {"id": "0x2"}
                }
            },
            "commands": {
                "lights": {
                    "mode": {
                        "id": "0x1",
                        "values": {
                            "off": "0x0",
                            "low": "0x1",
                            "high": "0x2",
                            "hazard": "0x3"
                        }
                    },
                    "signal": {
                        "id": "0x2",
                        "values": {
                            "off": "0x0",
                            "left": "0x1",
                            "right": "0x2"
                        }
                    }
                }
            }
        }
    }

@pytest.fixture
def can_generator(mock_protocol):
    with patch('can.interface.Bus'), \
         patch('os.system'), \
         patch('builtins.open', mock_open(read_data=json.dumps(mock_protocol))):
        generator = CANCommandGenerator(channel='vcan0', bitrate=500000)
        yield generator

def test_load_protocol_definitions(can_generator, mock_protocol):
    """Test loading protocol definitions from file"""
    assert can_generator.protocol == mock_protocol

def test_get_component_type_id(can_generator):
    """Test getting component type IDs"""
    assert can_generator.get_component_type_id('lights') == 0x1
    assert can_generator.get_component_type_id('motor') == 0x2
    assert can_generator.get_component_type_id('invalid') == 0

def test_get_component_id(can_generator):
    """Test getting component IDs"""
    assert can_generator.get_component_id('lights', 'front') == 0x1
    assert can_generator.get_component_id('lights', 'rear') == 0x2
    assert can_generator.get_component_id('lights', 'invalid') == 0

def test_get_command_id(can_generator):
    """Test getting command IDs"""
    assert can_generator.get_command_id('lights', 'mode') == 0x1
    assert can_generator.get_command_id('lights', 'signal') == 0x2
    assert can_generator.get_command_id('lights', 'invalid') == 0

def test_get_command_value(can_generator):
    """Test getting command values"""
    assert can_generator.get_command_value('lights', 'mode', 'off') == 0x0
    assert can_generator.get_command_value('lights', 'mode', 'low') == 0x1
    assert can_generator.get_command_value('lights', 'mode', 'high') == 0x2
    assert can_generator.get_command_value('lights', 'mode', 'invalid') == 0

def test_get_command_values(can_generator):
    """Test getting all command values"""
    mode_values = can_generator.get_command_values('lights', 'mode')
    # Check if the values exist, not comparing the entire dictionary
    assert 'off' in mode_values
    assert 'low' in mode_values
    assert 'high' in mode_values
    assert 'hazard' in mode_values

def test_get_message_header(can_generator):
    """Test message header generation"""
    # Test valid header
    header = can_generator.get_message_header('lights', 'mode')
    assert header == 0x11  # 0x1 (component_type) << 4 | 0x1 (command)
    
    # Test invalid header
    header = can_generator.get_message_header('invalid', 'mode')
    assert header == 0  # Will be 0 since both component_type_id and command_id are 0

def test_protocol_file_not_found():
    """Test handling of missing protocol file"""
    with patch('can.interface.Bus'), \
         patch('os.system'), \
         patch('builtins.open', side_effect=FileNotFoundError):
        generator = CANCommandGenerator(channel='vcan0', bitrate=500000)
        assert generator.protocol == {"kart": {}}

def test_invalid_protocol_json():
    """Test handling of invalid JSON in protocol file"""
    with patch('can.interface.Bus'), \
         patch('os.system'), \
         patch('builtins.open', mock_open(read_data='invalid json')):
        generator = CANCommandGenerator(channel='vcan0', bitrate=500000)
        assert generator.protocol == {"kart": {}}

def test_send_can_message(can_generator):
    """Test sending CAN messages using protocol"""
    # Mock bus.send to always succeed
    can_generator.bus.send.return_value = None
    
    # Test light mode command
    success = can_generator.send_can_message('lights', 'mode', 'front', 0x1)
    assert success is True
    
    message = can_generator.bus.send.call_args[0][0]
    assert message.arbitration_id == 0x100
    assert message.data[0] == 0x11  # Header: lights (0x1) << 4 | mode (0x1)
    assert message.data[1] == 0x1   # Component ID: front
    assert message.data[2] == 0x1   # Value: low beam
    
    # Test invalid message
    success = can_generator.send_can_message('invalid', 'mode', 'front', 0x1)
    assert success is False 