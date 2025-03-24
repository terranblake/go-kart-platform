"""
Python interface to the CAN bus using Cython bindings to ProtobufCANInterface
"""

import time
import logging
import collections
from datetime import datetime
import can
import threading

from ._can_interface import PyCANInterface, MessageType, ComponentType, ValueType
from .protocol import load_protocol_definitions, get_command_by_name
from ..telemetry import GoKartState

# Configure logging
logger = logging.getLogger(__name__)

class CANCommandGenerator:
    """Class to generate CAN messages for the go-kart using Protocol Buffers."""
    
    def __init__(self, channel='vcan0', bitrate=500000, node_id=0x01):
        """Initialize CAN command generator."""
        # Initialize socketcan interface for compatibility with existing code
        try:
            self.socketcan_bus = can.interface.Bus(channel=channel, bustype='socketcan', bitrate=bitrate)
            logger.info(f"SocketCAN bus initialized on channel {channel}")
        except Exception as e:
            logger.error(f"Error initializing SocketCAN bus: {e}")
            self.socketcan_bus = None
        
        # Initialize our Protocol Buffer CAN interface
        self.can_interface = PyCANInterface(node_id)
        if not self.can_interface.begin(bitrate):
            logger.error("Failed to initialize Protocol Buffer CAN interface")
        else:
            logger.info("Protocol Buffer CAN interface initialized successfully")
            
        # Initialize state tracking
        self.state = GoKartState()
        self.history = collections.deque(maxlen=600)  # Store 1 minute of data at 10Hz
        self.light_states = {}
        
        # Load protocol definitions from .proto files
        self.protocol = load_protocol_definitions()
        
        # Start processing thread
        self.running = True
        self.process_thread = threading.Thread(target=self._process_messages, daemon=True)
        self.process_thread.start()
    
    def _process_messages(self):
        """Process incoming CAN messages in a background thread."""
        while self.running:
            try:
                self.can_interface.process()
                time.sleep(0.01)  # Small sleep to prevent CPU hogging
            except Exception as e:
                logger.error(f"Error processing CAN messages: {e}")
                time.sleep(0.1)
    
    def _update_state_from_message(self, message_type, component_type, component_id, command_id, value_type, value):
        """Update state based on incoming message."""
        timestamp = datetime.now().timestamp()
        
        # Update based on component type
        if component_type == ComponentType.MOTORS:
            if command_id == 0x01:  # Speed
                self.state.speed = value / 10.0
            elif command_id == 0x02:  # Throttle
                self.state.throttle = value
            elif command_id == 0x03:  # Temperature
                self.state.motor_temp = value / 10.0
                
        elif component_type == ComponentType.BATTERY:
            if command_id == 0x01:  # Voltage
                self.state.battery_voltage = value / 10.0
                
        elif component_type == ComponentType.CONTROLS:
            if command_id == 0x01:  # Brake pressure
                self.state.brake_pressure = value / 10.0
            elif command_id == 0x02:  # Steering angle
                self.state.steering_angle = value / 10.0
        
        # Update timestamp and add to history
        self.state.timestamp = timestamp
        self.history.append(self.state.to_dict())
    
    def send_message(self, component_type_name, component_name, command_name, value_name=None, value=0):
        """Send a message using component, command, and value names."""
        # Get command info
        command_info = get_command_by_name(self.protocol, component_type_name, component_name, command_name)
        if not command_info:
            logger.error(f"Command not found: {component_type_name}.{component_name}.{command_name}")
            return False
        
        # Get value info if a name is provided
        if value_name and value_name in command_info.get('values', {}):
            value = command_info['values'][value_name]
        
        # Send the message
        return self.can_interface.send_message(
            command_info['message_type'],
            command_info['component_type'],
            command_info['component_id'],
            command_info['command_id'],
            command_info.get('value_type', ValueType.INT16),
            value
        )
    
    # Legacy API compatibility
    def send_emergency_stop(self, activate=True):
        """Send emergency stop command"""
        return self.send_message('controls', 'security', 'emergency', 'STOP' if activate else 'NORMAL')
        
    def send_speed_command(self, speed):
        """Send speed control command."""
        # Limit speed to 0-100%
        speed = max(0, min(100, int(speed)))
        return self.send_message('controls', 'throttle', 'parameter', None, speed)
        
    def send_steering_command(self, angle):
        """Send steering control command."""
        # Limit angle to -45 to +45 degrees
        angle = max(-45, min(45, int(angle)))
        return self.send_message('controls', 'steering', 'parameter', None, angle)
        
    def send_brake_command(self, pressure):
        """Send brake control command."""
        # Limit pressure to 0-100%
        pressure = max(0, min(100, int(pressure)))
        return self.send_message('controls', 'brake', 'parameter', None, pressure)
    
    def send_light_mode_by_name(self, mode_name):
        """Send light mode command by name."""
        return self.send_message('lights', 'front', 'mode', mode_name)
    
    def send_signal_by_name(self, signal_name):
        """Send turn signal command by name."""
        return self.send_message('lights', 'front', 'signal', signal_name)
    
    def send_brake_light(self, activate=True):
        """Send brake light command."""
        return self.send_message('lights', 'rear', 'brake', 'ON' if activate else 'OFF')
    
    # State retrieval methods
    def get_current_state(self):
        """Return the current state as a dictionary."""
        state_dict = self.state.to_dict()
        
        # Add light states
        state_dict.update({
            'light_mode': self.light_states.get('mode', 0),
            'light_signal': self.light_states.get('signal', 0),
            'light_brake': 1 if self.light_states.get('brake', False) else 0
        })
        
        return state_dict
        
    def get_history(self):
        """Return telemetry history as a list of dictionaries."""
        return list(self.history)
    
    def __del__(self):
        """Clean up resources."""
        self.running = False
        if hasattr(self, 'process_thread') and self.process_thread.is_alive():
            self.process_thread.join(timeout=1.0) 