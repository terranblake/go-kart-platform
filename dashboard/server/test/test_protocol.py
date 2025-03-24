import pytest
import json
from flask import Flask
from api.protocol import register_protocol_routes
from lib.can.protocol import load_protocol_definitions, view_protocol_structure

@pytest.fixture
def app():
    """Create a Flask app for testing."""
    app = Flask(__name__)
    register_protocol_routes(app)
    return app

@pytest.fixture
def client(app):
    """Create a test client for the app."""
    return app.test_client()

def test_protocol_loading():
    """Test that protocol definitions can be loaded."""
    protocol = load_protocol_definitions()
    assert protocol is not None
    assert 'message_types' in protocol
    assert 'component_types' in protocol
    assert 'components' in protocol
    assert 'commands' in protocol

def test_protocol_structure():
    """Test that the protocol structure can be viewed."""
    protocol = load_protocol_definitions()
    structure = view_protocol_structure(protocol)
    assert structure is not None
    assert 'message_types' in structure
    assert 'component_types' in structure
    assert 'components' in structure
    assert 'commands' in structure

def test_protocol_endpoint(client):
    """Test the protocol API endpoint."""
    response = client.get('/api/protocol')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'message_types' in data
    assert 'component_types' in data
    assert 'components' in data
    assert 'commands' in data

def test_component_type_endpoint(client):
    """Test the component type API endpoint."""
    response = client.get('/api/protocol/lights')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'components' in data
    assert 'commands' in data
    assert 'FRONT' in data['components']
    assert 'mode' in data['commands']

def test_component_endpoint(client):
    """Test the component API endpoint."""
    response = client.get('/api/protocol/lights/FRONT')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'component_id' in data
    assert 'commands' in data
    assert data['component_id'] == 0

def test_command_endpoint(client):
    """Test the command API endpoint."""
    response = client.get('/api/protocol/lights/FRONT/mode')
    assert response.status_code == 200
    data = json.loads(response.data)
    assert 'component_id' in data
    assert 'command_id' in data
    assert 'values' in data
    assert 'OFF' in data['values']
    assert 'ON' in data['values'] 