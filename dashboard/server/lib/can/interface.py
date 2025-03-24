"""
Python interface for the CrossPlatformCAN library

This module provides a higher-level interface for the CANInterface.
It provides a way to send and receive CAN messages using the protocol definitions.

The CANInterfaceWrapper is a wrapper around the CANInterface that provides a higher-level API
for sending and receiving CAN messages.

It is responsible for:
- validating the message using the ProtocolRegistry.create_message()
- sending the message using the CANInterface.send_message()
- receiving the message using the CANInterface.process()
- calling the appropriate handler for the message

It is not responsible for:
- creating the protocol definitions
- loading the protocol definitions
- registering the handlers
- processing the messages
- storing state
- starting the CANInterface
- stopping the CANInterface
"""
import logging
import os
import time
import threading
import platform
from typing import Callable, Dict, Any, Optional, List, Tuple
from cffi import FFI

from lib.can.protocol_registry import ProtocolRegistry

# Configure logging
logger = logging.getLogger(__name__)

# Define the C API for the CrossPlatformCAN library
ffi = FFI()
ffi.cdef("""
    typedef void* can_interface_t;
    
    // Constructor/destructor
    can_interface_t can_interface_create(uint32_t node_id);
    void can_interface_destroy(can_interface_t handle);
    
    // Member function wrappers
    bool can_interface_begin(can_interface_t handle, long baudrate, const char* device);
    
    // Handler registration
    void can_interface_register_handler(
        can_interface_t handle,
        int comp_type,
        uint8_t component_id,
        uint8_t command_id,
        void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)
    );
    
    // Message sending
    bool can_interface_send_message(
        can_interface_t handle,
        int msg_type,
        int comp_type,
        uint8_t component_id,
        uint8_t command_id,
        int value_type,
        int32_t value
    );
    
    // Message processing
    void can_interface_process(can_interface_t handle);
""")

# Global flag to track if we have hardware CAN support
has_can_hardware = False
lib = None

# Try to load the library, but don't fail if it can't be loaded
try:
    lib_path = os.path.join(os.path.dirname(__file__), "libcaninterface.so")
    logger.info(f"Loading CAN interface library from: {lib_path}")
    lib = ffi.dlopen(lib_path)
    has_can_hardware = True
    logger.info("CAN interface library loaded successfully")
except Exception as e:
    logger.warning(f"Failed to load CAN interface library: {e}")
    logger.warning("Running in simulation mode without CAN hardware")
    has_can_hardware = False

# Message handler type
MessageHandlerCallback = Callable[[int, int, int, int, int, int], None]

class MockCANInterface:
    """
    A mock implementation of the CAN interface for when hardware is not available.
    This allows the server to run in a simulation mode.
    """
    def __init__(self, node_id):
        """Initialize the mock CAN interface"""
        self.node_id = node_id
        logger.info(f"Created mock CAN interface with node_id={node_id}")
        self.handlers = {}
        
    def begin(self, baudrate, device):
        """Mock implementation of begin()"""
        logger.info(f"Mock CAN interface initialized with baudrate={baudrate}, device={device}")
        return True
        
    def register_handler(self, comp_type, comp_id, cmd_id, handler):
        """Mock implementation of register_handler()"""
        key = (comp_type, comp_id, cmd_id)
        self.handlers[key] = handler
        logger.debug(f"Registered mock handler for comp_type={comp_type}, comp_id={comp_id}, cmd_id={cmd_id}")
        return True
        
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, value_type, value):
        """Mock implementation of send_message()"""
        logger.info(f"Mock send message: msg_type={msg_type}, comp_type={comp_type}, " 
                   f"comp_id={comp_id}, cmd_id={cmd_id}, value_type={value_type}, value={value}")
        return True
        
    def process(self):
        """Mock implementation of process()"""
        # In a real implementation, we might simulate some messages here
        pass

class CANInterfaceWrapper:
    """
    A wrapper for the CANInterface that provides a higher-level API
    for sending and receiving CAN messages.
    """
    
    def __init__(self, node_id=0x01, channel='can0', baudrate=500000):
        """
        Initialize the CAN interface.
        
        Args:
            node_id (int): The CAN node ID for this device.
            channel (str): The CAN channel to use (can0, vcan0, etc.).
            baudrate (int): The CAN baudrate in bits/second.
        """
        self.node_id = node_id
        self.channel = channel
        self.baudrate = baudrate
        self.callbacks = []  # Store callbacks to prevent garbage collection
        self.using_mock = not has_can_hardware
        
        # Load protocol definitions
        self.protocol_registry = ProtocolRegistry()
        
        # Initialize lookup dictionaries for reverse mapping (numeric value to name)
        self.message_types_by_value = {v: k for k, v in self.protocol_registry.registry['message_types'].items()}
        self.component_types_by_value = {v: k for k, v in self.protocol_registry.registry['component_types'].items()}
        self.value_types_by_value = {v: k for k, v in self.protocol_registry.registry['value_types'].items()}
        
        # Create and initialize the CAN interface or mock
        if has_can_hardware:
            logger.info(f"Creating hardware CAN interface with node ID: {node_id}")
            self._can_interface = lib.can_interface_create(node_id)
            if not self._can_interface:
                logger.error("Failed to create hardware CAN interface, falling back to mock")
                self.using_mock = True
                self._can_interface = MockCANInterface(node_id)
            else:
                # Initialize the CAN interface
                channel_cstr = ffi.new("char[]", channel.encode('utf-8'))
                success = lib.can_interface_begin(self._can_interface, baudrate, channel_cstr)
                if not success:
                    logger.warning(f"Failed to initialize CAN interface on channel {channel}, but continuing anyway")
                else:
                    logger.info(f"CAN interface initialized successfully on channel {channel}")
        else:
            logger.info(f"Creating mock CAN interface with node ID: {node_id}")
            self._can_interface = MockCANInterface(node_id)

        # For state tracking (would normally come from CAN messages)
        self.current_state = {
            'timestamp': time.time(),
            'speed': 0,
            'throttle': 0,
            'brake': 0,
            'steering': 0,
            'battery': {
                'voltage': 12.0,
                'current': 0.0,
                'temperature': 25.0,
                'charge': 100.0
            },
            'lights': {
                'headlights': False,
                'brake_lights': False,
                'turn_signals': {'left': False, 'right': False}
            }
        }
        self.state_history = []

        # Start automatic message processing
        self.auto_process = False
        self.process_thread = None
        
        # Register handlers for status messages from all components
        self._register_default_handlers()
    
    def __del__(self):
        """Clean up resources when the object is destroyed"""
        self.stop_processing()
        if hasattr(self, '_can_interface') and self._can_interface and has_can_hardware:
            lib.can_interface_destroy(self._can_interface)
    
    def _get_type_name(self, registry_dict, value):
        """Helper to get a type name from the registry dictionary by its value"""
        return registry_dict.get(value, f"Unknown({value})")
    
    def _handle_message(self, msg_type, comp_type, comp_id, cmd_id, val_type, value):
        """Default message handler that logs messages and processes them through the protocol registry"""
        # Convert numeric types to names for better logging
        msg_type_name = self._get_type_name(self.message_types_by_value, msg_type)
        comp_type_name = self._get_type_name(self.component_types_by_value, comp_type)
        val_type_name = self._get_type_name(self.value_types_by_value, val_type)
        
        logger.info(f"Received message: {msg_type_name}, {comp_type_name}, Component ID: {comp_id}, "
                    f"Command ID: {cmd_id}, Value Type: {val_type_name}, Value: {value}")
        
        # Further processing can be added here
        # For example, broadcasting the message via WebSocket to dashboard clients
    
    def _register_default_handlers(self):
        """Register default handlers for various message types"""
        # Handlers will be stored as callbacks to prevent garbage collection
        
        # Handler for motor speed status
        @ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
        def handle_motor_speed(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            # Update the current state with the new motor speed
            self.current_state['speed'] = value
            # Add to history
            self._add_to_history({'speed': value})
            # Call the general message handler for logging
            self._handle_message(msg_type, comp_type, comp_id, cmd_id, val_type, value)
        
        # Handler for throttle position
        @ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
        def handle_throttle(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            # Update the current state with the new throttle position
            self.current_state['throttle'] = value
            # Add to history
            self._add_to_history({'throttle': value})
            # Call the general message handler for logging
            self._handle_message(msg_type, comp_type, comp_id, cmd_id, val_type, value)
        
        # Handler for brake position
        @ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
        def handle_brake(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            # Update the current state with the new brake position
            self.current_state['brake'] = value
            # Add to history
            self._add_to_history({'brake': value})
            # Call the general message handler for logging
            self._handle_message(msg_type, comp_type, comp_id, cmd_id, val_type, value)
        
        # Handler for steering position
        @ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
        def handle_steering(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            # Update the current state with the new steering angle
            self.current_state['steering'] = value
            # Add to history
            self._add_to_history({'steering': value})
            # Call the general message handler for logging
            self._handle_message(msg_type, comp_type, comp_id, cmd_id, val_type, value)
        
        # Handler for battery voltage
        @ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
        def handle_battery_voltage(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            # Update the current state with the new battery voltage
            self.current_state['battery']['voltage'] = value / 10.0  # Scale to voltage
            # Add to history
            self._add_to_history({'battery': {'voltage': value / 10.0}})
            # Call the general message handler for logging
            self._handle_message(msg_type, comp_type, comp_id, cmd_id, val_type, value)
        
        # Store the callbacks
        self.callbacks = [handle_motor_speed, handle_throttle, handle_brake, 
                           handle_steering, handle_battery_voltage]
        
        # Only register with actual hardware if available
        if has_can_hardware:
            # Register the handlers with the CAN interface
            # Motors.main.speed
            motors_type = self.protocol_registry.registry['component_types']['MOTORS']
            lib.can_interface_register_handler(self._can_interface, motors_type, 0, 0, handle_motor_speed)
            
            # Controls.throttle.position
            controls_type = self.protocol_registry.registry['component_types']['CONTROLS']
            lib.can_interface_register_handler(self._can_interface, controls_type, 0, 0, handle_throttle)
            
            # Controls.brake.position
            lib.can_interface_register_handler(self._can_interface, controls_type, 1, 0, handle_brake)
            
            # Controls.steering.angle
            lib.can_interface_register_handler(self._can_interface, controls_type, 2, 0, handle_steering)
            
            # Battery.voltage
            battery_type = self.protocol_registry.registry['component_types']['BATTERY']
            lib.can_interface_register_handler(self._can_interface, battery_type, 0, 0, handle_battery_voltage)
        else:
            # Register with the mock interface
            motors_type = self.protocol_registry.registry['component_types']['MOTORS']
            self._can_interface.register_handler(motors_type, 0, 0, lambda *args: handle_motor_speed(*args))
            
            controls_type = self.protocol_registry.registry['component_types']['CONTROLS']
            self._can_interface.register_handler(controls_type, 0, 0, lambda *args: handle_throttle(*args))
            self._can_interface.register_handler(controls_type, 1, 0, lambda *args: handle_brake(*args))
            self._can_interface.register_handler(controls_type, 2, 0, lambda *args: handle_steering(*args))
            
            battery_type = self.protocol_registry.registry['component_types']['BATTERY']
            self._can_interface.register_handler(battery_type, 0, 0, lambda *args: handle_battery_voltage(*args))
    
    def _add_to_history(self, update):
        """Add a state update to the history"""
        state_entry = {
            'timestamp': time.time(),
            **update
        }
        self.state_history.append(state_entry)
        # Limit history size to 1000 entries
        if len(self.state_history) > 1000:
            self.state_history.pop(0)
    
    def register_handler(self, comp_type, comp_id, cmd_id, handler):
        """
        Register a message handler for a specific message type.
        
        Args:
            comp_type (int): The component type ID.
            comp_id (int): The component ID.
            cmd_id (int): The command ID.
            handler (callable): The handler function to call when a matching message is received.
        """
        if has_can_hardware:
            # Convert string component type to numeric ID if needed
            if isinstance(comp_type, str):
                comp_type = self.protocol_registry.registry['component_types'].get(comp_type.upper(), 0)
            
            # Create a CFFI callback function
            cb = ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)", handler)
            self.callbacks.append(cb)  # Keep a reference to prevent garbage collection
            
            # Register with the CAN interface
            lib.can_interface_register_handler(self._can_interface, comp_type, comp_id, cmd_id, cb)
        else:
            # Convert string component type to numeric ID if needed
            if isinstance(comp_type, str):
                comp_type = self.protocol_registry.registry['component_types'].get(comp_type.upper(), 0)
                
            # Register with the mock interface
            self._can_interface.register_handler(comp_type, comp_id, cmd_id, handler)
    
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, value_type, value):
        """
        Send a message directly through the CAN interface.
        
        Args:
            msg_type (int): The message type ID.
            comp_type (int): The component type ID.
            comp_id (int): The component ID.
            cmd_id (int): The command ID.
            value_type (int): The value type ID.
            value (int): The value to send.
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        if has_can_hardware:
            return lib.can_interface_send_message(self._can_interface, msg_type, comp_type, 
                                                 comp_id, cmd_id, value_type, value)
        else:
            return self._can_interface.send_message(msg_type, comp_type, comp_id, cmd_id, value_type, value)
    
    def send_command(self, component_path, command_name, value_name=None, direct_value=None):
        """
        Send a command message to a component.
        
        Args:
            component_path (str): The path to the component (e.g., 'motors.main', 'lights.headlights').
            command_name (str): The name of the command (e.g., 'SPEED', 'POWER').
            value_name (str, optional): The name of the value to send (e.g., 'ON', 'OFF').
            direct_value (int, optional): The direct value to send.
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        try:
            message = self.protocol_registry.create_message(
                component_path=component_path,
                command_name=command_name,
                value_name=value_name,
                direct_value=direct_value
            )
            
            if message:
                msg_type, comp_type, comp_id, cmd_id, value_type, value = message
                return self.send_message(msg_type, comp_type, comp_id, cmd_id, value_type, value)
            else:
                logger.error(f"Failed to create message for {component_path}.{command_name}")
                return False
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return False
    
    def send_emergency_stop(self, active=True):
        """
        Send an emergency stop command.
        
        Args:
            active (bool): Whether to activate (True) or deactivate (False) the emergency stop.
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        return self.send_command('controls.emergency', 'EMERGENCY', 
                                direct_value=1 if active else 0)
    
    def process(self):
        """Process any pending messages from the CAN bus"""
        if has_can_hardware:
            lib.can_interface_process(self._can_interface)
        else:
            self._can_interface.process()
    
    def get_current_state(self):
        """Get the current state of the system"""
        # Update timestamp
        self.current_state['timestamp'] = time.time()
        return self.current_state
    
    def get_history(self, limit=100):
        """Get the history of state updates"""
        # Return the most recent entries
        return self.state_history[-limit:] if limit < len(self.state_history) else self.state_history
    
    def start_processing(self, interval=0.01):
        """
        Start a thread to automatically process incoming messages.
        
        Args:
            interval (float): Time between processing calls in seconds.
        """
        if self.process_thread and self.process_thread.is_alive():
            return
            
        self.auto_process = True
        
        def process_loop():
            while self.auto_process:
                self.process()
                time.sleep(interval)
                
        self.process_thread = threading.Thread(target=process_loop, daemon=True)
        self.process_thread.start()
        logger.info("Started automatic message processing")
        
    def stop_processing(self):
        """Stop automatic message processing"""
        self.auto_process = False
        
        if hasattr(self, 'process_thread') and self.process_thread and self.process_thread.is_alive():
            self.process_thread.join(1.0)
            logger.info("Stopped automatic message processing")

    # These methods allow the mock object to receive and handle simulated messages
    def _handle_motor_speed(self, msg_type, comp_type, comp_id, cmd_id, val_type, value):
        """Directly handle a motor speed message (for simulation)"""
        if self.callbacks and len(self.callbacks) > 0:
            self.callbacks[0](msg_type, comp_type, comp_id, cmd_id, val_type, value)
    
    def _handle_throttle(self, msg_type, comp_type, comp_id, cmd_id, val_type, value):
        """Directly handle a throttle message (for simulation)"""
        if self.callbacks and len(self.callbacks) > 1:
            self.callbacks[1](msg_type, comp_type, comp_id, cmd_id, val_type, value)
    
    def _handle_brake(self, msg_type, comp_type, comp_id, cmd_id, val_type, value):
        """Directly handle a brake message (for simulation)"""
        if self.callbacks and len(self.callbacks) > 2:
            self.callbacks[2](msg_type, comp_type, comp_id, cmd_id, val_type, value)
    
    def _handle_steering(self, msg_type, comp_type, comp_id, cmd_id, val_type, value):
        """Directly handle a steering message (for simulation)"""
        if self.callbacks and len(self.callbacks) > 3:
            self.callbacks[3](msg_type, comp_type, comp_id, cmd_id, val_type, value)
    
    def _handle_battery_voltage(self, msg_type, comp_type, comp_id, cmd_id, val_type, value):
        """Directly handle a battery voltage message (for simulation)"""
        if self.callbacks and len(self.callbacks) > 4:
            self.callbacks[4](msg_type, comp_type, comp_id, cmd_id, val_type, value)


# Create a singleton instance of the CAN interface for use throughout the application
can_interface = CANInterfaceWrapper(node_id=0x01, channel='can0') 