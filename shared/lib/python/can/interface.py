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
        int32_t value
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
        
        # Original code restored:
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
        if self._can_interface and self.has_can_hardware:
            lib.can_interface_destroy(self._can_interface)
    
    def _get_type_name(self, registry_dict, value):
        """Helper to get a type name from the registry dictionary by its value"""
        return registry_dict.get(value, f"Unknown({value})")
    
    def _handle_message(self, source_node_id, msg_type, comp_type, comp_id, cmd_id, val_type, value, timestamp_delta):
        """Default message handler: Stores sync info or calculates recorded_at and stores telemetry."""
        
        current_collector_time = time.time()
        
        # Check if it's a time sync message
        is_time_sync_msg = (comp_type == self.protocol_registry.get_component_type('SYSTEM_MONITOR') and
                            comp_id == self.protocol_registry.get_component_id('SYSTEM_MONITOR', 'TIME_MASTER') and
                            cmd_id == self.protocol_registry.get_command_id('SYSTEM_MONITOR', 'GLOBAL_TIME_SYNC_MS'))

        if is_time_sync_msg:
            # Store sync info for the sending node (which is the Time Master)
            master_timestamp_ms = value # Value is the 24-bit master time in ms
            self.logger.info(f"TIME_SYNC Received from {source_node_id:#04x}: MasterMS={master_timestamp_ms}, CollectorTime={current_collector_time:.3f}. Updating sync info.")
            self._node_sync_info[source_node_id] = {
                'last_master_ms': master_timestamp_ms,
                'last_collector_sync_time': current_collector_time
            }
            # Do NOT store sync messages themselves as telemetry
            return 
        
        # --- It's a data message, calculate recorded_at --- 
        self.logger.info(f"Received message from node {source_node_id:#04x}: Type={msg_type} CompT={comp_type} CompID={comp_id} CmdID={cmd_id} ValT={val_type} Val={value} Delta={timestamp_delta}")

        # --- Existing Time Sync Logic (kept for future use) ---
        time_master_node_id = None
        if self._node_sync_info:
            time_master_node_id = next(iter(self._node_sync_info))
        sync_info = None
        if time_master_node_id is not None:
            sync_info = self._node_sync_info.get(time_master_node_id)
        
        temp_recorded_at = current_collector_time # Default if sync fails
        if sync_info:
            # ... (Keep reconstruction logic inside the if block) ...
            last_master_ms = sync_info['last_master_ms']
            last_collector_sync_time = sync_info['last_collector_sync_time']
            self.logger.debug(
                f"Calculating recorded_at: last_coll_sync_t={last_collector_sync_time:.4f}, "
                f"last_master_ms(masked)={last_master_ms}, delta_ms={timestamp_delta}, src={source_node_id:#04x}"
            )
            collector_epoch_ms = int(last_collector_sync_time * 1000)
            mask_24bit = 0xFFFFFF
            base = collector_epoch_ms & ~mask_24bit
            candidate0 = (base - (mask_24bit + 1)) | last_master_ms
            candidate1 = base | last_master_ms
            candidate2 = (base + (mask_24bit + 1)) | last_master_ms
            candidates = { 
                abs(collector_epoch_ms - candidate0): candidate0,
                abs(collector_epoch_ms - candidate1): candidate1,
                abs(collector_epoch_ms - candidate2): candidate2
            }
            reconstructed_full_master_ms = candidates[min(candidates.keys())]
            recorded_at_epoch_ms = reconstructed_full_master_ms + timestamp_delta
            temp_recorded_at = recorded_at_epoch_ms / 1000.0 # Store calculated value temporarily
            self.logger.debug(f"Reconstructed Master Epoch MS: {reconstructed_full_master_ms}, Calculated recorded_at={temp_recorded_at:.4f}")
            # ... (Keep sanity check logic) ...
            time_diff = abs(current_collector_time - temp_recorded_at)
            if time_diff > 1.0: # Warn if difference > 1 second
                self.logger.warning(
                    f"Large time difference ({time_diff:.3f}s) between collector ({current_collector_time:.3f}) "
                    f"and calculated recorded_at ({temp_recorded_at:.3f}). Check sync frequency/rollover."
                    f" MasterMS={last_master_ms}, Delta={timestamp_delta}, Src={source_node_id:#04x}"
                )
        else:
            self.logger.warning(f"No sync info found for Time Master (looked for node {time_master_node_id}). Using collector time for recorded_at.")
        # --- End Existing Time Sync Logic ---

        # --- Simplified Approach: Use Collector Time --- 
        recorded_at = current_collector_time
        self.logger.debug(f"Using collector time as recorded_at: {recorded_at:.4f}")
        # ------------------------------------------- 

        if not self.telemetry_store:
            logger.error("Telemetry store is not set, dropping message")
            return

        state_data = {
            'timestamp': recorded_at, # This field maps to recorded_at in DB
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
        
        # Define the callback function signature for CFFI
        # This MUST match the signature in c_api.h!
        @ffi.callback("void(uint32_t, kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, int32_t, uint8_t)")
        def callback(source_node_id_c, msg_type_c, comp_type_c, comp_id_c, cmd_id_c, val_type_c, value_c, timestamp_delta_c):
            try:
                # Call the user-provided Python handler
                handler(source_node_id_c, msg_type_c, comp_type_c, comp_id_c, cmd_id_c, val_type_c, value_c, timestamp_delta_c)
            except Exception as e:
                print(f"Error in CAN message handler callback: {e}")

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
    
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, value_type, value):
        """
        Send a message over the CAN interface.
        
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
            # Mock interface needs updating if used
            self.logger.warning("Mock CAN interface does not support timestamp delta.")
            # Send the message using the mock interface
            # return self._can_interface.send_message(msg_type, comp_type, comp_id, cmd_id, value_type, value)
            return False # Return False as mock doesn't support it
    
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
            
            self.logger.info(f"Sending command: {message_type_name} {component_type_name} {component_name} {command_name} {value_name or direct_value}")
            
            # Send the message, pass 0 for timestamp delta as collector doesn't need it for sending
            return self.send_message(msg_type, comp_type, comp_id, cmd_id, val_type, val)
        except Exception as e:
            self.logger.error(f"Error sending command: {e}")
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
    
    def send_time_sync(self):
        """Sends a GLOBAL_TIME_SYNC message with the current time."""
        if not self.has_can_hardware:
            self.logger.warning("Cannot send time sync, no CAN hardware.")
            return False
            
        # Use milliseconds since epoch, fitting into 24 bits
        current_time_ms = int(time.time() * 1000)
        time_value_24bit = current_time_ms & 0xFFFFFF # Mask to lower 24 bits
        
        # Log the values being sent
        self.logger.debug(
            f"Sending Time Sync: Full MS={current_time_ms}, Masked 24bit={time_value_24bit}"
        )
        
        try:
            # Create message params using registry lookups
            msg_type = self.protocol_registry.get_message_type('STATUS')
            comp_type = self.protocol_registry.get_component_type('SYSTEM_MONITOR')
            comp_id = self.protocol_registry.get_component_id('SYSTEM_MONITOR', 'TIME_MASTER')
            cmd_id = self.protocol_registry.get_command_id('SYSTEM_MONITOR', 'GLOBAL_TIME_SYNC_MS')
            val_type = self.protocol_registry.get_value_type('UINT24') # Assuming UINT24 is suitable
            
            self.logger.debug(f"Sending Time Sync: {time_value_24bit}ms (masked)")
            # Pass 0 for delta when sending time sync itself
            return self.send_message(msg_type, comp_type, comp_id, cmd_id, val_type, time_value_24bit)
            
        except KeyError as e:
            self.logger.error(f"Error sending time sync: Invalid name in protocol registry - {e}")
            return False
        except Exception as e:
             self.logger.error(f"Error sending time sync: {e}", exc_info=True)
             return False

    def stop_processing(self):
        """Stop the background thread that processes CAN messages."""
        self.auto_process = False
        if self.process_thread:
            self.process_thread.join(timeout=1.0)
            self.process_thread = None