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
        self.connected = False
        self.last_update = 0
        
        # Motor information
        self.speed = 0  # km/h
        self.motor_temp = 0  # Celsius
        
        # Battery information
        self.battery_voltage = 0  # Volts
        self.battery_current = 0  # Amps
        self.battery_temp = 0  # Celsius
        self.battery_percent = 0  # Percent
        
        # Control information
        self.throttle = 0  # Percent (0-100)
        self.brake_pressure = 0  # Percent (0-100)
        self.steering_angle = 0  # Degrees (-45 to 45)
        
        # Safety information
        self.emergency_stop = False
        self.fault_code = 0
        
        # GPS information (if available)
        self.latitude = 0
        self.longitude = 0
        self.altitude = 0
        self.heading = 0
        self.gps_speed = 0
        
        # Sensor information
        self.accelerometer = {"x": 0, "y": 0, "z": 0}
        self.gyroscope = {"x": 0, "y": 0, "z": 0}
        
        logger.info("Go-Kart state initialized with default values")
    
    def update(self, **kwargs):
        """Update state with new values"""
        # Update timestamp
        self.timestamp = time.time()
        
        # Update provided values
        for key, value in kwargs.items():
            if hasattr(self, key):
                setattr(self, key, value)
                logger.debug(f"Updated state {key} = {value}")
            else:
                logger.warning(f"Tried to update unknown state attribute: {key}")
        
        # Update last_update timestamp
        self.last_update = self.timestamp
        
        return self
    
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