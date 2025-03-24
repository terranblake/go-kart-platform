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