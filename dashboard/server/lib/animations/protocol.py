"""
Animation Protocol Module.

This module defines the low-level protocol for transmitting LED animation data
over the CAN bus using the raw message interface.

The protocol implements:
- Frame-based binary data streaming
- Stream control messages (start/end)
- Configuration settings
"""

import logging
import struct
import time
from typing import List, Dict, Any, Tuple, Optional, Callable

from lib.can.interface import CANInterfaceWrapper

# Configure logging
logger = logging.getLogger(__name__)

# Constants for animation protocol
ANIMATION_BASE_ID = 0x700  # Base CAN ID for animation messages
ANIMATION_DATA_ID = 0x701  # CAN ID for animation data frames
ANIMATION_CTRL_ID = 0x700  # CAN ID for animation control messages

# Control command bytes (first byte in message payload)
CMD_STREAM_START = 0x01
CMD_STREAM_END = 0x02
CMD_FRAME_START = 0x03
CMD_FRAME_END = 0x04
CMD_CONFIG = 0x05

# Configuration parameter IDs (second byte in CONFIG messages)
CONFIG_FPS = 0x01
CONFIG_NUM_LEDS = 0x02
CONFIG_BRIGHTNESS = 0x03
CONFIG_MODE = 0x04

# Mode values
MODE_STATIC = 0x01
MODE_ANIMATION = 0x02
MODE_OFF = 0x00

class AnimationProtocol:
    """
    Protocol handler for transmitting LED animation data over CAN.
    
    This class implements a binary streaming protocol built on top of the
    raw CAN message interface. It supports:
    
    1. Sending configuration parameters (FPS, LED count, etc.)
    2. Streaming animation frame data
    3. Stream control with start/end markers
    4. Status tracking and callback notifications
    """

    def __init__(self, can_interface: CANInterfaceWrapper):
        """
        Initialize the animation protocol handler.
        
        Args:
            can_interface: The CAN interface wrapper to use for sending messages
        """
        self.can_interface = can_interface
        self.stream_active = False
        self.frame_active = False
        self.current_frame = 0
        self.max_data_per_message = 8  # Maximum bytes per CAN frame
        
        # Register for status messages
        can_interface.register_raw_handler(ANIMATION_CTRL_ID, self._handle_control_message)
        
        # Callbacks
        self.on_stream_completed = None
        self.on_frame_processed = None
        
        logger.info("Animation protocol initialized")

    def _handle_control_message(self, can_id: int, data: bytes, length: int) -> None:
        """
        Handle control messages received from animations hardware.
        
        Args:
            can_id: The CAN ID of the message
            data: Raw message data bytes
            length: Length of the data
        """
        if length < 1:
            logger.warning(f"Received too short control message, length={length}")
            return
            
        cmd = data[0]
        
        if cmd == CMD_STREAM_END:
            logger.info("Received stream end acknowledgement")
            self.stream_active = False
            if self.on_stream_completed:
                self.on_stream_completed()
                
        elif cmd == CMD_FRAME_END:
            if length >= 2:
                frame_num = data[1]
                logger.debug(f"Received frame end acknowledgement for frame {frame_num}")
                if self.on_frame_processed:
                    self.on_frame_processed(frame_num)
            else:
                logger.warning("Received frame end without frame number")

    def start_stream(self, num_frames: int, fps: int, num_leds: int) -> bool:
        """
        Start an animation stream.
        
        Args:
            num_frames: Number of frames in the animation
            fps: Frames per second
            num_leds: Number of LEDs in the strip
            
        Returns:
            True if the stream was started successfully
        """
        if self.stream_active:
            logger.warning("Attempted to start a stream while another is active")
            return False
            
        # Build and send start stream message
        data = bytes([CMD_STREAM_START, num_frames, fps, num_leds & 0xFF, (num_leds >> 8) & 0xFF])
        
        logger.info(f"Starting animation stream: {num_frames} frames, {fps} FPS, {num_leds} LEDs")
        
        if self.can_interface.send_raw_message(ANIMATION_CTRL_ID, data):
            self.stream_active = True
            self.current_frame = 0
            return True
        else:
            logger.error("Failed to send stream start message")
            return False

    def end_stream(self) -> bool:
        """
        End the current animation stream.
        
        Returns:
            True if the end message was sent successfully
        """
        if not self.stream_active:
            logger.warning("Attempted to end a stream that is not active")
            return False
            
        # Build and send end stream message
        data = bytes([CMD_STREAM_END])
        
        logger.info("Ending animation stream")
        
        if self.can_interface.send_raw_message(ANIMATION_CTRL_ID, data):
            # Don't reset stream_active here - wait for acknowledgement
            return True
        else:
            logger.error("Failed to send stream end message")
            return False

    def start_frame(self, frame_num: int, frame_size: int) -> bool:
        """
        Start a new animation frame.
        
        Args:
            frame_num: Frame number (sequential)
            frame_size: Size of the frame data in bytes
            
        Returns:
            True if the frame start message was sent successfully
        """
        if not self.stream_active:
            logger.warning("Attempted to start a frame outside an active stream")
            return False
            
        if self.frame_active:
            logger.warning("Attempted to start a frame while another is active")
            return False
            
        # Build and send frame start message
        data = bytes([
            CMD_FRAME_START, 
            frame_num, 
            frame_size & 0xFF,
            (frame_size >> 8) & 0xFF
        ])
        
        logger.debug(f"Starting frame {frame_num}, size {frame_size} bytes")
        
        if self.can_interface.send_raw_message(ANIMATION_CTRL_ID, data):
            self.frame_active = True
            self.current_frame = frame_num
            return True
        else:
            logger.error(f"Failed to send frame start message for frame {frame_num}")
            return False

    def end_frame(self, frame_num: int) -> bool:
        """
        End the current animation frame.
        
        Args:
            frame_num: Frame number (should match the current frame)
            
        Returns:
            True if the frame end message was sent successfully
        """
        if not self.stream_active or not self.frame_active:
            logger.warning("Attempted to end a frame that is not active")
            return False
            
        if frame_num != self.current_frame:
            logger.warning(f"Frame number mismatch: current={self.current_frame}, ending={frame_num}")
            # Continue anyway
            
        # Build and send frame end message
        data = bytes([CMD_FRAME_END, frame_num])
        
        logger.debug(f"Ending frame {frame_num}")
        
        if self.can_interface.send_raw_message(ANIMATION_CTRL_ID, data):
            self.frame_active = False
            return True
        else:
            logger.error(f"Failed to send frame end message for frame {frame_num}")
            return False

    def send_frame_data(self, frame_data: bytes, chunk_size: int = 8, delay_ms: int = 0) -> bool:
        """
        Send animation frame data in chunks.
        
        Args:
            frame_data: The binary frame data to send
            chunk_size: Chunk size in bytes (max 8)
            delay_ms: Optional delay between chunks in milliseconds
            
        Returns:
            True if all data was sent successfully
        """
        if not self.stream_active or not self.frame_active:
            logger.warning("Attempted to send frame data outside an active frame")
            return False
            
        if chunk_size > self.max_data_per_message:
            chunk_size = self.max_data_per_message
            logger.warning(f"Chunk size limited to {chunk_size} bytes")
            
        # Send data in chunks
        total_chunks = (len(frame_data) + chunk_size - 1) // chunk_size
        bytes_sent = 0
        
        logger.debug(f"Sending {len(frame_data)} bytes in {total_chunks} chunks")
        
        for i in range(total_chunks):
            start_pos = i * chunk_size
            end_pos = min(start_pos + chunk_size, len(frame_data))
            chunk = frame_data[start_pos:end_pos]
            
            if not self.can_interface.send_raw_message(ANIMATION_DATA_ID, chunk):
                logger.error(f"Failed to send data chunk {i+1}/{total_chunks}")
                return False
                
            bytes_sent += len(chunk)
            
            # Optional delay between chunks
            if delay_ms > 0 and i < total_chunks - 1:
                time.sleep(delay_ms / 1000)
        
        logger.debug(f"Successfully sent {bytes_sent} bytes")
        return True

    def send_config(self, param_id: int, value: int) -> bool:
        """
        Send a configuration parameter to the animation controller.
        
        Args:
            param_id: Parameter ID (see CONFIG_* constants)
            value: Parameter value (up to 32 bits)
            
        Returns:
            True if the config message was sent successfully
        """
        # Build config message: [CMD_CONFIG, param_id, value_bytes...]
        data = bytearray([CMD_CONFIG, param_id])
        
        # Encode value as little-endian bytes
        if param_id in (CONFIG_FPS, CONFIG_BRIGHTNESS, CONFIG_MODE):
            # 8-bit value
            data.append(value & 0xFF)
        elif param_id == CONFIG_NUM_LEDS:
            # 16-bit value
            data.extend([value & 0xFF, (value >> 8) & 0xFF])
        else:
            # Default to 32-bit value
            data.extend([
                value & 0xFF,
                (value >> 8) & 0xFF,
                (value >> 16) & 0xFF,
                (value >> 24) & 0xFF
            ])
        
        logger.info(f"Sending config: param_id={param_id}, value={value}")
        
        return self.can_interface.send_raw_message(ANIMATION_CTRL_ID, bytes(data))
        
    def send_rgb_frame(self, frame_num: int, led_data: List[Tuple[int, int, int]]) -> bool:
        """
        Send a complete RGB animation frame.
        
        This is a convenience method that handles the frame start/data/end protocol.
        
        Args:
            frame_num: Frame number
            led_data: List of RGB tuples (r, g, b) for each LED
            
        Returns:
            True if the frame was sent successfully
        """
        # Convert RGB data to binary format
        # Format: [r0, g0, b0, r1, g1, b1, ...]
        frame_bytes = bytearray()
        for r, g, b in led_data:
            frame_bytes.extend([r & 0xFF, g & 0xFF, b & 0xFF])
            
        frame_size = len(frame_bytes)
        
        # Start the frame
        if not self.start_frame(frame_num, frame_size):
            return False
            
        # Send the frame data
        if not self.send_frame_data(frame_bytes):
            return False
            
        # End the frame
        return self.end_frame(frame_num)

    def set_callbacks(self, on_stream_completed: Optional[Callable[[], None]] = None, 
                     on_frame_processed: Optional[Callable[[int], None]] = None) -> None:
        """
        Set callback functions for animation events.
        
        Args:
            on_stream_completed: Called when a stream is acknowledged as complete
            on_frame_processed: Called when a frame is acknowledged as processed,
                               with the frame number as parameter
        """
        self.on_stream_completed = on_stream_completed
        self.on_frame_processed = on_frame_processed
