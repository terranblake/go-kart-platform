#!/usr/bin/env python3
"""
Python interface for CrossPlatformCAN library using CFFI

This module provides a Pythonic interface to the C API of the CrossPlatformCAN library.
It uses CFFI to call directly into the library, providing full access to the CAN
interface functionality.

Requirements:
- CFFI (pip install cffi)
- Compiled CrossPlatformCAN library with C API functions
"""

import os
import sys
import time
import threading
from typing import Callable, List, Optional
from cffi import FFI

# Define message types from Protocol Buffer
class MessageType:
    COMMAND = 0
    STATUS = 1
    
# Define component types from Protocol Buffer
class ComponentType:
    LIGHTS = 1
    CONTROLS = 2
    # Add others as needed
    
# Define value types from Protocol Buffer
class ValueType:
    BOOLEAN = 0
    INT8 = 1
    UINT8 = 2
    INT16 = 3
    UINT16 = 4
    INT24 = 5
    UINT24 = 6

# Callback function type
MessageHandlerCallback = Callable[[int, int, int, int, int, int], None]

class CANInterface:
    """Python wrapper for the CrossPlatformCAN library"""
    
    def __init__(self, node_id: int = 0x01, lib_path: Optional[str] = None):
        """
        Initialize the CAN interface
        
        Args:
            node_id: The CAN node ID for this device
            lib_path: Path to the CrossPlatformCAN library (optional)
        """
        self.node_id = node_id
        self.ffi = FFI()
        self._callbacks = []  # Keep Python callbacks alive
        
        # Define the C API
        self.ffi.cdef("""
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
        if lib_path:
            print(f"Loading library from: {lib_path}")
            self.lib = self.ffi.dlopen(lib_path)
        else:
            # Try to find the library in standard locations
            paths = [
                "./libCrossPlatformCAN.so",
                "./python/libCrossPlatformCAN.so",
                "../build/libCrossPlatformCAN.so"
            ]
            
            for p in paths:
                try:
                    if os.path.exists(p):
                        print(f"Loading library from: {p}")
                        self.lib = self.ffi.dlopen(p)
                        break
                except Exception as e:
                    print(f"Failed to load from {p}: {e}")
                    continue
            else:
                raise ImportError("Could not find CrossPlatformCAN library")
            
        # Create the interface
        print(f"Creating CAN interface with node ID: {node_id}")
        self.handle = self.lib.can_interface_create(node_id)
        if not self.handle:
            raise RuntimeError("Failed to create CAN interface")
            
        # Automatic processing thread
        self.auto_process = False
        self.process_thread = None
            
    def __del__(self):
        """Clean up resources"""
        self.stop_processing()
        if hasattr(self, 'handle') and self.handle:
            self.lib.can_interface_destroy(self.handle)
            
    def begin(self, baudrate: int = 500000, device: str = "can0") -> bool:
        """
        Initialize the CAN interface
        
        Args:
            baudrate: The CAN bus speed in bits per second
            device: The CAN device name (Linux only)
            
        Returns:
            True if initialization was successful, False otherwise
        """
        device_cstr = self.ffi.new("char[]", device.encode('utf-8'))
        return self.lib.can_interface_begin(self.handle, baudrate, device_cstr)
        
    def register_handler(self, component_type: int, component_id: int, 
                         command_id: int, handler: MessageHandlerCallback) -> None:
        """
        Register a handler for specific CAN messages
        
        Args:
            component_type: The component type to filter for
            component_id: The component ID to filter for (or 0xFF for all)
            command_id: The command ID to filter for
            handler: Function to call when matching message is received
        """
        
        # Create a callback that the C code can call
        @self.ffi.callback("void(int, int, uint8_t, uint8_t, int, int32_t)")
        def callback_wrapper(msg_type, comp_type, comp_id, cmd_id, val_type, value):
            handler(msg_type, comp_type, comp_id, cmd_id, val_type, value)
            
        # Keep a reference to prevent garbage collection
        self._callbacks.append(callback_wrapper)
        
        # Register with the C library
        self.lib.can_interface_register_handler(
            self.handle, 
            component_type, 
            component_id, 
            command_id, 
            callback_wrapper
        )
        
    def send_message(self, message_type: int, component_type: int,
                    component_id: int, command_id: int,
                    value_type: int, value: int) -> bool:
        """
        Send a message over the CAN bus
        
        Args:
            message_type: Type of message (e.g., MessageType.COMMAND)
            component_type: Type of component (e.g., ComponentType.LIGHTS)
            component_id: ID of the component
            command_id: Command ID
            value_type: Type of value (e.g., ValueType.BOOLEAN)
            value: Value to send
            
        Returns:
            True if the message was sent successfully, False otherwise
        """
        return self.lib.can_interface_send_message(
            self.handle,
            message_type,
            component_type,
            component_id,
            command_id,
            value_type,
            value
        )
        
    def process(self) -> None:
        """Process incoming CAN messages (should be called regularly)"""
        self.lib.can_interface_process(self.handle)
        
    def start_processing(self, interval: float = 0.01) -> None:
        """
        Start a thread to automatically process incoming messages
        
        Args:
            interval: Time between processing calls (seconds)
        """
        if self.process_thread and self.process_thread.is_alive():
            return
            
        self.auto_process = True
        
        def process_loop():
            while self.auto_process:
                self.process()
                time.sleep(interval)
                
        self.process_thread = threading.Thread(target=process_loop)
        self.process_thread.daemon = True
        self.process_thread.start()
        
    def stop_processing(self) -> None:
        """Stop automatic message processing"""
        self.auto_process = False
        
        if hasattr(self, 'process_thread') and self.process_thread and self.process_thread.is_alive():
            self.process_thread.join(1.0)
            
    # Convenience methods for common tasks
    
    def set_lights_location(self, location: int) -> bool:
        """
        Set the location of lights
        
        Args:
            location: Integer value representing the location
                0 = FRONT
                1 = REAR
                2 = LEFT
                3 = RIGHT
                4 = CENTER
                
        Returns:
            True if the message was sent successfully, False otherwise
        """
        LIGHTS_CMD_LOCATION = 3  # Command ID for setting light location
        return self.send_message(
            MessageType.COMMAND,
            ComponentType.LIGHTS,
            0x01,  # First lights component
            LIGHTS_CMD_LOCATION,
            ValueType.UINT8,
            location
        )
    
    def set_controls_diagnostic_mode(self, enable_test_mode: bool) -> bool:
        """
        Set the controls diagnostic mode
        
        Args:
            enable_test_mode: True to enable test mode, False to disable
            
        Returns:
            True if the message was sent successfully, False otherwise
        """
        CONTROLS_CMD_DIAGNOSTIC_MODE = 7  # Command ID for setting diagnostic mode
        value = 6 if enable_test_mode else 0
        return self.send_message(
            MessageType.COMMAND,
            ComponentType.CONTROLS,
            8,  # First controls component
            CONTROLS_CMD_DIAGNOSTIC_MODE,
            ValueType.INT8,
            value
        )