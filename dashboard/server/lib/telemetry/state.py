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
    
    def __init__(self):
        """Initialize state with default values"""
        # Basic information
        self.timestamp = time.time()
        self.last_update = 0

        # telemetry data
        self.message_type = None
        self.component_type = None
        self.component_id = None
        self.command_id = None
        self.value_type = None
        self.value = None
        
        logger.info("Go-Kart state initialized with default values")
    
    def to_dict(self):
        """Convert state to dictionary for serialization"""
        # Get all instance attributes
        state_dict = {key: value for key, value in self.__dict__.items() 
                     if not key.startswith('_')}
        
        # Add a formatted timestamp for display
        state_dict['timestamp_formatted'] = datetime.fromtimestamp(self.timestamp).strftime('%Y-%m-%d %H:%M:%S.%f')[:-3]
        
        # Add connection status based on last update time
        state_dict['connected'] = (time.time() - self.last_update) < 5.0  # Consider connected if updated in last 5 seconds
        
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