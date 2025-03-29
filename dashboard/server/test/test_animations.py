"""
Tests for the animation API.

This module contains tests for the animation API endpoints and protocol.
"""

import unittest
import json
import os
import tempfile
from unittest.mock import MagicMock, patch

from lib.animations.protocol import AnimationProtocol
from lib.animations.manager import AnimationManager, Animation
from lib.can.interface import CANInterfaceWrapper

class TestAnimationProtocol(unittest.TestCase):
    """Test the animation protocol."""
    
    def setUp(self):
        """Set up test fixtures."""
        # Create a mock CAN interface
        self.can_interface = MagicMock(spec=CANInterfaceWrapper)
        
        # Create the protocol
        self.protocol = AnimationProtocol(self.can_interface)
        
    def test_start_stream(self):
        """Test starting an animation stream."""
        # Set up the mock to return True
        self.can_interface.send_raw_message.return_value = True
        
        # Call the method
        result = self.protocol.start_stream(10, 30, 60)
        
        # Check the result
        self.assertTrue(result)
        self.assertTrue(self.protocol.stream_active)
        
        # Check that the correct message was sent
        self.can_interface.send_raw_message.assert_called_once()
        args = self.can_interface.send_raw_message.call_args[0]
        self.assertEqual(args[0], 0x700)  # CAN ID
        self.assertEqual(args[1][0], 0x01)  # CMD_STREAM_START
        self.assertEqual(args[1][1], 10)    # num_frames
        self.assertEqual(args[1][2], 30)    # fps
        self.assertEqual(args[1][3], 60)    # num_leds (low byte)
        self.assertEqual(args[1][4], 0)     # num_leds (high byte)
        
    def test_end_stream(self):
        """Test ending an animation stream."""
        # Set up the mock to return True
        self.can_interface.send_raw_message.return_value = True
        
        # First, start a stream
        self.protocol.start_stream(10, 30, 60)
        
        # Reset the mock
        self.can_interface.send_raw_message.reset_mock()
        
        # End the stream
        result = self.protocol.end_stream()
        
        # Check the result
        self.assertTrue(result)
        
        # Check that the correct message was sent
        self.can_interface.send_raw_message.assert_called_once()
        args = self.can_interface.send_raw_message.call_args[0]
        self.assertEqual(args[0], 0x700)  # CAN ID
        self.assertEqual(args[1][0], 0x02)  # CMD_STREAM_END
        
    def test_send_frame(self):
        """Test sending a frame."""
        # Set up the mock to return True
        self.can_interface.send_raw_message.return_value = True
        
        # First, start a stream
        self.protocol.start_stream(10, 30, 60)
        
        # Reset the mock
        self.can_interface.send_raw_message.reset_mock()
        
        # Prepare frame data - 3 RGB LEDs
        led_data = [(255, 0, 0), (0, 255, 0), (0, 0, 255)]
        
        # Send the frame
        result = self.protocol.send_rgb_frame(0, led_data)
        
        # Check the result
        self.assertTrue(result)
        
        # Check that the correct frame start message was sent
        frame_start_call = self.can_interface.send_raw_message.call_args_list[0]
        self.assertEqual(frame_start_call[0][0], 0x700)  # CAN ID
        self.assertEqual(frame_start_call[0][1][0], 0x03)  # CMD_FRAME_START
        self.assertEqual(frame_start_call[0][1][1], 0)     # frame_num
        self.assertEqual(frame_start_call[0][1][2], 9)     # frame_size (low byte) = 3 LEDs * 3 bytes = 9
        self.assertEqual(frame_start_call[0][1][3], 0)     # frame_size (high byte)
        
        # Check that the correct frame data was sent - should be 2 message with raw data
        data_call = self.can_interface.send_raw_message.call_args_list[1]
        self.assertEqual(data_call[0][0], 0x701)  # CAN ID for data
        # Data might not include all 9 bytes if the protocol implementation 
        # limits the data to 8 bytes per message (which is the CAN limit)
        data_bytes = bytes(data_call[0][1])
        expected_start = b'\xff\x00\x00\x00\xff\x00\x00\x00'
        self.assertEqual(data_bytes[:8], expected_start)
        
        # Verify we're sending multiple messages (at least for start, data, and end)
        self.assertGreaterEqual(self.can_interface.send_raw_message.call_count, 3)

    def test_send_config(self):
        """Test sending configuration parameters."""
        # Set up the mock to return True
        self.can_interface.send_raw_message.return_value = True
        
        # Send a configuration parameter
        result = self.protocol.send_config(0x01, 30)  # FPS = 30
        
        # Check the result
        self.assertTrue(result)
        
        # Check that the correct message was sent
        self.can_interface.send_raw_message.assert_called_once()
        args = self.can_interface.send_raw_message.call_args[0]
        self.assertEqual(args[0], 0x700)  # CAN ID
        self.assertEqual(args[1][0], 0x05)  # CMD_CONFIG
        self.assertEqual(args[1][1], 0x01)  # CONFIG_FPS
        self.assertEqual(args[1][2], 30)    # value

class TestAnimationManager(unittest.TestCase):
    """Test the animation manager."""
    
    def setUp(self):
        """Set up test fixtures."""
        # Create a temporary directory
        self.temp_dir = tempfile.TemporaryDirectory()
        
        # Create a mock protocol
        self.protocol = MagicMock(spec=AnimationProtocol)
        
        # Create the manager
        self.manager = AnimationManager(self.protocol, self.temp_dir.name)
        
    def tearDown(self):
        """Tear down test fixtures."""
        self.temp_dir.cleanup()
        
    def test_save_animation(self):
        """Test saving an animation."""
        # Create test animation data
        animation_data = {
            'name': 'Test Animation',
            'type': 'chase',
            'speed': 100,
            'direction': 'forward',
            'dimensions': {
                'length': 60,
                'height': 1
            },
            'colors': ['#ff0000', '#00ff00', '#0000ff']
        }
        
        # Save the animation
        animation_id = self.manager.save_animation(animation_data)
        
        # Check that the animation was saved
        self.assertIsNotNone(animation_id)
        self.assertIn(animation_id, self.manager.animations)
        
        # Check the animation object
        animation = self.manager.animations[animation_id]
        self.assertEqual(animation.name, 'Test Animation')
        self.assertEqual(animation.type, 'chase')
        self.assertEqual(animation.speed, 100)
        self.assertEqual(animation.colors, ['#ff0000', '#00ff00', '#0000ff'])
        
        # Check that the file was created
        file_path = self.manager.animation_files[animation_id]
        self.assertTrue(os.path.exists(file_path))
        
        # Check file contents
        with open(file_path, 'r') as f:
            data = json.load(f)
            self.assertIn('animations', data)
            self.assertEqual(len(data['animations']), 1)
            self.assertEqual(data['animations'][0]['name'], 'Test Animation')
        
    def test_delete_animation(self):
        """Test deleting an animation."""
        # First, save an animation
        animation_data = {
            'name': 'Test Animation',
            'type': 'chase',
            'colors': ['#ff0000']
        }
        animation_id = self.manager.save_animation(animation_data)
        
        # Get the file path
        file_path = self.manager.animation_files[animation_id]
        
        # Delete the animation
        result = self.manager.delete_animation(animation_id)
        
        # Check the result
        self.assertTrue(result)
        
        # Check that the animation was removed from memory
        self.assertNotIn(animation_id, self.manager.animations)
        self.assertNotIn(animation_id, self.manager.animation_files)
        
        # Check that the file was deleted
        self.assertFalse(os.path.exists(file_path))
        
    def test_play_animation(self):
        """Test playing an animation."""
        # First, save an animation
        animation_data = {
            'name': 'Test Animation',
            'type': 'chase',
            'colors': ['#ff0000'],
            'frames': [
                {'leds': [{'index': 0, 'color': '#ff0000'}]},
                {'leds': [{'index': 1, 'color': '#ff0000'}]},
                {'leds': [{'index': 2, 'color': '#ff0000'}]}
            ]
        }
        animation_id = self.manager.save_animation(animation_data)
        
        # Set up the protocol mock
        self.protocol.start_stream.return_value = True
        self.protocol.send_rgb_frame.return_value = True
        
        # Play the animation
        result = self.manager.play_animation(animation_id, False)
        
        # Check the result
        self.assertTrue(result)
        self.assertEqual(self.manager.current_animation_id, animation_id)
        self.assertTrue(self.manager.playback_active)
        
        # Wait for playback to complete
        self.manager.playback_thread.join(1.0)
        
        # Check that the protocol methods were called
        self.protocol.send_config.assert_called()
        self.protocol.start_stream.assert_called_once()
        # Should have 3 frames
        self.assertEqual(self.protocol.send_rgb_frame.call_count, 3)

if __name__ == '__main__':
    unittest.main()
