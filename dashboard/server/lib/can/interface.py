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

# Load the library
try:
    lib_path = os.path.join(os.path.dirname(__file__), "libcaninterface.so")
    logger.info(f"Loading CAN interface library from: {lib_path}")
    lib = ffi.dlopen(lib_path)
    logger.info("CAN interface library loaded successfully")
except Exception as e:
    logger.error(f"Failed to load CAN interface library: {e}")
    raise

# Message handler type
MessageHandlerCallback = Callable[[int, int, int, int, int, int], None]

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
        
        # Load protocol definitions
        self.protocol_registry = ProtocolRegistry()
        
        # Initialize lookup dictionaries for reverse mapping (numeric value to name)
        self.message_types_by_value = {v: k for k, v in self.protocol_registry.registry['message_types'].items()}
        self.component_types_by_value = {v: k for k, v in self.protocol_registry.registry['component_types'].items()}
        self.value_types_by_value = {v: k for k, v in self.protocol_registry.registry['value_types'].items()}
        
        # Create and initialize the CAN interface
        logger.info(f"Creating CAN interface with node ID: {node_id}")
        self._can_interface = lib.can_interface_create(node_id)
        if not self._can_interface:
            raise RuntimeError("Failed to create CAN interface")
        
        # Initialize the CAN interface
        channel_cstr = ffi.new("char[]", channel.encode('utf-8'))
        success = lib.can_interface_begin(self._can_interface, baudrate, channel_cstr)
        if not success:
            logger.warning(f"Failed to initialize CAN interface on channel {channel}, but continuing anyway")
        else:
            logger.info(f"CAN interface initialized successfully on channel {channel}")

        # Start automatic message processing
        self.auto_process = False
        self.process_thread = None
        
        # Register handlers for status messages from all components
        self._register_default_handlers()
    
    def __del__(self):
        """Clean up resources when the object is destroyed"""
        self.stop_processing()
        if hasattr(self, '_can_interface') and self._can_interface:
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
        """Register default handlers for messages from components"""
        # Using the protocol registry, register handlers for all components and commands
        logger.info("Registering default message handlers")
        
        # Register a generic handler for all message types
        for comp_type_name, comp_type_value in self.protocol_registry.registry['component_types'].items():
            # Skip reserved or special values
            if comp_type_name.startswith('_'):
                continue
                
            # For each component, register handlers for all commands
            # We use 0xFF as the component_id to match all components of this type
            # We use 0xFF as the command_id to match all commands for this component
            
            @ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
            def handler_wrapper(msg_type, comp_type_val, comp_id, cmd_id, val_type, value):
                self._handle_message(msg_type, comp_type_val, comp_id, cmd_id, val_type, value)
            
            # Keep the callback alive
            self.callbacks.append(handler_wrapper)
            
            # Register the handler with the C library
            lib.can_interface_register_handler(
                self._can_interface, 
                comp_type_value,  # Component type (e.g., LIGHTS, MOTORS)
                0xFF,             # Component ID (all components of this type)
                0xFF,             # Command ID (all commands)
                handler_wrapper
            )
            
            logger.debug(f"Registered handler for component type {comp_type_name}")
    
    def register_handler(self, component_type, component_id, command_id, handler):
        """
        Register a handler for a specific message type.
        
        Args:
            component_type (int or str): The component type (integer value or string name).
            component_id (int or str): The component ID (integer value or component name).
            command_id (int or str): The command ID (integer value or command name).
            handler (callable): A function that takes (message_type, component_type, component_id, command_id, value_type, value) and returns nothing.
            
        Returns:
            bool: True if the handler was registered successfully, False otherwise.
        """
        # Convert string names to numeric values if needed
        if isinstance(component_type, str):
            component_type = self.protocol_registry.get_component_type(component_type)
            if component_type is None:
                logger.error(f"Unknown component type: {component_type}")
                return False
                
        if isinstance(component_id, str) and isinstance(component_type, int):
            # Convert component name to ID if given as string
            comp_type_name = self._get_type_name(self.component_types_by_value, component_type)
            component_id = self.protocol_registry.get_component_id(comp_type_name, component_id)
            if component_id is None:
                logger.error(f"Unknown component ID: {component_id} for type {comp_type_name}")
                return False
                
        if isinstance(command_id, str) and isinstance(component_type, int):
            # Convert command name to ID if given as string
            comp_type_name = self._get_type_name(self.component_types_by_value, component_type)
            command_id = self.protocol_registry.get_command_id(comp_type_name, command_id)
            if command_id is None:
                logger.error(f"Unknown command ID: {command_id} for type {comp_type_name}")
                return False
        
        @ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
        def handler_wrapper(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            handler(msg_type, comp_type, comp_id, cmd_id, val_type, value)
        
        # Keep the callback alive
        self.callbacks.append(handler_wrapper)
        
        # Register with the C library
        lib.can_interface_register_handler(
            self._can_interface, 
            component_type, 
            component_id, 
            command_id, 
            handler_wrapper
        )
        
        comp_type_name = self._get_type_name(self.component_types_by_value, component_type)
        logger.debug(f"Registered custom handler for component type {comp_type_name}, ID {component_id}, command ID {command_id}")
        return True
    
    def send_message(self, message_type, component_type, component_id, command_id, value_type, value):
        """
        Send a CAN message.
        
        Args:
            message_type (int): The message type (COMMAND, STATUS, etc.).
            component_type (int): The component type (LIGHTS, MOTORS, etc.).
            component_id (int): The component ID.
            command_id (int): The command ID.
            value_type (int): The value type (BOOLEAN, UINT8, etc.).
            value: The value to send.
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        # Log the message being sent
        msg_type_name = self._get_type_name(self.message_types_by_value, message_type)
        comp_type_name = self._get_type_name(self.component_types_by_value, component_type)
        val_type_name = self._get_type_name(self.value_types_by_value, value_type)
        
        logger.info(f"Sending message: {msg_type_name}, {comp_type_name}, Component ID: {component_id}, "
                    f"Command ID: {command_id}, Value Type: {val_type_name}, Value: {value}")
        
        # Send the message using the C library
        return lib.can_interface_send_message(
            self._can_interface,
            message_type,
            component_type,
            component_id,
            command_id,
            value_type,
            value
        )
    
    def send_protocol_message(self, message_spec):
        """
        Send a message using the protocol registry to resolve component and command names.
        
        Args:
            message_spec (dict): A dictionary with keys:
                - message_type: The message type name (e.g., 'COMMAND')
                - component_type: The component type name (e.g., 'LIGHTS')
                - component_name: The component name (e.g., 'FRONT')
                - command_name: The command name (e.g., 'TOGGLE')
                - value_name: (optional) The value name (e.g., 'ON')
                - value: (optional) The direct value to send
                
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        msg_tuple = self.protocol_registry.create_message(
            message_spec.get('message_type', 'COMMAND'),
            message_spec.get('component_type'),
            message_spec.get('component_name'),
            message_spec.get('command_name'),
            message_spec.get('value_name'),
            message_spec.get('value')
        )
        
        # Check if any parts of the message are None
        if None in msg_tuple:
            logger.error(f"Invalid message specification: {message_spec}")
            return False
            
        return self.send_message(*msg_tuple)
    
    def process(self):
        """
        Process incoming CAN messages.
        
        Returns:
            None
        """
        lib.can_interface_process(self._can_interface)
    
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


# Create a singleton instance of the CAN interface for use throughout the application
can_interface = CANInterfaceWrapper(node_id=0x01, channel='can0') 