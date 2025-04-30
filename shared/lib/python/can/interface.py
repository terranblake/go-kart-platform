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

# Use relative imports
from shared.lib.python.can.protocol_registry import ProtocolRegistry
from shared.lib.python.telemetry.state import GoKartState
from shared.lib.python.telemetry.store import TelemetryStore

# Configure logging
logger = logging.getLogger(__name__)

# Define the C API for the CrossPlatformCAN library
ffi = FFI()

# When using dlopen(), CFFI needs to know the types of the functions we're calling
# We must define the necessary types for the function signatures
ffi.cdef("""
    // Opaque handle to the C++ ProtobufCANInterface
    typedef void* can_interface_t;
    
    // Use the enum types directly - CFFI will understand them from the shared library
    typedef int kart_common_MessageType;
    typedef int kart_common_ComponentType;
    typedef int kart_common_ValueType;
    
    // Function declarations
    can_interface_t can_interface_create(uint32_t node_id);
    void can_interface_destroy(can_interface_t handle);
    bool can_interface_begin(can_interface_t handle, long baudrate, const char* device);
    void can_interface_register_handler(
        can_interface_t handle,
        kart_common_MessageType msg_type,
        kart_common_ComponentType comp_type,
        uint8_t component_id,
        uint8_t command_id,
        void (*handler)(kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, int32_t)
    );
    bool can_interface_send_message(
        can_interface_t handle,
        int msg_type,
        int comp_type,
        uint8_t component_id,
        uint8_t command_id,
        int value_type,
        int32_t value
    );
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
        
    def begin(self, baudrate, device):
        """Mock implementation of begin()"""
        self.logger.info(f"Mock CAN interface initialized with baudrate={baudrate}, device={device}")
        return True
        
    def register_handler(self, msg_type, comp_type, comp_id, cmd_id, handler):
        """Mock implementation of register_handler()"""
        key = (msg_type, comp_type, comp_id, cmd_id)
        self.handlers[key] = handler
        self.logger.debug(f"Registered mock handler for msg_type={msg_type}, comp_type={comp_type}, comp_id={comp_id}, cmd_id={cmd_id}")
        return True
        
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, value_type, value):
        """Mock implementation of send_message()"""
        self.logger.info(f"Mock send message: msg_type={msg_type}, comp_type={comp_type}, " 
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
        
        if not self.telemetry_store:
            logger.error("Telemetry store is not set, dropping message")
            return

        state_data = {
            'message_type': msg_type,
            'component_type': comp_type,
            'component_id': comp_id,
            'command_id': cmd_id,
            'value_type': val_type,
            'value': value
        }
        
        state_data = GoKartState(**state_data)
        self.telemetry_store.update_state(state_data)
    
    def _register_default_handlers(self):
        """Register default handlers for status messages from components."""
        self.logger.info("Registering default handlers")
        registered_count = 0
        
        try:
            for component_type in self.protocol_registry.get_component_types():
                for command in self.protocol_registry.get_commands(component_type):
                    self.logger.debug(f"Registering handlers for component type: {component_type} command: {command}")

                    # convert the type, command, and value to their numeric values
                    comp_type = self.protocol_registry.get_component_type(component_type)
                    cmd_id = self.protocol_registry.get_command_id(component_type, command)

                    # Register handler for STATUS messages from all component IDs
                    self.register_handler('STATUS', comp_type, 255, cmd_id, self._handle_message)
                    registered_count += 1
        except Exception as e:
            self.logger.error(f"Error registering default handlers: {e}")
            # Don't exit just because we couldn't register handlers
        
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
        # Convert message type if it's a string
        msg_type = message_type
        if isinstance(message_type, str):
            msg_type = self.protocol_registry.get_message_type(message_type)
        
        # Register the handler with the C++ library if available
        if self.has_can_hardware:
            # Create a callback that won't be garbage collected
            @ffi.callback("void(int, int, int, int, int, int)")
            def callback(msg_type, comp_type, comp_id, cmd_id, val_type, value):
                handler(msg_type, comp_type, comp_id, cmd_id, val_type, value)
            
            # Store the callback to prevent garbage collection
            self.callbacks.append(callback)
            
            # Register the handler with the C++ library
            lib.can_interface_register_handler(
                self._can_interface, 
                msg_type, 
                comp_type, 
                comp_id, 
                cmd_id, 
                callback
            )
        else:
            # Register the handler with the mock interface
            self._can_interface.register_handler(msg_type, comp_type, comp_id, cmd_id, handler)
    
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, value_type, value):
        """
        Send a message on the CAN bus.
        
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
        # Send the message using the C++ library if available
        if self.has_can_hardware:
            return lib.can_interface_send_message(
                self._can_interface, 
                msg_type, 
                comp_type, 
                comp_id, 
                cmd_id, 
                value_type, 
                value
            )
        else:
            # Send the message using the mock interface
            return self._can_interface.send_message(msg_type, comp_type, comp_id, cmd_id, value_type, value)
    
    def send_command(self, message_type_name, component_type_name, component_name, command_name, value_name=None, direct_value=None):
        """
        Send a command message on the CAN bus using high-level parameters.
        This method uses the protocol registry to convert the high-level parameters to the appropriate IDs.
        
        Args:
            message_type_name (str): The name of the message type (e.g., 'COMMAND', 'STATUS')
            component_type_name (str): The name of the component type (e.g., 'LIGHTS', 'MOTORS')
            component_name (str): The name of the component (e.g., 'FRONT', 'REAR')
            command_name (str): The name of the command (e.g., 'MODE', 'BRIGHTNESS')
            value_name (str, optional): The name of the value (e.g., 'ON', 'OFF')
            direct_value (int, optional): A direct integer value to send instead of a named value.
            
        Returns:
            bool: True if the command was sent successfully, False otherwise.
        """
        try:
            # Create a message tuple from the high-level parameters
            msg_type, comp_type, comp_id, cmd_id, val_type, val = self.protocol_registry.create_message(
                message_type_name,
                component_type_name,
                component_name,
                command_name,
                value_name,
                direct_value
            )
            
            self.logger.debug(f"Sending command: {message_type_name} {component_type_name} {component_name} {command_name} {value_name or direct_value}")
            
            # Send the message
            return self.send_message(msg_type, comp_type, comp_id, cmd_id, val_type, val)
        except Exception as e:
            self.logger.error(f"Error sending command: {e}")
            return False
    
    def process(self):
        """Process CAN messages once."""
        # Process messages using the C++ library if available
        if self.has_can_hardware:
            lib.can_interface_process(self._can_interface)
        else:
            # Process messages using the mock interface
            self._can_interface.process()
    
    def start_processing(self, interval=0.01):
        """
        Start a background thread to process CAN messages.
        
        Args:
            interval (float): The interval in seconds at which to process messages.
        """
        if self.auto_process:
            return
            
        self.auto_process = True
        
        def process_loop():
            """Background thread function to continuously process messages."""
            while self.auto_process:
                self.process()
                time.sleep(interval)
        
        self.process_thread = threading.Thread(target=process_loop, daemon=True)
        self.process_thread.start()
    
    def stop_processing(self):
        """Stop the background thread that processes CAN messages."""
        self.auto_process = False
        if self.process_thread:
            self.process_thread.join(timeout=1.0)
            self.process_thread = None