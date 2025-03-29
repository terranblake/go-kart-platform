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
    
    // Handler registration
    void can_interface_register_handler(
        can_interface_t handle,
        int msg_type,
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
    
    // Raw message handling
    void can_interface_register_raw_handler(
        can_interface_t handle,
        uint32_t can_id,
        void (*handler)(uint32_t, const uint8_t*, uint8_t)
    );
    
    // Raw message sending
    bool can_interface_send_raw_message(
        can_interface_t handle,
        uint32_t can_id,
        const uint8_t* data,
        uint8_t length
    );
""")

# Global flag to track if we have hardware CAN support
has_can_hardware = False
lib = None

# Try to load the library, but don't fail if it can't be loaded
try:
    # Determine the correct library path based on OS or build location
    # This might need adjustment depending on where CMake puts the library
    lib_dir = os.path.join(os.path.dirname(__file__), "..", "..", "..", "components", "common", "lib", "CrossPlatformCAN", "build")
    lib_name = "libCrossPlatformCAN.dylib" if os.uname().sysname == "Darwin" else "libCrossPlatformCAN.so"
    lib_path = os.path.join(lib_dir, lib_name)
    
    if not os.path.exists(lib_path):
        # Fallback to looking in the current directory if not found in build dir
         lib_path_fallback = os.path.join(os.path.dirname(__file__), lib_name.replace("lib", "")) # Try without 'lib' prefix too
         if os.path.exists(lib_path_fallback):
             lib_path = lib_path_fallback
         else:
             lib_path = os.path.join(os.path.dirname(__file__), "libcaninterface.so") # Original path as last resort

    logger.info(f"Attempting to load CAN interface library from: {lib_path}")
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
        # Note: Mock takes 5 args, real C API takes 6 (including handle)
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
        self.has_can_hardware = has_can_hardware # Use the global flag determined at load time
        
        # Load protocol definitions
        self.protocol_registry = ProtocolRegistry()
        
        # Initialize lookup dictionaries for reverse mapping (numeric value to name)
        self.message_types_by_value = {v: k for k, v in self.protocol_registry.registry['message_types'].items()}
        self.component_types_by_value = {v: k for k, v in self.protocol_registry.registry['component_types'].items()}
        self.value_types_by_value = {v: k for k, v in self.protocol_registry.registry['value_types'].items()}
        
        # Create and initialize the CAN interface or mock
        if self.has_can_hardware and lib: # Check if lib was loaded successfully
            logger.info(f"Creating hardware CAN interface with node ID: {node_id}")
            # Create the CAN interface first
            self._can_interface = lib.can_interface_create(node_id)
            if not self._can_interface:
                logger.error("Failed to create CAN interface handle, falling back to mock mode")
                self.has_can_hardware = False # Update flag if creation failed
                self._can_interface = MockCANInterface(node_id)
            else:
                # Initialize the CAN interface
                channel_cstr = ffi.new("char[]", channel.encode('utf-8'))
                success = lib.can_interface_begin(self._can_interface, baudrate, channel_cstr)
                if not success:
                    # Don't necessarily fall back to mock if begin fails, maybe just log warning
                    logger.warning(f"Failed to initialize CAN interface on channel {channel}, but continuing with hardware handle.")
                else:
                    logger.info(f"CAN interface initialized successfully on channel {channel}")
        else:
            logger.info(f"Creating mock CAN interface with node ID: {node_id}")
            self.has_can_hardware = False # Ensure flag is false if lib didn't load
            self._can_interface = MockCANInterface(node_id)

        # Start automatic message processing
        self.auto_process = False
        self.process_thread = None
        
        # Register handlers for status messages from all components
        self._register_default_handlers()
    
    def __del__(self):
        """Clean up resources when the object is destroyed"""
        self.stop_processing()
        if self._can_interface and self.has_can_hardware and lib: # Check lib exists
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
        
        # Check if protocol registry is loaded
        if not self.protocol_registry or not self.protocol_registry.registry:
             self.logger.error("Protocol registry not loaded. Cannot register default handlers.")
             return

        for component_type_name in self.protocol_registry.get_component_types():
            comp_type_id = self.protocol_registry.get_component_type(component_type_name)
            if comp_type_id is None:
                self.logger.warning(f"Could not find ID for component type: {component_type_name}")
                continue

            for command_name in self.protocol_registry.get_commands(component_type_name):
                cmd_id = self.protocol_registry.get_command_id(component_type_name, command_name)
                if cmd_id is None:
                     self.logger.warning(f"Could not find ID for command: {command_name} in component: {component_type_name}")
                     continue
                     
                # No need to iterate through values for handler registration
                self.logger.debug(f"Registering handlers for {component_type_name} (ID: {comp_type_id}), Command: {command_name} (ID: {cmd_id})")

                # Register handler for STATUS messages from all component IDs (0xFF)
                self.register_handler('STATUS', comp_type_id, 0xFF, cmd_id, self._handle_message)
                
                # Also register for COMMAND echo messages (Arduino might be sending these)
                # Use component ID 0xFF to catch echoes from any specific component ID
                self.register_handler('COMMAND', comp_type_id, 0xFF, cmd_id, self._handle_message)
                
                registered_count += 2
        
        self.logger.info(f"Registered {registered_count} default message handlers")
    
    def register_handler(self, message_type, comp_type, comp_id, cmd_id, handler):
        """
        Register a message handler for a specific message type.
        
        Args:
            message_type (str/int): The message type (e.g., 'COMMAND', 'STATUS') or its ID.
            comp_type (int): The component type ID.
            comp_id (int): The component ID (0xFF for wildcard).
            cmd_id (int): The command ID.
            handler (callable): The handler function to call when a matching message is received.
                                Signature: void(int msg_type, int comp_type, uint8_t comp_id, 
                                                uint8_t cmd_id, int val_type, int32_t value)
        """
        # Convert message type name to ID if necessary
        if isinstance(message_type, str):
            msg_type_id = self.protocol_registry.get_message_type(message_type.upper())
            if msg_type_id is None:
                self.logger.error(f"Invalid message type name: {message_type}")
                return
        elif isinstance(message_type, int):
            msg_type_id = message_type
        else:
             self.logger.error(f"Invalid message_type type: {type(message_type)}")
             return

        # Validate other IDs (basic check)
        if not all(isinstance(i, int) for i in [comp_type, comp_id, cmd_id]):
             self.logger.error("Component type, ID, and command ID must be integers.")
             return

        self.logger.info(f"Registering handler for msg_type={msg_type_id}, comp_type={comp_type}, comp_id={comp_id}, cmd_id={cmd_id}")

        if self.has_can_hardware and lib:
            # Create a CFFI callback function - make sure signature matches C API
            # void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)
            try:
                cb = ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)", handler)
                self.callbacks.append(cb)  # Keep a reference to prevent garbage collection

                # Register with the CAN interface (Pass the handle)
                lib.can_interface_register_handler(self._can_interface, msg_type_id, comp_type, comp_id, cmd_id, cb)
                self.logger.debug("Hardware handler registered.")
            except Exception as e:
                 self.logger.error(f"Failed to create or register CFFI callback: {e}")
        elif not self.has_can_hardware and isinstance(self._can_interface, MockCANInterface):
             # Mock interface registration needs different signature if MockCANInterface expects it
             # Adjusting mock call if necessary (assuming mock takes 5 args as defined)
             self._can_interface.register_handler(msg_type_id, comp_type, comp_id, cmd_id, handler)
             self.logger.debug("Mock handler registered.")
        else:
             self.logger.error("Cannot register handler: No valid CAN interface (hardware or mock).")

    
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
        if self.has_can_hardware and lib:
            return lib.can_interface_send_message(self._can_interface, msg_type, comp_type, 
                                                 comp_id, cmd_id, value_type, value)
        elif not self.has_can_hardware and isinstance(self._can_interface, MockCANInterface):
            # Mock send_message takes 6 args
            return self._can_interface.send_message(msg_type, comp_type, comp_id, cmd_id, value_type, value)
        else:
             self.logger.error("Cannot send message: No valid CAN interface.")
             return False

    
    def send_command(self, message_type_name, component_type_name, component_name, command_name, value_name=None, direct_value=None):
        """
        Send a command message to a component.
        
        Args:
            message_type_name (str): The name of the message type (e.g., 'COMMAND').
            component_type_name (str): The name of the component type (e.g., 'LIGHTS').
            component_name (str): The specific component name (e.g., 'HEADLIGHTS'). Used to find component ID.
            command_name (str): The name of the command (e.g., 'POWER').
            value_name (str, optional): The name of the value to send (e.g., 'ON').
            direct_value (int, optional): The direct integer value to send. Overrides value_name if provided.
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        try:
            message = self.protocol_registry.create_message(
                message_type=message_type_name,
                component_type=component_type_name,
                component_name=component_name, # Pass component_name here
                command_name=command_name,
                value_name=value_name,
                value=direct_value # Pass direct_value here
            )
            
            if message:
                # Log using names if possible
                val_repr = value_name if value_name else direct_value
                logger.info(f"Sending command: {component_type_name}.{component_name}.{command_name} = {val_repr} (Raw Value: {message[5]})")
                logger.debug(f"Raw message tuple: {message}")
                
                return self.send_message(
                    msg_type=message[0],    # Message Type ID
                    comp_type=message[1],   # Component Type ID
                    comp_id=message[2],     # Component Instance ID
                    cmd_id=message[3],      # Command ID
                    value_type=message[4],  # Value Type ID
                    value=message[5]        # Actual Value
                )
            else:
                logger.error(f"Failed to create message for {component_type_name}.{component_name}.{command_name}")
                return False
        except Exception as e:
            logger.exception(f"Error sending command {component_type_name}.{component_name}.{command_name}: {e}")
            return False

    
    def register_raw_handler(self, can_id, handler):
        """
        Register a raw message handler for a specific CAN ID.
        
        Args:
            can_id (int): The CAN ID to handle (e.g., 0x700 for animation data)
            handler (callable): The handler function to call when a raw CAN message with
                                the specified ID is received. The handler should accept
                                arguments: (can_id, data_bytes, length)
        """
        self.logger.info(f"Registering raw handler for CAN ID: 0x{can_id:X}")
        
        if self.has_can_hardware and lib:
            # Create a CFFI callback function for the raw handler
            # Signature: void (*handler)(uint32_t, const uint8_t*, uint8_t)
            try:
                @ffi.callback("void(uint32_t, const uint8_t*, uint8_t)")
                def raw_handler_wrapper(cb_can_id, data_ptr, length):
                    # Convert C byte array to Python bytes
                    # Use ffi.buffer which creates a buffer object accessing the memory
                    data = bytes(ffi.buffer(data_ptr, length))
                    # Call the user's Python handler
                    try:
                        handler(cb_can_id, data, length)
                    except Exception as e:
                         self.logger.error(f"Error in raw handler callback for CAN ID 0x{cb_can_id:X}: {e}", exc_info=True)

                self.callbacks.append(raw_handler_wrapper)  # Keep a reference
                
                # Register with the CAN interface
                lib.can_interface_register_raw_handler(self._can_interface, can_id, raw_handler_wrapper)
                self.logger.info(f"Raw hardware handler registered for CAN ID: 0x{can_id:X}")
            except Exception as e:
                 self.logger.error(f"Failed to create or register raw CFFI callback: {e}")
        elif not self.has_can_hardware and isinstance(self._can_interface, MockCANInterface):
            # Mock interface doesn't support raw handlers directly yet
            self.logger.warning(f"Mock mode doesn't support raw message handlers (CAN ID: 0x{can_id:X})")
        else:
             self.logger.error("Cannot register raw handler: No valid CAN interface.")

    
    def send_raw_message(self, can_id, data):
        """
        Send a raw message directly over the CAN bus.
        
        Args:
            can_id (int): The CAN ID to use (e.g., 0x700 for animation data)
            data (bytes): The data bytes to send (max 8 bytes)
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        if not isinstance(data, bytes):
             self.logger.error(f"Data must be bytes, got {type(data)}")
             return False
        if len(data) > 8:
            self.logger.error(f"Raw CAN message too long: {len(data)} bytes (max 8)")
            return False
            
        self.logger.info(f"Sending raw CAN message - ID: 0x{can_id:X}, Length: {len(data)}, Data: {data.hex()}")
        
        if self.has_can_hardware and lib:
            # Convert Python bytes to C data
            # ffi.new("uint8_t[]", data) creates a copy
            c_data = ffi.new("uint8_t[]", list(data)) # Pass as a list of ints
            # Send via the C API
            return lib.can_interface_send_raw_message(self._can_interface, can_id, c_data, len(data))
        elif not self.has_can_hardware and isinstance(self._can_interface, MockCANInterface):
            self.logger.warning(f"Mock mode doesn't support sending raw messages (CAN ID: 0x{can_id:X})")
            return True  # Pretend success in mock mode
        else:
             self.logger.error("Cannot send raw message: No valid CAN interface.")
             return False

    
    def process(self):
        """Process any pending messages from the CAN bus"""
        if self.has_can_hardware and lib:
            lib.can_interface_process(self._can_interface)
        elif not self.has_can_hardware and isinstance(self._can_interface, MockCANInterface):
            self._can_interface.process()
        # else: Do nothing if no interface
    
    def start_processing(self, interval=0.01):
        """
        Start a thread to automatically process incoming messages.
        
        Args:
            interval (float): Time between processing calls in seconds.
        """
        if self.process_thread and self.process_thread.is_alive():
            self.logger.debug("Processing thread already running.")
            return
            
        self.auto_process = True
        
        def process_loop():
            self.logger.info("CAN processing thread started.")
            while self.auto_process:
                try:
                    self.process()
                    time.sleep(interval)
                except Exception as e:
                     self.logger.error(f"Exception in CAN processing loop: {e}", exc_info=True)
                     # Avoid tight loop on error
                     time.sleep(max(interval, 0.1))
            self.logger.info("CAN processing thread stopped.")
                
        self.process_thread = threading.Thread(target=process_loop, daemon=True, name="CANProcessThread")
        self.process_thread.start()
        logger.info("Started automatic message processing thread.")
        
    def stop_processing(self):
        """Stop automatic message processing"""
        if not self.auto_process:
             return
             
        self.logger.info("Stopping automatic message processing...")
        self.auto_process = False
        
        if hasattr(self, 'process_thread') and self.process_thread and self.process_thread.is_alive():
            try:
                self.process_thread.join(timeout=1.0) # Wait for thread to finish
                if self.process_thread.is_alive():
                     self.logger.warning("CAN processing thread did not stop gracefully.")
                else:
                     logger.info("Stopped automatic message processing thread.")
            except Exception as e:
                 self.logger.error(f"Error stopping processing thread: {e}")
        self.process_thread = None # Clear thread reference
