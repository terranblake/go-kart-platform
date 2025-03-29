"""
Animation Manager Module.

This module provides a high-level interface for managing LED animations,
including loading, storing, and playing them on physical hardware.
"""

import os
import json
import time
import threading
import logging
from typing import List, Dict, Any, Tuple, Optional, Callable
import uuid
import copy

from .protocol import AnimationProtocol, CONFIG_FPS, CONFIG_NUM_LEDS, CONFIG_BRIGHTNESS, CONFIG_MODE, MODE_ANIMATION, MODE_OFF

# Configure logging
logger = logging.getLogger(__name__)

class Animation:
    """Represents a single animation with metadata and frames."""
    
    def __init__(self, name: str, animation_data: Dict[str, Any]):
        """
        Initialize an animation from raw data.
        
        Args:
            name: Animation name
            animation_data: Animation data dictionary
        """
        self.id = str(uuid.uuid4())
        self.name = name
        self.type = animation_data.get('type', 'custom')
        self.speed = animation_data.get('speed', 100)
        self.fps = int(1000 / self.speed)
        self.direction = animation_data.get('direction', 'forward')
        
        # Get dimensions
        dimensions = animation_data.get('dimensions', {'length': 60, 'height': 1})
        self.length = dimensions.get('length', 60)
        self.height = dimensions.get('height', 1)
        
        # Load colors
        self.colors = animation_data.get('colors', ['#ff0000'])
        
        # Load frames if available
        self.frames = animation_data.get('frames', [])
        self.current_frame = 0
        
        # Placeholder for processed binary frames
        self.binary_frames = []
        self._process_frames()
        
    def _process_frames(self) -> None:
        """Process animation frames into binary data format."""
        self.binary_frames = []
        
        # If no frames provided, create simple frames based on animation type
        if not self.frames:
            self._generate_frames()
            
        # Process each frame to RGB data
        for frame in self.frames:
            frame_data = []
            if 'leds' in frame:
                # Handle 1D and 2D formats
                for led_data in frame['leds']:
                    if 'index' in led_data:
                        # 1D format
                        index = led_data['index']
                        if 0 <= index < self.length * self.height:
                            color = self._hex_to_rgb(led_data['color'])
                            frame_data.append((index, color))
                    elif 'x' in led_data and 'y' in led_data:
                        # 2D format
                        x, y = led_data['x'], led_data['y']
                        if 0 <= x < self.length and 0 <= y < self.height:
                            index = y * self.length + x
                            color = self._hex_to_rgb(led_data['color'])
                            frame_data.append((index, color))
            
            # Sort by index and extract just RGB values
            frame_data.sort(key=lambda item: item[0])  # Sort by LED index
            
            # Create a full frame with all LEDs (default to off)
            full_frame = [(0, 0, 0) for _ in range(self.length * self.height)]
            
            # Update with actual values
            for index, (r, g, b) in frame_data:
                if 0 <= index < len(full_frame):
                    full_frame[index] = (r, g, b)
            
            self.binary_frames.append(full_frame)
    
    def _generate_frames(self) -> None:
        """Generate frames for basic animation types."""
        # This is a simplified version - in a real implementation, you would 
        # generate proper animations based on type, colors, etc.
        if self.type == 'chase':
            for i in range(self.length):
                frame = {'leds': []}
                color = self.colors[i % len(self.colors)]
                frame['leds'].append({'index': i, 'color': color})
                self.frames.append(frame)
        elif self.type == 'pulse':
            # Simple fade in/out
            for i in range(10):
                frame = {'leds': []}
                alpha = i / 10.0
                for j in range(self.length * self.height):
                    color_index = j % len(self.colors)
                    color = self._adjust_brightness(self.colors[color_index], alpha)
                    frame['leds'].append({'index': j, 'color': color})
                self.frames.append(frame)
            # Fade out
            for i in range(10):
                frame = {'leds': []}
                alpha = (10 - i) / 10.0
                for j in range(self.length * self.height):
                    color_index = j % len(self.colors)
                    color = self._adjust_brightness(self.colors[color_index], alpha)
                    frame['leds'].append({'index': j, 'color': color})
                self.frames.append(frame)
        else:
            # Default - static color
            frame = {'leds': []}
            for i in range(self.length * self.height):
                color_index = i % len(self.colors)
                frame['leds'].append({'index': i, 'color': self.colors[color_index]})
            self.frames.append(frame)
    
    def _hex_to_rgb(self, hex_color: str) -> Tuple[int, int, int]:
        """Convert hex color string to RGB tuple."""
        hex_color = hex_color.lstrip('#')
        return tuple(int(hex_color[i:i+2], 16) for i in (0, 2, 4))
    
    def _adjust_brightness(self, hex_color: str, alpha: float) -> str:
        """Adjust color brightness by alpha factor."""
        r, g, b = self._hex_to_rgb(hex_color)
        r = int(r * alpha)
        g = int(g * alpha)
        b = int(b * alpha)
        return f'#{r:02x}{g:02x}{b:02x}'
    
    def get_frame(self, frame_index: int) -> List[Tuple[int, int, int]]:
        """
        Get the RGB data for a specific frame.
        
        Args:
            frame_index: Index of the frame to retrieve
            
        Returns:
            List of RGB tuples for each LED
        """
        if not self.binary_frames:
            return []
            
        normalized_index = frame_index % len(self.binary_frames)
        return self.binary_frames[normalized_index]
    
    def to_dict(self) -> Dict[str, Any]:
        """
        Convert animation to a dictionary representation.
        
        Returns:
            Dictionary with animation data
        """
        return {
            'id': self.id,
            'name': self.name,
            'type': self.type,
            'speed': self.speed,
            'direction': self.direction,
            'dimensions': {
                'length': self.length,
                'height': self.height
            },
            'colors': self.colors,
            'frames': self.frames,
            'frame_count': len(self.frames)
        }


class AnimationManager:
    """
    Manages animations, provides storage, playback, and hardware integration.
    """
    
    def __init__(self, protocol: AnimationProtocol, 
                animation_dir: Optional[str] = None,
                default_fps: int = 30,
                default_brightness: int = 255):
        """
        Initialize the animation manager.
        
        Args:
            protocol: The animation protocol handler for hardware communication
            animation_dir: Directory to load/save animations from/to
            default_fps: Default frames per second for playback
            default_brightness: Default brightness (0-255)
        """
        self.protocol = protocol
        self.animation_dir = animation_dir or os.path.join(os.getcwd(), 'animations')
        self.default_fps = default_fps
        self.default_brightness = default_brightness
        
        # Ensure animation directory exists
        # os.makedirs(self.animation_dir, exist_ok=True)
        
        # Collections
        self.animations: Dict[str, Animation] = {}
        self.animation_files: Dict[str, str] = {}  # Maps animation ID to file path
        
        # Playback state
        self.current_animation_id: Optional[str] = None
        self.playback_active = False
        self.playback_thread: Optional[threading.Thread] = None
        self.playback_stop_event = threading.Event()
        
        # Callbacks
        self.on_playback_started = None
        self.on_playback_stopped = None
        self.on_frame_played = None
        
        # Load animations
        self._load_animations()
        
        # Set up protocol callbacks
        protocol.set_callbacks(
            on_stream_completed=self._on_stream_completed,
            on_frame_processed=self._on_frame_processed
        )
        
        logger.info(f"Animation manager initialized with {len(self.animations)} animations")
    
    def _load_animations(self) -> None:
        """Load animations from the animation directory."""
        if not os.path.exists(self.animation_dir):
            logger.warning(f"Animation directory not found: {self.animation_dir}")
            return
        
        print(f"Loading animations from {self.animation_dir}")
        # Find animation files
        animation_files = []
        for root, _, files in os.walk(self.animation_dir):
            for file in files:
                if file.endswith('.json'):
                    animation_files.append(os.path.join(root, file))
        
        # Load each file
        for file_path in animation_files:
            try:
                with open(file_path, 'r') as f:
                    data = json.load(f)
                    
                if 'animations' in data and isinstance(data['animations'], list):
                    for anim_data in data['animations']:
                        if 'name' in anim_data:
                            animation = Animation(anim_data['name'], anim_data)
                            self.animations[animation.id] = animation
                            self.animation_files[animation.id] = file_path
                            logger.info(f"Loaded animation: {animation.name} from {file_path}")
                else:
                    logger.warning(f"Invalid animation file format: {file_path}")
            except Exception as e:
                logger.error(f"Error loading animation file {file_path}: {e}")
    
    def save_animation(self, animation_data: Dict[str, Any]) -> Optional[str]:
        """
        Save a new animation.
        
        Args:
            animation_data: Animation data dictionary
            
        Returns:
            Animation ID if saved successfully, None otherwise
        """
        try:
            if 'name' not in animation_data:
                animation_data['name'] = f"Animation {len(self.animations) + 1}"
                
            animation = Animation(animation_data['name'], animation_data)
            
            # Store in memory
            self.animations[animation.id] = animation
            
            # Save to file
            file_name = f"{animation.name.lower().replace(' ', '_')}_{animation.id[:8]}.json"
            file_path = os.path.join(self.animation_dir, file_name)
            
            with open(file_path, 'w') as f:
                json.dump({
                    'animations': [animation_data]
                }, f, indent=2)
                
            self.animation_files[animation.id] = file_path
            logger.info(f"Saved animation: {animation.name} to {file_path}")
            
            return animation.id
        except Exception as e:
            logger.error(f"Error saving animation: {e}")
            return None
    
    def delete_animation(self, animation_id: str) -> bool:
        """
        Delete an animation.
        
        Args:
            animation_id: ID of the animation to delete
            
        Returns:
            True if deleted successfully
        """
        if animation_id not in self.animations:
            logger.warning(f"Animation not found: {animation_id}")
            return False
            
        # Stop playback if this animation is playing
        if self.current_animation_id == animation_id:
            self.stop_playback()
            
        # Remove from memory
        animation = self.animations.pop(animation_id)
        
        # Delete file if it exists and no other animations are in it
        if animation_id in self.animation_files:
            file_path = self.animation_files.pop(animation_id)
            
            # Check if other animations use this file
            other_animations_in_file = False
            for anim_id, path in self.animation_files.items():
                if path == file_path:
                    other_animations_in_file = True
                    break
                    
            if not other_animations_in_file and os.path.exists(file_path):
                try:
                    os.remove(file_path)
                    logger.info(f"Deleted animation file: {file_path}")
                except Exception as e:
                    logger.error(f"Error deleting animation file {file_path}: {e}")
        
        logger.info(f"Deleted animation: {animation.name}")
        return True
    
    def get_animation(self, animation_id: str) -> Optional[Dict[str, Any]]:
        """
        Get animation data by ID.
        
        Args:
            animation_id: ID of the animation to retrieve
            
        Returns:
            Animation data dictionary, or None if not found
        """
        if animation_id not in self.animations:
            return None
            
        return self.animations[animation_id].to_dict()
    
    def get_all_animations(self) -> List[Dict[str, Any]]:
        """
        Get a list of all animations.
        
        Returns:
            List of animation data dictionaries
        """
        return [anim.to_dict() for anim in self.animations.values()]
    
    def play_animation(self, animation_id: str, loop: bool = True, max_fps: Optional[int] = None) -> bool:
        """
        Start playing an animation on the LED hardware.
        
        Args:
            animation_id: ID of the animation to play
            loop: Whether to loop the animation
            max_fps: Maximum frames per second (None for animation default)
            
        Returns:
            True if playback started successfully
        """
        if animation_id not in self.animations:
            logger.warning(f"Animation not found: {animation_id}")
            return False
            
        # Stop any existing playback
        if self.playback_active:
            self.stop_playback()
            
        animation = self.animations[animation_id]
        self.current_animation_id = animation_id
        
        # Calculate FPS
        fps = max_fps or animation.fps or self.default_fps
        if fps <= 0:
            fps = self.default_fps
            
        # Configure the LED controller
        self.protocol.send_config(CONFIG_FPS, fps)
        self.protocol.send_config(CONFIG_NUM_LEDS, animation.length * animation.height)
        self.protocol.send_config(CONFIG_BRIGHTNESS, self.default_brightness)
        self.protocol.send_config(CONFIG_MODE, MODE_ANIMATION)
        
        # Start a thread for playback
        self.playback_active = True
        self.playback_stop_event.clear()
        
        self.playback_thread = threading.Thread(
            target=self._playback_worker,
            args=(animation, loop, fps),
            daemon=True
        )
        self.playback_thread.start()
        
        logger.info(f"Started playback of animation: {animation.name}")
        
        if self.on_playback_started:
            self.on_playback_started(animation_id)
            
        return True
    
    def stop_playback(self) -> bool:
        """
        Stop the current animation playback.
        
        Returns:
            True if playback was stopped, False if no playback was active
        """
        if not self.playback_active:
            return False
            
        # Signal the playback thread to stop
        self.playback_stop_event.set()
        
        # Wait for the thread to finish
        if self.playback_thread and self.playback_thread.is_alive():
            self.playback_thread.join(1.0)
            
        # Reset playback state
        self.playback_active = False
        animation_id = self.current_animation_id
        self.current_animation_id = None
        
        logger.info("Stopped animation playback")
        
        # End the stream on the controller
        self.protocol.end_stream()
        
        # Reset configuration
        self.protocol.send_config(CONFIG_MODE, MODE_OFF)
        
        if self.on_playback_stopped and animation_id:
            self.on_playback_stopped(animation_id)
            
        return True
    
    def _playback_worker(self, animation: Animation, loop: bool, fps: int) -> None:
        """Worker thread for animation playback."""
        try:
            frame_count = len(animation.binary_frames)
            if frame_count == 0:
                logger.warning(f"Animation {animation.name} has no frames")
                return
                
            # Start the animation stream
            if not self.protocol.start_stream(frame_count, fps, animation.length * animation.height):
                logger.error("Failed to start animation stream")
                return
                
            # Send each frame
            frame_index = 0
            while self.playback_active and not self.playback_stop_event.is_set():
                if frame_index >= frame_count:
                    if not loop:
                        break
                    frame_index = 0
                
                # Get frame data
                frame_data = animation.get_frame(frame_index)
                
                # Send the frame
                if not self.protocol.send_rgb_frame(frame_index, frame_data):
                    logger.error(f"Failed to send frame {frame_index}")
                    break
                    
                # Call frame played callback
                if self.on_frame_played:
                    self.on_frame_played(animation.id, frame_index)
                    
                # Move to next frame
                frame_index += 1
                
                # Wait for next frame
                frame_time = 1.0 / fps
                time.sleep(frame_time)
                
            # End the stream
            self.protocol.end_stream()
            
        except Exception as e:
            logger.error(f"Error in animation playback: {e}")
            
        finally:
            # Ensure playback is stopped
            self.playback_active = False
            
            # Call stopped callback if not already called
            if not self.playback_stop_event.is_set() and self.on_playback_stopped:
                self.on_playback_stopped(animation.id)
    
    def _on_stream_completed(self) -> None:
        """
        Handler called when the controller signals stream completion.
        """
        logger.info("Stream completion acknowledged by controller")
        
        # Nothing more to do here, we already handle playback completion
        # in the playback worker
        
    def _on_frame_processed(self, frame_num: int) -> None:
        """
        Handler called when the controller signals frame processing.
        
        Args:
            frame_num: The frame number that was processed
        """
        logger.debug(f"Frame {frame_num} processed by controller")
        
        # This could be used for synchronization in a more advanced implementation
    
    def set_callbacks(self, 
                     on_playback_started: Optional[Callable[[str], None]] = None,
                     on_playback_stopped: Optional[Callable[[str], None]] = None,
                     on_frame_played: Optional[Callable[[str, int], None]] = None) -> None:
        """
        Set callback functions for animation events.
        
        Args:
            on_playback_started: Called when animation playback starts, with animation ID
            on_playback_stopped: Called when animation playback stops, with animation ID
            on_frame_played: Called when a frame is played, with animation ID and frame number
        """
        self.on_playback_started = on_playback_started
        self.on_playback_stopped = on_playback_stopped
        self.on_frame_played = on_frame_played
    
    def preview_frame(self, animation_id: str, frame_index: int) -> Optional[List[Tuple[int, int, int]]]:
        """
        Get a preview of a specific animation frame.
        
        Args:
            animation_id: ID of the animation
            frame_index: Index of the frame to preview
            
        Returns:
            List of RGB tuples for each LED, or None if not found
        """
        if animation_id not in self.animations:
            return None
            
        animation = self.animations[animation_id]
        return animation.get_frame(frame_index)

    # Added method for real-time frame sending
    def send_realtime_frame(self, led_data: List[Tuple[int, int, int]]) -> bool:
        """
        Send a single frame of LED data directly over CAN for real-time updates.

        Bypasses the stream start/end protocol and sends data using ANIMATION_DATA_ID.
        Assumes the receiving device is already configured and expecting raw frame data.

        Args:
            led_data: List of RGB tuples (r, g, b) for each LED.

        Returns:
            True if all data chunks were sent successfully, False otherwise.
        """
        try:
            # Convert RGB data to binary format: [r0, g0, b0, r1, g1, b1, ...]
            frame_bytes = bytearray()
            for r, g, b in led_data:
                # Ensure values are within byte range
                r = max(0, min(255, int(r)))
                g = max(0, min(255, int(g)))
                b = max(0, min(255, int(b)))
                frame_bytes.extend([r, g, b])

            frame_size = len(frame_bytes)
            chunk_size = self.protocol.max_data_per_message # Use chunk size from protocol (usually 8)
            total_chunks = (frame_size + chunk_size - 1) // chunk_size
            bytes_sent = 0

            logger.debug(f"Sending real-time frame: {frame_size} bytes in {total_chunks} chunks")

            # Send data in chunks using the raw CAN interface
            for i in range(total_chunks):
                start_pos = i * chunk_size
                end_pos = min(start_pos + chunk_size, frame_size)
                chunk = bytes(frame_bytes[start_pos:end_pos]) # Convert slice to bytes

                if not self.protocol.can_interface.send_raw_message(ANIMATION_DATA_ID, chunk):
                    logger.error(f"Failed to send real-time data chunk {i+1}/{total_chunks}")
                    return False

                bytes_sent += len(chunk)

                # Optional small delay to prevent flooding the CAN bus, adjust if needed
                # time.sleep(0.001) # 1ms delay

            logger.debug(f"Successfully sent real-time frame ({bytes_sent} bytes)")
            return True

        except Exception as e:
            logger.error(f"Error sending real-time frame: {str(e)}")
            return False
