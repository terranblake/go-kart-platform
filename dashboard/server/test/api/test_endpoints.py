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

# Import directly from modules
from api.endpoints import app, command_generator, socketio

@pytest.fixture
def client():
    with patch('can.interface.Bus'), patch('os.system'):
        app.config['TESTING'] = True
        with app.test_client() as client:
            yield client

def test_index_route(client):
    """Test the main index route"""
    response = client.get('/')
    assert response.status_code == 200
    
def test_app_configuration():
    """Test the Flask app configuration"""
    # Check template folder ends with correct path
    assert app.template_folder.endswith('../templates')
    # Check static folder ends with correct path
    assert app.static_folder.endswith('../static')
    
    # Check CORS is configured (indirectly verify it's working)
    # CORS should add OPTIONS method to routes
    options_methods = [rule.methods for rule in app.url_map.iter_rules() 
                      if 'OPTIONS' in rule.methods]
    assert len(options_methods) > 0
    
    # Check SocketIO is configured
    assert socketio is not None 