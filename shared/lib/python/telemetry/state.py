"""
GoKartState class for managing CAN message state

This module provides a class for storing the state of a CAN message.
"""

import time
from typing import Dict, Any, Optional

class GoKartState:
    """Class representing the state of a CAN message."""
    
    def __init__(self, message_type: Optional[int] = None, 
                component_type: Optional[int] = None,
                component_id: Optional[int] = None, 
                command_id: Optional[int] = None,
                value_type: Optional[int] = None, 
                value: Optional[int] = None,
                timestamp: Optional[float] = None):
        """
        Initialize the GoKartState with the CAN message parameters.
        
        Args:
            message_type: The type of message (e.g., COMMAND, STATUS)
            component_type: The type of component (e.g., LIGHTS, MOTORS)
            component_id: The ID of the component
            command_id: The ID of the command
            value_type: The type of value (e.g., INT8, BOOL)
            value: The value of the message
            timestamp: The timestamp of the message (defaults to current time)
        """
        self.message_type = message_type
        self.component_type = component_type
        self.component_id = component_id
        self.command_id = command_id
        self.value_type = value_type
        self.value = value
        self.timestamp = timestamp or time.time()
    
    def to_dict(self) -> Dict[str, Any]:
        """Convert the state to a dictionary."""
        return {
            'message_type': self.message_type,
            'component_type': self.component_type,
            'component_id': self.component_id,
            'command_id': self.command_id,
            'value_type': self.value_type,
            'value': self.value,
            'timestamp': self.timestamp
        }
        
    def __str__(self) -> str:
        """Return a string representation of the state."""
        return (f"GoKartState(message_type={self.message_type}, "
                f"component_type={self.component_type}, "
                f"component_id={self.component_id}, "
                f"command_id={self.command_id}, "
                f"value_type={self.value_type}, "
                f"value={self.value}, "
                f"timestamp={self.timestamp})") 