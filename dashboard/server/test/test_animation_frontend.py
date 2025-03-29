"""
Test cases for the animation frontend integration.

These tests ensure the animation UI components render correctly
and that communication between frontend and backend works properly.
"""

import json
import unittest
from unittest.mock import patch, MagicMock
from flask import template_rendered

# Import modules to test
from api.endpoints import app, socketio
from lib.animations.manager import AnimationManager
from lib.animations.protocol import AnimationProtocol

class TestAnimationFrontend(unittest.TestCase):
    """Test the animation frontend integration."""
    
    def setUp(self):
        """Set up test environment before each test."""
        # Use existing app instance for testing
        self.app = app
        self.app.config['TESTING'] = True
        self.app.config['SERVER_NAME'] = 'localhost'
        self.client = self.app.test_client()
        
        # Create mock CAN interface
        self.mock_can_interface = MagicMock()
        self.mock_can_interface.send_message.return_value = True
        
        # Create mock animation protocol
        self.mock_protocol = MagicMock(spec=AnimationProtocol)
        
        # Create mock animation manager
        self.mock_animation_manager = MagicMock(spec=AnimationManager)
        
        # Set up test animation data
        self.test_animation = {
            "id": "test-animation-1",
            "name": "Test Animation",
            "type": "chase",
            "speed": 100,
            "direction": "forward",
            "dimensions": {
                "length": 60,
                "height": 1
            },
            "colors": ["#ff0000", "#00ff00", "#0000ff"]
        }
    
    def test_animations_component_renders(self):
        """Test that the animations component renders correctly."""
        recorded_templates = []
        def record_template(sender, template, context, **extra):
            recorded_templates.append(template.name)
            
        # Connect signal before request
        signal_connected = template_rendered.connect(record_template, self.app)
        try:
            # Request the main page
            response = self.client.get('/')
            
            # Check that the page loaded successfully
            self.assertEqual(response.status_code, 200)
            
            # Check that the main template was rendered
            self.assertIn('index.html', recorded_templates)
            
            # Check for animation-related elements in the response content
            self.assertIn(b'animation-dashboard', response.data)
            self.assertIn(b'show-animation-panel', response.data)
        finally:
            # Disconnect signal after request
            template_rendered.disconnect(record_template, self.app)
    
    def test_animation_api_endpoints(self):
        """Test the animation API endpoints."""
        with patch('api.animations.animation_manager', self.mock_animation_manager):
            # Mock get_all_animations to return our test animation
            self.mock_animation_manager.get_all_animations.return_value = [self.test_animation]
            
            # Test listing animations
            response = self.client.get('/api/animations/')
            self.assertEqual(response.status_code, 200)
            data = json.loads(response.data)
            self.assertGreaterEqual(len(data), 1)
            self.assertTrue(any(a['id'] == 'test-animation-1' for a in data))
            
            # Mock get_animation to return the test animation
            self.mock_animation_manager.get_animation.return_value = self.test_animation
            
            # Test getting a specific animation
            response = self.client.get('/api/animations/test-animation-1')
            self.assertEqual(response.status_code, 200)
            animation = json.loads(response.data)
            self.assertEqual(animation['name'], 'Test Animation')
            
            # Test the test endpoint
            test_data = {
                "animation_data": {
                    "name": "Test Direct Animation",
                    "type": "chase",
                    "dimensions": {
                        "length": 60,
                        "height": 1
                    },
                    "colors": ["#ff0000"]
                },
                "loop": True
            }
            # Mock save_animation and play_animation
            self.mock_animation_manager.save_animation.return_value = "temp_123"
            self.mock_animation_manager.play_animation.return_value = True
            
            response = self.client.post('/api/animations/test', 
                json=test_data,
                content_type='application/json')
            self.assertEqual(response.status_code, 200)
            result = json.loads(response.data)
            self.assertEqual(result['status'], 'success')
            self.assertIn('temp_id', result)
            self.mock_animation_manager.save_animation.assert_called_once()
            self.mock_animation_manager.play_animation.assert_called_once()
    
    def test_websocket_communication(self):
        """Test WebSocket communication for animation control."""
        with patch('api.animations.animation_manager', self.mock_animation_manager):
            # Mock methods that will be called
            # Assuming get_status is called internally by the socket handler or manager
            # If AnimationManager spec doesn't have get_status, remove this line or add it to the spec
            # For now, let's assume it's needed and add it to the mock's attributes if it causes issues later
            # self.mock_animation_manager.get_status.return_value = {'is_playing': False} 
            self.mock_animation_manager.play_animation.return_value = True
            self.mock_animation_manager.stop_playback.return_value = True
            
            # Create a test client for WebSocket
            socket_client = socketio.test_client(self.app, namespace='/animations')
            self.assertTrue(socket_client.is_connected('/animations'))
            
            # Test receiving animation status on connect
            received = socket_client.get_received('/animations')
            self.assertGreaterEqual(len(received), 1)
            self.assertTrue(any(event['name'] == 'animation_status' for event in received))
            
            # Test sending play animation request
            socket_client.emit('play_animation', {
                'animation_id': 'test-animation-1',
                'loop': True
            }, namespace='/animations')
            
            # Check that play_animation was called (adjust for potential default args)
            self.mock_animation_manager.play_animation.assert_called_with('test-animation-1', True, None) 
            
            # Check that we received a success response (or status update)
            received = socket_client.get_received('/animations')
            # Note: The actual response might be 'animation_status' update, not 'success'
            self.assertTrue(any(event['name'] in ['success', 'animation_status'] for event in received))
            
            # Test sending stop animation request
            socket_client.emit('stop_animation', namespace='/animations')
            
            # Check that stop_playback was called
            self.mock_animation_manager.stop_playback.assert_called_once()
            
            # Check that we received a success response (or status update)
            received = socket_client.get_received('/animations')
            self.assertTrue(any(event['name'] in ['success', 'animation_status'] for event in received))
