import pytest
from flask import json
from unittest.mock import patch

# Import directly from modules, avoiding importing from 'api' package as a relative import
from api.endpoints import app, command_generator

@pytest.fixture
def client():
    with patch('can.interface.Bus'), patch('os.system'):
        app.config['TESTING'] = True
        with app.test_client() as client:
            yield client

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
        assert data['status'] == 'success'

def test_control_lights(client):
    """Test light control using the /api/command endpoint"""
    with patch.object(command_generator, 'bus'):
        # Define the different light modes to test
        test_modes = [
            {'name': 'off', 'value': 0},
            {'name': 'on', 'value': 1},
            {'name': 'hazard', 'value': 8}
        ]
        
        for mode in test_modes:
            # Test light mode command
            response = client.post('/api/command',
                                data=json.dumps({
                                    'component_type': 'LIGHTS',
                                    'component_name': 'ALL',
                                    'command_name': 'MODE',
                                    'direct_value': mode['value']
                                }),
                                content_type='application/json')
            assert response.status_code == 200
            data = json.loads(response.data)
            assert data['status'] == 'success'
            
            # Test brake command
            response = client.post('/api/command',
                                data=json.dumps({
                                    'component_type': 'LIGHTS',
                                    'component_name': 'ALL',
                                    'command_name': 'BRAKE',
                                    'direct_value': 1
                                }),
                                content_type='application/json')
            assert response.status_code == 200
            data = json.loads(response.data)
            assert data['status'] == 'success'

def test_get_settings(client):
    """Test the /api/settings GET endpoint"""
    response = client.get('/api/settings')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'max_speed' in data

def test_update_settings(client):
    """Test the /api/settings POST endpoint"""
    test_settings = {
        'max_speed': 40
    }
    response = client.post('/api/settings',
                          data=json.dumps(test_settings),
                          content_type='application/json')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert data['status'] == 'success'
    assert 'settings' in data 