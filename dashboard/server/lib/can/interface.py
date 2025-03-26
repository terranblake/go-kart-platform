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
from typing import Callable
from cffi import FFI

from lib.can.protocol_registry import ProtocolRegistry
from lib.telemetry.state import GoKartState
from lib.telemetry.store import TelemetryStore
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
    
    // Regular message handler registration
    void can_interface_register_handler(
        can_interface_t handle,
        int msg_type,
        int comp_type,
        uint8_t component_id,
        uint8_t command_id,
        void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)
    );
    
    // Animation handler registration
    void can_interface_register_animation_handler(
        can_interface_t handle,
        uint8_t component_id,
        uint8_t command_id,
        void (*handler)(uint8_t, uint8_t, const uint8_t*, size_t, bool)
    );
    
    // Regular message sending
    bool can_interface_send_message(
        can_interface_t handle,
        int msg_type,
        int comp_type,
        uint8_t component_id,
        uint8_t command_id,
        int value_type,
        int32_t value
    );
    
    // Animation data streaming
    bool can_interface_send_animation_data(
        can_interface_t handle,
        int comp_type,
        uint8_t component_id,
        uint8_t command_id,
        const uint8_t* data,
        size_t length,
        uint8_t chunk_size
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
        self.logger = logging.getLogger(__name__)
        self.logger.info(f"Created mock CAN interface with node_id={node_id}")
        self.handlers = {}
        self.animation_handlers = {}
        
    def begin(self, baudrate, device):
        """Mock implementation of begin()"""
        self.logger.info(f"Mock CAN interface initialized with baudrate={baudrate}, device={device}")
        return True
        
    def register_handler(self, comp_type, comp_id, cmd_id, handler):
        """Mock implementation of register_handler()"""
        key = (comp_type, comp_id, cmd_id)
        self.handlers[key] = handler
        self.logger.debug(f"Registered mock handler for comp_type={comp_type}, comp_id={comp_id}, cmd_id={cmd_id}")
        return True
        
    def register_animation_handler(self, comp_id, cmd_id, handler):
        """Mock implementation of register_animation_handler()"""
        key = (comp_id, cmd_id)
        self.animation_handlers[key] = handler
        self.logger.debug(f"Registered mock animation handler for comp_id={comp_id}, cmd_id={cmd_id}")
        return True
        
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, value_type, value):
        """Mock implementation of send_message()"""
        self.logger.info(f"Mock send message: msg_type={msg_type}, comp_type={comp_type}, " 
                   f"comp_id={comp_id}, cmd_id={cmd_id}, value_type={value_type}, value={value}")
        return True
        
    def send_animation_data(self, comp_type, comp_id, cmd_id, data, length, chunk_size=6):
        """Mock implementation of send_animation_data()"""
        self.logger.info(f"Mock send animation data: comp_type={comp_type}, comp_id={comp_id}, " 
                   f"cmd_id={cmd_id}, data_length={length}, chunk_size={chunk_size}")
                   
        # In simulation mode, we could trigger any registered animation handlers
        # with chunks of the data to simulate real transmission
        if (comp_id, cmd_id) in self.animation_handlers:
            handler = self.animation_handlers[(comp_id, cmd_id)]
            
            # Simulate chunking the data
            for i in range(0, length, chunk_size):
                is_last = (i + chunk_size >= length)
                chunk_length = min(chunk_size, length - i)
                chunk = data[i:i+chunk_length]
                
                # Call the handler with the chunk
                handler(comp_id, cmd_id, chunk, chunk_length, is_last)
                
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
    
    def __init__(self, node_id=0x01, channel='can0', baudrate=500000, telemetry_store: TelemetryStore = None):
        """
        Initialize the CAN interface.
        
        Args:
            node_id (int): The CAN node ID for this device.
            channel (str): The CAN channel to use (can0, vcan0, etc.).
            baudrate (int): The CAN baudrate in bits/second.
            telemetry_store (TelemetryStore, optional): Store for telemetry data.
            protocol_registry (ProtocolRegistry, optional): Protocol registry to use.
        """
        self.logger = logging.getLogger(__name__)
        self.node_id = node_id
        self.channel = channel
        self.baudrate = baudrate
        self.telemetry_store = telemetry_store
        self.callbacks = []  # Store callbacks to prevent garbage collection
        self._can_interface = None
        self.has_can_hardware = has_can_hardware
        
        # Load protocol definitions
        self.protocol_registry = ProtocolRegistry()
        
        # Initialize lookup dictionaries for reverse mapping (numeric value to name)
        self.message_types_by_value = {v: k for k, v in self.protocol_registry.registry['message_types'].items()}
        self.component_types_by_value = {v: k for k, v in self.protocol_registry.registry['component_types'].items()}
        self.value_types_by_value = {v: k for k, v in self.protocol_registry.registry['value_types'].items()}
        
        # Create and initialize the CAN interface or mock
        if self.has_can_hardware:
            logger.info(f"Creating hardware CAN interface with node ID: {node_id}")
            # Create the CAN interface first
            self._can_interface = lib.can_interface_create(node_id)
            if not self._can_interface:
                logger.error("Failed to create CAN interface, falling back to mock mode")
                self.has_can_hardware = False
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

        # Start automatic message processing
        self.auto_process = False
        self.process_thread = None
        
        # Register handlers for status messages from all components
        self._register_default_handlers()
    
    def __del__(self):
        """Clean up resources when the object is destroyed"""
        self.stop_processing()
        if self._can_interface and self.has_can_hardware:
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
        
        logger.info(f"Received CAN message: {msg_type_name}, {comp_type_name}, Component ID: {comp_id}, "
                    f"Command ID: {cmd_id}, Value Type: {val_type_name}, Value: {value}")
        
        if not self.telemetry_store:
            logger.warning("Telemetry store is not set, skipping update")
            return

        state_data = {
            'message_type': msg_type_name,
            'component_type': comp_type_name,
            'component_id': comp_id,
            'command_id': cmd_id,
            'value_type': val_type_name,
            'value': value,
            'last_update': time.time()
        }
        
        state_data = GoKartState().from_dict(state_data)
        self.telemetry_store.update_state(state_data)
    
    def _register_default_handlers(self):
        """Register default handlers for status messages from components."""
        self.logger.info("Registering default handlers")
        registered_count = 0
        
        for component_type in self.protocol_registry.get_component_types():
            for command in self.protocol_registry.get_commands(component_type):
                for value in self.protocol_registry.get_command_values(component_type, command):
                    self.logger.info(f"Registering handlers for component type: {component_type} command: {command} value: {value}")

                    # convert the type, command, and value to their numeric values
                    comp_type = self.protocol_registry.get_component_type(component_type)
                    cmd_id = self.protocol_registry.get_command_id(component_type, command)

                    # Register handler for STATUS messages from all component IDs
                    self.register_handler('STATUS', comp_type, 255, cmd_id, self._handle_message)
                    
                    # Also register for COMMAND echo messages (Arduino might be sending these)
                    self.register_handler('COMMAND', comp_type, 255, cmd_id, self._handle_message)
                    
                    registered_count += 2
        
        self.logger.info(f"Registered {registered_count} message handlers")
    
    def register_handler(self, message_type, comp_type, comp_id, cmd_id, handler):
        """
        Register a message handler for a specific message type.
        
        Args:
            message_type (str/int): The message type (e.g., 'COMMAND', 'STATUS')
            comp_type (int): The component type ID.
            comp_id (int): The component ID.
            cmd_id (int): The command ID.
            handler (callable): The handler function to call when a matching message is received.
        """
        if isinstance(comp_type, str):
            comp_type = self.protocol_registry.registry['component_types'].get(comp_type.upper(), 0)

        if comp_type is None or cmd_id is None:
            self.logger.info(f"Failed to get component type, component id, or value id for {comp_type}, {comp_id}, {cmd_id}")
            return

        self.logger.info(f"Registering handler for message_type={message_type}, comp_type={comp_type}, comp_id={comp_id}, cmd_id={cmd_id}")

        message_type = self.protocol_registry.get_message_type(message_type)
        if self.has_can_hardware:
            # Create a CFFI callback function - make sure signature matches C API
            cb = ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)", handler)
            self.callbacks.append(cb)  # Keep a reference to prevent garbage collection

            # Register with the CAN interface
            lib.can_interface_register_handler(self._can_interface, message_type, comp_type, comp_id, cmd_id, cb)
        else:
            self._can_interface.register_handler(message_type, comp_type, comp_id, cmd_id, handler)
    
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
        if self.has_can_hardware:
            return lib.can_interface_send_message(self._can_interface, msg_type, comp_type, 
                                                 comp_id, cmd_id, value_type, value)
        else:
            return self._can_interface.send_message(msg_type, comp_type, comp_id, cmd_id, value_type, value)
    
    def send_command(self, message_type_name, component_type_name, component_name, command_name, value_name=None, direct_value=None):
        """
        Send a command message to a component.
        
        Args:
            component_path (str): The path to the component (e.g., 'motors.main', 'lights.headlights').
            command_name (str): The name of the command (e.g., 'SPEED', 'POWER').
            command_data (dict, optional): Additional command data.
            value_name (str, optional): The name of the value to send (e.g., 'ON', 'OFF').
            direct_value (int, optional): The direct value to send.
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        try:
            # Use message_type = 'COMMAND' for all commands
            message = self.protocol_registry.create_message(
                message_type=message_type_name,
                component_type=component_type_name,
                component_name=component_name,
                command_name=command_name,
                value_name=value_name,
                value=direct_value
            )
            
            if message:
                logger.info(f"Sending command: {component_type_name}.{component_name}.{command_name} = {value_name} ({message[5]})")
                logger.info(f"message: {message}")
                return self.send_message(
                    msg_type=message[0],
                    comp_type=message[1],
                    comp_id=message[2],
                    cmd_id=message[3],
                    value_type=message[4],
                    value=message[5]
                )
            else:
                logger.error(f"Failed to create message for {component_type_name}.{component_name}.{command_name}")
                return False
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return False
    
    def process(self):
        """Process any pending messages from the CAN bus"""
        if self.has_can_hardware:
            lib.can_interface_process(self._can_interface)
        else:
            self._can_interface.process()
    
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

    def register_animation_handler(self, component_id, command_id, handler):
        """
        Register an animation handler for binary data streams.
        
        Args:
            component_id (int): The component ID.
            command_id (int): The command ID.
            handler (callable): The handler function to call when animation data is received.
                Signature: handler(component_id, command_id, data, length, is_last_chunk)
        """
        self.logger.info(f"Registering animation handler for component_id={component_id}, command_id={command_id}")

        if self.has_can_hardware:
            # Create a CFFI callback function for the animation handler
            cb = ffi.callback("void(uint8_t, uint8_t, const uint8_t*, size_t, bool)", handler)
            self.callbacks.append(cb)  # Keep a reference to prevent garbage collection

            # Register with the CAN interface
            lib.can_interface_register_animation_handler(self._can_interface, component_id, command_id, cb)
        else:
            self._can_interface.register_animation_handler(component_id, command_id, handler)
    
    def send_animation_data(self, component_type_name, component_name, command_name, data, chunk_size=6):
        """
        Send animation data as a binary stream.
        
        Args:
            component_type_name (str): Name of the component type (e.g., 'LIGHTS').
            component_name (str): Name of the component (e.g., 'FRONT').
            command_name (str): Name of the command (e.g., 'ANIMATION_DATA').
            data (bytes): Binary data to send.
            chunk_size (int, optional): Maximum bytes per message. Defaults to 6.
            
        Returns:
            bool: True if the data was sent successfully, False otherwise.
        """
        try:
            # Get component type, component ID, and command ID
            component_type = self.protocol_registry.get_component_type(component_type_name)
            component_id = self.protocol_registry.get_component_id(component_type_name, component_name)
            command_id = self.protocol_registry.get_command_id(component_type_name, command_name)
            
            if component_type is None or component_id is None or command_id is None:
                self.logger.error(f"Failed to get component type, component id, or command id for {component_type_name}.{component_name}.{command_name}")
                return False
                
            # Convert data to bytes if it's not already
            if not isinstance(data, bytes):
                if isinstance(data, list):
                    data = bytes(data)
                else:
                    data = bytes(str(data), 'utf-8')
            
            # Create CFFI objects for the binary data
            data_length = len(data)
            cffi_data = ffi.new(f"uint8_t[{data_length}]")
            for i in range(data_length):
                cffi_data[i] = data[i]
                
            logger.info(f"Sending animation data: {component_type_name}.{component_name}.{command_name}, {data_length} bytes")
            
            if self.has_can_hardware:
                success = lib.can_interface_send_animation_data(
                    self._can_interface,
                    component_type,
                    component_id,
                    command_id,
                    cffi_data,
                    data_length,
                    chunk_size
                )
                
                if not success:
                    logger.error(f"Failed to send animation data for {component_type_name}.{component_name}.{command_name}")
                    
                return success
            else:
                return self._can_interface.send_animation_data(
                    component_type,
                    component_id, 
                    command_id,
                    data,
                    data_length,
                    chunk_size
                )
        except Exception as e:
            logger.error(f"Error sending animation data: {e}")
            return False