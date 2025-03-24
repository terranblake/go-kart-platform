import pytest
import json
from unittest.mock import patch, mock_open
import sys
import os
from pathlib import Path

# Add the server directory to Python path
server_dir = Path(__file__).parent.parent
if str(server_dir) not in sys.path:
    sys.path.insert(0, str(server_dir))

import app

@pytest.fixture
def client():
    """Flask test client"""
    app.config['TESTING'] = True
    with app.test_client() as client:
        yield client

@pytest.fixture
def mock_can_bus():
    """Mock CAN bus interface"""
    with patch('can.interface.Bus') as mock_bus:
        yield mock_bus

@pytest.fixture
def mock_protocol():
    """Mock protocol definition"""
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
                    },
                    "brake": {
                        "id": "0x3",
                        "values": {
                            "off": "0x0",
                            "on": "0x1"
                        }
                    },
                    "test": {
                        "id": "0x4",
                        "values": {
                            "off": "0x0",
                            "on": "0x1"
                        }
                    }
                }
            }
        }
    }

@pytest.fixture
def can_generator(mock_can_bus, mock_protocol):
    """CAN command generator with mocked dependencies"""
    with patch('os.system'), \
         patch('builtins.open', mock_open(read_data=json.dumps(mock_protocol))):
        generator = app.CANCommandGenerator(channel='vcan0', bitrate=500000)
        yield generator 