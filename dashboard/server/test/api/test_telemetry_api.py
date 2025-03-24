import pytest
from flask import json
import sys
import os
from pathlib import Path
from unittest.mock import patch
from api.endpoints import app

@pytest.fixture
def client():
    with patch('can.interface.Bus'), patch('os.system'):
        app.config['TESTING'] = True
        with app.test_client() as client:
            yield client

def test_get_state(client):
    """Test the /api/state endpoint"""
    response = client.get('/api/state')
    assert response.status_code == 200
    data = json.loads(response.data)
    
    # Check all required fields are present
    required_fields = [
        'speed', 'throttle', 'battery_voltage', 'motor_temp',
        'brake_pressure', 'steering_angle', 'timestamp'
    ]
    for field in required_fields:
        assert field in data

def test_get_history(client):
    """Test the /api/history endpoint"""
    response = client.get('/api/history')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert isinstance(data, list) 