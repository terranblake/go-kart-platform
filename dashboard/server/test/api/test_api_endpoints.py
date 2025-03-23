import pytest
from flask import json
import sys
import os
from pathlib import Path
from unittest.mock import patch

# Add the server directory to Python path
server_dir = Path(__file__).parent.parent.parent
if str(server_dir) not in sys.path:
    sys.path.insert(0, str(server_dir))

from app import app, command_generator

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

def test_send_command(client):
    """Test the /api/command endpoint"""
    with patch.object(command_generator, 'bus'):
        test_commands = {
            'speed': 20,
            'steering': 15,
            'brake': 50,
            'emergency_stop': True
        }
        
        response = client.post('/api/command',
                             data=json.dumps(test_commands),
                             content_type='application/json')
        assert response.status_code == 200
        data = json.loads(response.data)
        assert data['success'] is True

def test_control_lights(client):
    """Test the /api/lights endpoint"""
    with patch.object(command_generator, 'bus'):
        test_light_commands = [
            {'mode': 1},  # Low beam
            {'signal': 1},  # Left signal
            {'brake': True},  # Brake lights on
            {'test': True},  # Test mode on
            {'location': 'front'}  # Front location
        ]
        
        for command in test_light_commands:
            response = client.post('/api/lights',
                                 data=json.dumps(command),
                                 content_type='application/json')
            assert response.status_code == 200
            data = json.loads(response.data)
            assert data['success'] is True

def test_get_camera_status(client):
    """Test the /api/camera/status endpoint"""
    response = client.get('/api/camera/status')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'status' in data

def test_get_settings(client):
    """Test the /api/settings GET endpoint"""
    response = client.get('/api/settings')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'camera_status' in data

def test_update_settings(client):
    """Test the /api/settings POST endpoint"""
    test_settings = {
        'camera_enabled': True
    }
    response = client.post('/api/settings',
                          data=json.dumps(test_settings),
                          content_type='application/json')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['success'] is True 