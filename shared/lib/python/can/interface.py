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
import platform
import time
import threading
from cffi import FFI
from typing import Optional

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
        void (*handler)(uint32_t, kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, int32_t, uint8_t)
    );
    bool can_interface_send_message(
        can_interface_t handle,
        int msg_type,
        int comp_type,
        uint8_t component_id,
        uint8_t command_id,
        int value_type,
        int32_t value,
        int8_t delay_override
    );
    void can_interface_process(can_interface_t handle);
""")

# Global flag to track if we have hardware CAN support
has_can_hardware = False
lib = None

# Try to load the library, but don't fail if it can't be loaded
try:
    SYSTEM_PLATFORM = platform.system().lower()
    lib_name = 'invalid'
    if SYSTEM_PLATFORM == "linux":
        lib_name = "libcaninterface.so"
    elif SYSTEM_PLATFORM == "darwin":
        lib_name = "libcaninterface.dylib"
    else:
        logger.error(f"Unsupported platform: {SYSTEM_PLATFORM}")
        raise Exception(f"Unsupported platform: {SYSTEM_PLATFORM}")

    lib_path = os.path.join(os.path.dirname(__file__), lib_name)
    logger.info(f"Loading CAN interface library from: {lib_path}")
    lib = ffi.dlopen(lib_path)
    has_can_hardware = True
    logger.info("CAN interface library loaded successfully")
except Exception as e:
    logger.warning(f"Failed to load CAN interface library: {e}")
    logger.warning("Running in simulation mode without CAN hardware")
    has_can_hardware = False

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
        
        # Dictionary to store the last sync info received from each node
        # Format: { node_id: {'last_master_ms': int, 'last_collector_sync_time': float} }
        self._node_sync_info = {}
        
        # Create and initialize the CAN interface or mock
        self.logger.info(f"Creating hardware CAN interface with node ID: {node_id}")
        self._can_interface = lib.can_interface_create(node_id)
        self.logger.info(f"Attempting to initialize CAN interface handle {self._can_interface}...")
        channel_cstr = ffi.new("char[]", channel.encode('utf-8'))
        success = lib.can_interface_begin(self._can_interface, self.baudrate, channel_cstr)
        if not success:
            # Depending on platform, this might not be fatal (e.g., multicast might succeed anyway)
            self.logger.warning(f"Call to can_interface_begin for channel {channel} returned false. Interface might still work partially.")
        else:
            self.logger.info(f"CAN interface initialized successfully via can_interface_begin on channel {channel}")

        # Start automatic message processing
        self.auto_process = False
        self.process_thread = None
        
        # Register handlers for status messages from all components
        self._register_default_handlers()
    
    def __del__(self):
        """Clean up resources when the object is destroyed"""
        self.stop_processing()
        lib.can_interface_destroy(self._can_interface)
    
    def _get_type_name(self, registry_dict, value):
        """Helper to get a type name from the registry dictionary by its value"""
        return registry_dict.get(value, f"Unknown({value})")
    
    def _handle_message(self, source_node_id, msg_type, comp_type, comp_id, cmd_id, val_type, value, timestamp_delta):
        """Default message handler: Stores telemetry using collector receive time."""

        current_collector_time = time.time()
        self.logger.info(f"Received message from node {source_node_id:#04x}: Type={msg_type} CompT={comp_type} CompID={comp_id} CmdID={cmd_id} ValT={val_type} Val={value} Delta={timestamp_delta}")

        # --- Simplified Approach: Use Collector Time --- 
        recorded_at = current_collector_time
        self.logger.debug(f"Using collector time as recorded_at: {recorded_at:.4f}")
        # ------------------------------------------- 

        if not self.telemetry_store:
            logger.error("Telemetry store is not set, dropping message")
            return

        state_data = {
            'timestamp': recorded_at, # Maps to recorded_at in DB
            'message_type': msg_type,
            'component_type': comp_type,
            'component_id': comp_id,
            'command_id': cmd_id,
            'value_type': val_type,
            'value': value
        }
        
        state_data = GoKartState(**state_data)

        # --- Timing Update State --- 
        t_before_update = time.time()
        self.telemetry_store.update_state(state_data)
        t_after_update = time.time()
        update_duration = (t_after_update - t_before_update) * 1000 # milliseconds
        if update_duration > 10: # Log if update takes > 10ms
            self.logger.warning(f"_handle_message: telemetry_store.update_state took {update_duration:.2f} ms")
        # --------------------------- 
    
    def _register_default_handlers(self):
        """Register default handlers for status messages from components."""
        self.logger.info("Registering default handlers")
        registered_count = 0

        try:

            pong_comp_type = self.protocol_registry.get_component_type('SYSTEM_MONITOR') # Still need these for the check below
            pong_cmd_id = self.protocol_registry.get_command_id('SYSTEM_MONITOR', 'PONG')
            
            for component_type_name in self.protocol_registry.get_component_types():
                component_type_id = self.protocol_registry.get_component_type(component_type_name)
                if component_type_id is None: continue

                for command_name in self.protocol_registry.get_commands(component_type_name):
                    command_id = self.protocol_registry.get_command_id(component_type_name, command_name)
                    if command_id is None: continue

                    # Skip registering default _handle_message if it's PONG
                    if component_type_id == pong_comp_type and command_id == pong_cmd_id:
                         self.logger.debug(f"Skipping default handler registration for PONG.")
                         continue

                    # Register handler for STATUS messages
                    self.logger.debug(f"Registering default handler for {component_type_name}/{command_name} (STATUS)")
                    self.register_handler('STATUS', component_type_id, 255, command_id, self._handle_message)
                    registered_count += 1

        except Exception as e:
            self.logger.error(f"Error registering default handlers: {e}", exc_info=True)

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
        
        # Define the callback function signature for CFFI
        # This MUST match the signature in c_api.h!
        @ffi.callback("void(uint32_t, kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, int32_t, uint8_t)")
        def callback(source_node_id_c, msg_type_c, comp_type_c, comp_id_c, cmd_id_c, val_type_c, value_c, timestamp_delta_c):
            try:
                # Call the user-provided Python handler
                handler(source_node_id_c, msg_type_c, comp_type_c, comp_id_c, cmd_id_c, val_type_c, value_c, timestamp_delta_c)
            except Exception as e:
                # Use logger instead of print
                self.logger.error(f"Error in CAN message handler callback: {e}", exc_info=True)

        # Keep a reference to the callback object! CFFI requires this.
        self.callbacks.append(callback)

        # Register the actual callback
        lib.can_interface_register_handler(
            self._can_interface,
            msg_type,
            comp_type,
            comp_id,
            cmd_id,
            callback  # Pass the CFFI callback object
        )
        print(f"Registered handler for msg_type={msg_type}, comp_type={comp_type}, comp_id={comp_id}, cmd_id={cmd_id}")
    
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, value_type, value, delay_override: Optional[int] = None):
        """
        Send a message over the CAN interface.
        
        Args:
            msg_type (int): The message type ID.
            comp_type (int): The component type ID.
            comp_id (int): The component ID.
            cmd_id (int): The command ID.
            value_type (int): The value type ID.
            value (int): The value to send.
            delay_override (Optional[int]): Value for the delay byte (data[1]). -1 or None for default delta calc.
            
        Returns:
            bool: True if the message was sent successfully, False otherwise.
        """
        # Use -1 if delay_override is None, otherwise cast to int8
        c_delay_override = -1 if delay_override is None else int(delay_override)
        
        # Clamp c_delay_override to int8 range if necessary (though uint8 is more likely intended)
        # Assuming the C side handles uint8 interpretation from int8 input
        if not (-128 <= c_delay_override <= 127):
            self.logger.warning(f"delay_override {delay_override} out of int8 range. Using -1 (default delta).")
            c_delay_override = -1

        return lib.can_interface_send_message(
            self._can_interface,
            msg_type,
            comp_type,
            comp_id,
            cmd_id,
            value_type,
            value,
            c_delay_override # Pass the potentially modified override value
        )
    
    def send_command(self, message_type_name, component_type_name, component_name, command_name, value_name=None, direct_value=None, delay_override: Optional[int] = None):
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
            delay_override (Optional[int]): Value for the delay byte (data[1]), typically used for PING.
            
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
            
            self.logger.info(f"Sending command: {message_type_name} {component_type_name} {component_name} {command_name} {value_name or direct_value}" + (f" w/ delay_override={delay_override}" if delay_override is not None else ""))
            
            # Call send_message with the optional delay_override
            return self.send_message(msg_type, comp_type, comp_id, cmd_id, val_type, val, delay_override=delay_override)
        except Exception as e:
            self.logger.error(f"Error sending command: {e}", exc_info=True)
            return False
    
    def process(self):
        """Process CAN messages once."""
        # Process messages using the C++ library if available
        lib.can_interface_process(self._can_interface)
    
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