"""
Go-Kart state tracking module
"""

import time
import logging
from datetime import datetime

# Configure logging
logger = logging.getLogger(__name__)

class GoKartState:
    """Class for tracking the current state of the Go-Kart"""
    
    def __init__(self, *args, **kwargs):
        """Initialize state with default values"""
        # Basic information
        self.timestamp = time.time()

        # telemetry data populated from kwargs
        self.message_type = kwargs.get('message_type', None)
        self.component_type = kwargs.get('component_type', None)
        self.component_id = kwargs.get('component_id', None)
        self.command_id = kwargs.get('command_id', None)
        self.value_type = kwargs.get('value_type', None)
        self.value = kwargs.get('value', None)
    
    def to_dict(self):
        """Convert state to dictionary for serialization"""
        # Get all instance attributes
        state_dict = {key: value for key, value in self.__dict__.items() 
                     if not key.startswith('_')}
        
        # Add a formatted timestamp for display
        state_dict['timestamp_formatted'] = datetime.fromtimestamp(self.timestamp).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]

        return state_dict
    
    def from_dict(self, data):
        """Update state from dictionary"""
        for key, value in data.items():
            if hasattr(self, key) and not key.startswith('_'):
                setattr(self, key, value)
        
        # Update timestamp
        self.timestamp = time.time()
        self.last_update = self.timestamp
        
        return self