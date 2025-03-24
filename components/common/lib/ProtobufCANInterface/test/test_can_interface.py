# for testing the compiled c++ library

import sys
import os

# Add the build directory to the Python path
sys.path.append(os.path.join(os.path.dirname(__file__), 'build'))

# Import the library
from ProtobufCANInterface import ProtobufCANInterface

# Create an instance of the library
can_interface = ProtobufCANInterface(0x01)

# Initialize the CAN interface
can_interface.begin(500000)

# Register a handler for the lights component
def lights_handler(message_type, component_type, component_id, command_id, value_type, value):
    print(f"Received lights message: {message_type}, {component_type}, {component_id}, {command_id}, {value_type}, {value}")

can_interface.register_handler(0x01, 0x01, 0x01, lights_handler)

# Send a message to the lights component
can_interface.send_message(0x01, 0x01, 0x01, 0x01, 0x01, 0x01)

# Process incoming messages
can_interface.process()