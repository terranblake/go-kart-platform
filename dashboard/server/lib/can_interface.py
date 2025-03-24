# for the entire can interface implementation

import can
import time
import struct
import logging
import collections
import os
from datetime import datetime

from lib.telemetry import GoKartState
from lib.protocol import (
    load_protocol_definitions, 
    get_command_value, 
    get_command_values,
    get_component_type_id, 
    get_component_id, 
    get_command_id, 
    get_message_header
)

# Configure logging
logger = logging.getLogger(__name__)

class CANCommandGenerator:
    """Class to generate CAN messages for the go-kart."""

    # Command IDs
    COMMAND_ID = 0x100
    DEVICE_NANO = 0x02
    CMD_EMERGENCY_STOP = 0x0A
    CMD_SPEED_CONTROL = 0x0B
    CMD_STEERING_CONTROL = 0x0C
    CMD_BRAKE_CONTROL = 0x0D
    CMD_LIGHTS_CONTROL = 0x0E
    CMD_LIGHTS_MODE = 0x0F
    CMD_LIGHTS_SIGNAL = 0x10
    CMD_LIGHTS_BRAKE = 0x11

    def __init__(self, channel='vcan0', bitrate=500000):
        """Initialize CAN command generator."""
        try:
            self.bus = can.interface.Bus(channel=channel, bustype='socketcan', bitrate=bitrate)
            logger.info(f"CAN bus initialized on channel {channel}")
        except Exception as e:
            logger.error(f"Error initializing CAN bus: {e}")
            self.bus = None
            
        # Initialize state tracking
        self.state = GoKartState()
        self.history = collections.deque(maxlen=600)  # Store 1 minute of data at 10Hz
        self.light_states = {}
        
        # Load protocol definitions
        self.protocol = load_protocol_definitions('protocol.json')

    def get_command_value(self, command_type, command, value_name):
        """Get numeric value for a command value by name."""
        return get_command_value(self.protocol, command_type, command, value_name)

    def get_command_values(self, command_type, command):
        """Get all possible values for a command."""
        return get_command_values(self.protocol, command_type, command)

    def get_component_type_id(self, component_type):
        """Get numeric ID for a component type."""
        return get_component_type_id(self.protocol, component_type)

    def get_component_id(self, component_type, component):
        """Get numeric ID for a component."""
        return get_component_id(self.protocol, component_type, component)

    def get_command_id(self, command_type, command):
        """Get numeric ID for a command."""
        return get_command_id(self.protocol, command_type, command)

    def get_message_header(self, command_type, command):
        """Generate message header for a command."""
        return get_message_header(self.protocol, command_type, command)

    def send_can_message(self, command_type, command, component, value):
        """Send a CAN message using the protocol."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
            
        header = self.get_message_header(command_type, command)
        component_id = self.get_component_id(command_type, component)
        
        if header is None or header == 0 or component_id is None or component_id == 0:
            logger.error(f"Invalid protocol definition for {command_type}.{command}.{component}")
            return False
            
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[header, component_id, value],
            is_extended_id=False
        )
        
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending {command_type}.{command} message: {e}")
            return False

    def listen_telemetry(self, test_mode=False):
        """Listen for incoming telemetry messages."""
        while True:
            try:
                if not self.bus:
                    logger.error("CAN bus not initialized")
                    time.sleep(0.1)
                    if test_mode:
                        return
                    continue
                    
                message = self.bus.recv(timeout=0.1)
                if message is None:
                    # Handle timeout case
                    logger.debug("CAN bus receive timeout")
                    if test_mode:
                        return
                    continue
                
                # Check if valid device ID
                if len(message.data) < 1 or message.data[0] != self.DEVICE_NANO:
                    logger.debug(f"Ignoring message with invalid device ID: {message.data[0] if len(message.data) > 0 else 'empty'}")
                    if test_mode:
                        return
                    continue
                
                # Parse messages based on ID
                if message.arbitration_id == 0x100:  # Speed Throttle
                    self.state.speed = struct.unpack('>h', message.data[1:3])[0] / 10.0
                    self.state.throttle = message.data[3]
                    logger.debug(f"Speed: {self.state.speed:.1f} km/h, Throttle: {self.state.throttle}%")
                
                elif message.arbitration_id == 0x101:  # Battery Motor
                    self.state.battery_voltage = struct.unpack('>h', message.data[1:3])[0] / 10.0
                    self.state.motor_temp = struct.unpack('>h', message.data[3:5])[0] / 10.0
                    logger.debug(f"Battery: {self.state.battery_voltage:.1f} V, Temp: {self.state.motor_temp:.1f} C")
                
                elif message.arbitration_id == 0x102:  # Brake Steering
                    self.state.brake_pressure = struct.unpack('>h', message.data[1:3])[0] / 10.0
                    self.state.steering_angle = struct.unpack('>h', message.data[3:5])[0] / 10.0
                    logger.debug(f"Brake: {self.state.brake_pressure:.1f} %, Steering: {self.state.steering_angle:.1f} deg")
                
                # Update timestamp and add to history
                self.state.timestamp = datetime.now().timestamp()
                self.history.append(self.state.to_dict())
                
                # If in test mode, just process one message and return
                if test_mode:
                    return
                
            except Exception as e:
                logger.error(f"Error in telemetry: {e}")
                time.sleep(0.1)
                # If in test mode, raise the exception so the test can fail properly
                if test_mode:
                    raise

    def get_current_state(self):
        """Return the current state as a dictionary."""
        state_dict = self.state.to_dict()
        
        # Add light states
        state_dict.update({
            'light_mode': self.light_states.get('lights_mode', 0),
            'light_signal': self.light_states.get('lights_signal', 0),
            'light_brake': 1 if self.light_states.get('lights_brake', 0) == 0xFF else 0,
            'light_test': 1 if self.light_states.get('lights_test', 0) == 0xFF else 0,
        })
        
        return state_dict

    def get_history(self):
        """Return telemetry history as a list of dictionaries."""
        return list(self.history)
        
    def get_speed(self):
        """Return the current speed."""
        return self.state.speed
        
    def get_battery_voltage(self):
        """Return the current battery voltage."""
        return self.state.battery_voltage
        
    def get_brake_pressure(self):
        """Return the current brake pressure."""
        return self.state.brake_pressure

    def send_emergency_stop(self, activate=True):
        """Send emergency stop command"""
        # Emergency stop should ignore the CAN bus initialization check
        # for safety reasons
        
        # As a backup, also trigger OS-level emergency stop mechanism
        try:
            os.system('sudo killall motor_controller')
        except:
            pass
            
        # If CAN is available, also send the CAN command
        if self.bus:
            message = can.Message(
                arbitration_id=self.COMMAND_ID,
                data=[self.DEVICE_NANO, self.CMD_EMERGENCY_STOP, 0xFF if activate else 0x00],
                is_extended_id=False
            )
            try:
                self.bus.send(message)
                logger.info(f"EMERGENCY STOP {'ACTIVATED' if activate else 'DEACTIVATED'}")
                return True
            except Exception as e:
                logger.error(f"Error sending emergency stop: {e}")
                
        return False

    def send_speed_command(self, speed):
        """Send speed control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        # Limit speed to 0-100%
        speed = max(0, min(100, speed))
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_SPEED_CONTROL, speed],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending speed command: {e}")
            return False

    def send_steering_command(self, angle):
        """Send steering control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        # Limit angle to -45 to +45 degrees
        angle = max(-45, min(45, angle))
        # Convert to 0-90 range for CAN message
        can_angle = angle + 45
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_STEERING_CONTROL, can_angle],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending steering command: {e}")
            return False

    def send_brake_command(self, pressure):
        """Send brake control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        # Limit pressure to 0-100%
        pressure = max(0, min(100, pressure))
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_BRAKE_CONTROL, pressure],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending brake command: {e}")
            return False

    def send_lights_command(self, lights_on):
        """Send lights control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if lights_on else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending lights command: {e}")
            return False

    def send_light_mode_by_index(self, mode_index):
        """Set light mode by index (0=off, 1=low, 2=high, 3=hazard)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        # Validate mode index
        if mode_index < 0 or mode_index > 3:
            logger.error(f"Invalid light mode index: {mode_index}")
            return False
        
        # Convert mode index to value
        value = mode_index
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, value],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_mode'] = mode_index
            logger.info(f"Light Mode: {['OFF', 'LOW', 'HIGH', 'HAZARD'][mode_index]}")
            return True
        except Exception as e:
            logger.error(f"Error sending light mode command: {e}")
            return False

    def send_light_mode(self, mode_name):
        """Set light mode by name (off, low, high, hazard)"""
        mode_map = {'off': 0, 'low': 1, 'high': 2, 'hazard': 3}
        if mode_name.lower() not in mode_map:
            logger.error(f"Invalid light mode: {mode_name}")
            return False
        
        return self.send_light_mode_by_index(mode_map[mode_name.lower()])

    def send_light_signal_by_index(self, signal_index):
        """Set turn signal by index (0=off, 1=left, 2=right)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        # Validate signal index
        if signal_index < 0 or signal_index > 2:
            logger.error(f"Invalid turn signal index: {signal_index}")
            return False
        
        # Convert signal index to value
        value = signal_index
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, value],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_signal'] = signal_index
            logger.info(f"Turn Signal: {['OFF', 'LEFT', 'RIGHT'][signal_index]}")
            return True
        except Exception as e:
            logger.error(f"Error sending turn signal command: {e}")
            return False

    def send_light_signal(self, signal_name):
        """Set turn signal by name (off, left, right)"""
        signal_map = {'off': 0, 'left': 1, 'right': 2}
        if signal_name.lower() not in signal_map:
            logger.error(f"Invalid turn signal: {signal_name}")
            return False
        
        return self.send_light_signal_by_index(signal_map[signal_name.lower()])

    def send_light_brake(self, state):
        """Set brake lights (on/off)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if state else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_brake'] = 0xFF if state else 0x00
            logger.info(f"Brake Lights: {'ON' if state else 'OFF'}")
            return True
        except Exception as e:
            logger.error(f"Error sending brake lights command: {e}")
            return False

    def send_light_test(self, enable):
        """Enable/disable light test mode"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if enable else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_test'] = 0xFF if enable else 0x00
            logger.info(f"Light Test Mode: {'ON' if enable else 'OFF'}")
            return True
        except Exception as e:
            logger.error(f"Error sending light test command: {e}")
            return False

    def send_light_location(self, is_front):
        """Set light controller location (front/rear)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if is_front else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            logger.info(f"Light Location Set: {'FRONT' if is_front else 'REAR'}")
            return True
        except Exception as e:
            logger.error(f"Error sending light location command: {e}")
            return False

    def send_light_mode_by_name(self, mode_name):
        """Send light mode command by name"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
            
        # Convert mode name to protocol value
        try:
            mode_value = get_command_value('lights', 'mode', mode_name)
            
            message = can.Message(
                arbitration_id=self.COMMAND_ID,
                data=[self.DEVICE_NANO, self.CMD_LIGHTS_MODE, int(mode_value, 16)],
                is_extended_id=False
            )
            self.bus.send(message)
            logger.info(f"Light Mode Set: {mode_name}")
            return True
        except Exception as e:
            logger.error(f"Error sending light mode command: {e}")
            return False
            
    def send_signal_by_name(self, signal_name):
        """Send signal light command by name"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
            
        # Convert signal name to protocol value
        try:
            signal_value = get_command_value('lights', 'signal', signal_name)
            
            message = can.Message(
                arbitration_id=self.COMMAND_ID,
                data=[self.DEVICE_NANO, self.CMD_LIGHTS_SIGNAL, int(signal_value, 16)],
                is_extended_id=False
            )
            self.bus.send(message)
            logger.info(f"Signal Light Set: {signal_name}")
            return True
        except Exception as e:
            logger.error(f"Error sending signal light command: {e}")
            return False
            
    def send_brake_light(self, activate):
        """Send brake light command"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
            
        try:
            brake_value = get_command_value('lights', 'brake', 'on' if activate else 'off')
            
            message = can.Message(
                arbitration_id=self.COMMAND_ID,
                data=[self.DEVICE_NANO, self.CMD_LIGHTS_BRAKE, int(brake_value, 16)],
                is_extended_id=False
            )
            self.bus.send(message)
            logger.info(f"Brake Light: {'ON' if activate else 'OFF'}")
            return True
        except Exception as e:
            logger.error(f"Error sending brake light command: {e}")
            return False
