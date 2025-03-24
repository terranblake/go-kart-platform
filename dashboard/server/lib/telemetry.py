# any helper functions for parsing telemetry data"""Telemetry module for Go-Kart Dashboard"""

from datetime import datetime

class GoKartState:
    """Class to store the state of the Go-Kart."""
    
    def __init__(self):
        """Initialize with default values."""
        self.speed = 0.0
        self.battery_voltage = 0.0
        self.brake_pressure = 0.0
        self.throttle = 0
        self.steering_angle = 0.0
        self.motor_temp = 0.0
        self.timestamp = datetime.now().timestamp()
        
    def to_dict(self):
        """Convert state to dictionary for JSON serialization."""
        return {
            'speed': self.speed,
            'battery_voltage': self.battery_voltage,
            'brake_pressure': self.brake_pressure,
            'throttle': self.throttle,
            'steering_angle': self.steering_angle,
            'motor_temp': self.motor_temp,
            'timestamp': self.timestamp
        }