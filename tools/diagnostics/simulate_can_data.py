import time
import random
import argparse
# import can
import logging
import sys
import os
import math

# --- Add Project Root to sys.path ---
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)
    # print(f"DEBUG: Added Project Root {PROJECT_ROOT} to sys.path")

try:
    # Use relative imports since we're adding project root to path
    from shared.lib.python.can.protocol_registry import ProtocolRegistry
    from shared.lib.python.can.interface import CANInterfaceWrapper
    from shared.lib.python.telemetry.store import TelemetryStore
    # print(f"DEBUG: Successfully imported modules.")
except ImportError as e:
    print(f"Error: Could not import required shared modules (shared.lib.python.*).")
    print(f"Current sys.path: {sys.path}")
    print(f"Import error: {e}")
    print("Ensure the project structure is correct and accessible from the root.")
    sys.exit(1)

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class CANSimulator:
    """
    Simulates CAN messages for testing and development purposes.
    Uses the existing CAN interface infrastructure to generate realistic messages.
    """
    
    def __init__(self, protocol_registry, channel='vcan0', interval=0.1, node_id=0x10):
        """
        Initialize the CAN simulator
        
        Args:
            protocol_registry: ProtocolRegistry instance
            channel: CAN channel name (default: vcan0)
            interval: Simulation interval in seconds (default: 0.1)
            node_id: CAN node ID for the simulator (default: 0x10)
        """
        self.protocol_registry = protocol_registry
        self.channel = channel
        self.interval = interval
        self.running = False
        self.simulation_time = 0.0
        
        # Initialize counters for different simulation modes
        self.random_counter = 0
        self.realistic_counter = 0
        self.vehicle_counter = 0
        self.sensors_counter = 0
        
        # Set up navigation state
        self._init_navigation_state()
        
        logger.info(f"Available component types: {protocol_registry.get_component_types()}")
        
        # Create the CAN interface
        self.can_interface = CANInterfaceWrapper(
            node_id=node_id,
            channel=channel,
            baudrate=500000
        )
        
        # Get the list of component types for simulation
        self.component_types = self.protocol_registry.get_component_types()
        logger.info(f"Available component types: {self.component_types}")
        
        # Register simulation patterns
        self.simulation_patterns = {
            'random': self._simulate_random,
            'realistic': self._simulate_realistic,
            'vehicle': self._simulate_vehicle,
            'sensors': self._simulate_sensors,
            'navigation': self._simulate_navigation  # Add navigation pattern
        }
    
    def _init_navigation_state(self):
        """Initialize the state variables for navigation simulation"""
        # Starting position (San Francisco as an example)
        self.nav_latitude = 37.7749  # Decimal degrees
        self.nav_longitude = -122.4194  # Decimal degrees
        self.nav_altitude = 10.0  # Meters above sea level
        self.nav_heading = 0.0  # Degrees (0 = North, 90 = East)
        self.nav_speed = 0.0  # km/h
        self.nav_satellites = 8  # Number of GPS satellites
        self.nav_fix_quality = 2  # GPS fix quality (0=no fix, 1=GPS, 2=DGPS)
        
        # Simulation parameters
        self.nav_route_type = random.choice(['circle', 'linear', 'random'])
        self.nav_route_radius = 0.01  # ~1km radius
        self.nav_linear_direction = random.uniform(0, 360)  # Random direction
        self.nav_time_offset = time.time()  # Base time for simulation
    
    def _simulate_random(self):
        """Generate random CAN messages across all components"""
        # Choose a random component type
        comp_type = random.choice(self.component_types)
        
        try:
            # Get available commands for this component type
            commands = self.protocol_registry.get_commands(comp_type)
            if not commands:
                return False
                
            # Choose a random command
            command = random.choice(list(commands))
            
            # Get available components for this component type
            components = self.protocol_registry.registry['components'].get(comp_type, {})
            if not components:
                return False
                
            # Choose a random component
            comp_name = random.choice(list(components.keys()))
            
            # Get possible values for this command
            values = self.protocol_registry.registry['commands'][comp_type][command].get('values', {})
            value_name = None
            direct_value = None
            
            if values:
                # If we have enumerated values, use one of them
                value_name = random.choice(list(values.keys()))
            else:
                # Otherwise use a random value
                direct_value = random.randint(0, 255)
            
            # Send the message using the CAN interface
            self.can_interface.send_command(
                'COMMAND', 
                comp_type, 
                comp_name, 
                command,
                value_name, 
                direct_value
            )
            logger.info(f"Sent random message: {comp_type}/{comp_name}/{command}/{value_name or direct_value}")
            return True
            
        except Exception as e:
            logger.error(f"Error generating random message: {e}")
            return False
    
    def _simulate_realistic(self):
        """Simulate realistic CAN messages for all component types"""
        self.last_update_time = time.time()
        
        # Simulate speed oscillating between 20-40 km/h
        speed = 30 + 10 * math.sin(self.simulation_time / 10)
        
        # --- LIGHTS simulation (staggered) ---
        if self.realistic_counter % 10 == 0:  # Update lights less frequently
            # Headlights status (front lights)
            self.can_interface.send_command(
                'STATUS', 'LIGHTS', 'FRONT', 'MODE',
                'ON' if self.simulation_time % 60 < 50 else 'DIM'
            )
            time.sleep(0.0001) # Tiny sleep
            self.can_interface.send_command(
                'STATUS', 'LIGHTS', 'REAR', 'MODE', 'ON'
            )
            time.sleep(0.0001)
            # Turn signals
            turn_signal = 'NONE'
            if 10 <= (self.simulation_time % 30) < 20:
                turn_signal = 'LEFT'
            elif 20 <= (self.simulation_time % 30) < 30:
                turn_signal = 'RIGHT'
            self.can_interface.send_command(
                'STATUS', 'LIGHTS', 'REAR', 'SIGNAL', turn_signal
            )
            logger.debug("Simulated Lights Update")

        # --- MOTORS simulation (staggered) ---
        if self.realistic_counter % 2 == 0:  # Update motors more frequently
            # Motor 1 Throttle
            self.can_interface.send_command(
                'STATUS', 'MOTORS', 'MOTOR_LEFT_REAR', 'THROTTLE', 
                direct_value=int(speed)
            )
            time.sleep(0.0001) # Tiny sleep
            # Motor 1 RPM
            rpm = int(speed * 10) 
            self.can_interface.send_command(
                'STATUS', 'MOTORS', 'MOTOR_LEFT_REAR', 'RPM', 
                direct_value=rpm
            )
            time.sleep(0.0001)
            # Motor 1 Temp
            motor_temp = 40 + 0.1 * speed + 0.01 * self.simulation_time
            self.can_interface.send_command(
                'STATUS', 'MOTORS', 'MOTOR_LEFT_REAR', 'TEMPERATURE', 
                direct_value=int(motor_temp)
            )
            time.sleep(0.0001)
            # Motor 2 Throttle
            self.can_interface.send_command(
                'STATUS', 'MOTORS', 'MOTOR_RIGHT_REAR', 'THROTTLE', 
                direct_value=int(speed * 0.98)
            )
            time.sleep(0.0001)
            # Common Direction
            self.can_interface.send_command(
                'STATUS', 'MOTORS', 'MOTOR_LEFT_REAR', 'DIRECTION', 'FORWARD'
            )
            logger.debug("Simulated Motor Update")
            
        # --- BATTERIES simulation (staggered) ---
        if self.realistic_counter % 5 == 0: # Medium frequency
            battery_voltage = 12.6 - (self.simulation_time / 3600)
            self.can_interface.send_command(
                'STATUS', 'BATTERIES', 'MOTOR_LEFT_REAR', 'VOLTAGE',
                direct_value=int(battery_voltage * 10)
            )
            time.sleep(0.0001)
            battery_current = 5 + (speed / 10)
            self.can_interface.send_command(
                'STATUS', 'BATTERIES', 'MOTOR_LEFT_REAR', 'CURRENT',
                direct_value=int(battery_current * 10)
            )
            time.sleep(0.0001)
            battery_temp = 25 + (battery_current / 10)
            self.can_interface.send_command(
                'STATUS', 'BATTERIES', 'MOTOR_LEFT_REAR', 'TEMPERATURE',
                direct_value=int(battery_temp)
            )
            time.sleep(0.0001)
            self.can_interface.send_command(
                'STATUS', 'BATTERIES', 'ACCESSORY', 'VOLTAGE',
                direct_value=int(11.9 * 10)
            )
            logger.debug("Simulated Battery Update")
        
        # --- CONTROLS simulation (staggered) ---
        if self.realistic_counter % 3 == 0: # Frequent, but slightly less than motors
            # Throttle pedal position
            throttle_position = int(speed / 100 * 1024)
            self.can_interface.send_command(
                'STATUS', 'CONTROLS', 'THROTTLE', 'POSITION',
                direct_value=throttle_position
            )
            time.sleep(0.0001)
            # Brake pedal position
            brake_position = 0
            if (self.simulation_time % 30) > 25:
                brake_position = int(((self.simulation_time % 30) - 25) * 200)
            self.can_interface.send_command(
                'STATUS', 'CONTROLS', 'BRAKE', 'POSITION',
                direct_value=brake_position
            )
            time.sleep(0.0001)
            # Steering position
            steering_position = int(512 + 200 * math.sin(self.simulation_time / 5))
            self.can_interface.send_command(
                'STATUS', 'CONTROLS', 'STEERING', 'POSITION',
                direct_value=steering_position
            )
            time.sleep(0.0001)
            # Turn signal switch position
            if 10 <= (self.simulation_time % 30) < 20:
                turn_signal_state = 'LEFT'
            elif 20 <= (self.simulation_time % 30) < 30:
                turn_signal_state = 'RIGHT'
            else:
                turn_signal_state = 'NONE'
            self.can_interface.send_command(
                'STATUS', 'CONTROLS', 'TURN_SIGNAL_SWITCH', 'STATE',
                turn_signal_state
            )
            time.sleep(0.0001)
            # System mode
            self.can_interface.send_command(
                'STATUS', 'CONTROLS', 'SYSTEM', 'MODE',
                'MANUAL'
            )
            logger.debug("Simulated Controls Update")
        
        # --- NAVIGATION simulation (staggered) ---
        if self.realistic_counter % 4 == 0: # Medium frequency
            # Update position based on time
            elapsed_time = time.time() - self.nav_time_offset
            angle = (elapsed_time * 15) % 360
            lat_base = 37.7749
            long_base = -122.4194
            self.nav_latitude = lat_base + 0.001 * math.sin(math.radians(angle))
            self.nav_longitude = long_base + 0.002 * math.sin(math.radians(angle * 2))
            self.nav_heading = (angle + 90) % 360
            self.nav_speed = 25 + 5 * math.sin(elapsed_time / 5)
            
            # Send GPS data
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'GPS_PRIMARY', 'LATITUDE', direct_value=int(self.nav_latitude * 1000000))
            time.sleep(0.0001)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'GPS_PRIMARY', 'LONGITUDE', direct_value=int(self.nav_longitude * 1000000))
            time.sleep(0.0001)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'GPS_PRIMARY', 'SPEED', direct_value=int(self.nav_speed * 10))
            time.sleep(0.0001)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'GPS_PRIMARY', 'COURSE', direct_value=int(self.nav_heading * 10))
            time.sleep(0.0001)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'GPS_PRIMARY', 'GPS_FIX_STATUS', direct_value=1)
            time.sleep(0.0001)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'GPS_PRIMARY', 'SATELLITES', direct_value=random.randint(8, 12))
            
            # Send IMU data
            time.sleep(0.0001)
            accel_x = math.sin(math.radians(self.nav_heading)) * 0.1
            accel_y = math.cos(math.radians(self.nav_heading)) * 0.1
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'IMU_PRIMARY', 'ACCEL_X', direct_value=int(accel_x * 100))
            time.sleep(0.0001)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'IMU_PRIMARY', 'ACCEL_Y', direct_value=int(accel_y * 100))
            # Add Z accel/gyro if desired
            
            # Send Environment data
            time.sleep(0.0001)
            ambient_temp = 22 + 2 * math.sin(self.simulation_time / 300)
            humidity = 60 + 10 * math.sin(self.simulation_time / 600)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'ENVIRONMENT_PRIMARY', 'TEMPERATURE_AMBIENT', direct_value=int(ambient_temp * 10))
            time.sleep(0.0001)
            self.can_interface.send_command('STATUS', 'NAVIGATION', 'ENVIRONMENT_PRIMARY', 'HUMIDITY_RELATIVE', direct_value=int(humidity))
            logger.debug("Simulated Navigation Update")

        # Update counters for next cycle
        self.simulation_time += self.interval
        self.realistic_counter += 1
    
    def _simulate_vehicle(self):
        """Simulate a moving vehicle with speed and steering variations"""
        # Simulate speed changes over time
        current_time = time.time()
        speed = int(50 + 30 * math.sin(current_time / 10.0))  # Oscillate between 20-80
        
        # Steering varies less frequently
        steering = int(128 + 50 * math.sin(current_time / 20.0))  # Center is 128, range 78-178
        
        try:
            # Send speed message
            self.can_interface.send_command(
                'STATUS',
                'MOTORS',
                'DRIVE',
                'SPEED',
                None, 
                speed
            )
            
            # Send steering message with less frequency (1/3 of the time)
            if random.random() < 0.33:
                self.can_interface.send_command(
                    'STATUS',
                    'CONTROLS',
                    'STEERING',
                    'POSITION',
                    None, 
                    steering
                )
                
            logger.info(f"Simulated vehicle - Speed: {speed}, Steering: {steering}")
            return True
            
        except Exception as e:
            logger.error(f"Error simulating vehicle: {e}")
            return False
    
    def _simulate_sensors(self):
        """Simulate sensor readings with realistic patterns"""
        # Define sensor types to simulate
        sensors = [
            # (component_type, component_name, command, min_value, max_value, change_rate)
            ('SENSORS', 'BATTERY', 'LEVEL', 70, 100, 0.05),  # Battery slowly decreases
            ('SENSORS', 'TEMPERATURE', 'LEVEL', 25, 45, 0.1),  # Temperature varies
            ('SENSORS', 'SPEED', 'LEVEL', 0, 80, 1.0),  # Speed changes quickly
            ('SENSORS', 'ACCELERATION', 'LEVEL', -10, 10, 2.0)  # Acceleration varies rapidly
        ]
        
        # Choose 1-2 random sensors to update
        selected_sensors = random.sample(sensors, random.randint(1, 2))
        
        success = True
        for sensor in selected_sensors:
            comp_type, comp_name, command, min_val, max_val, change_rate = sensor
            
            # Generate value with small random changes but within bounds
            last_value = getattr(self, f"last_{comp_type}_{comp_name}", None)
            
            if last_value is None:
                # First time, generate a random value in the middle of the range
                new_value = (min_val + max_val) // 2
            else:
                # Create a change with some randomness but limited by change_rate
                change = random.uniform(-change_rate, change_rate) * (max_val - min_val) / 10
                new_value = last_value + change
                # Ensure within bounds
                new_value = max(min_val, min(max_val, new_value))
            
            # Store for next time
            setattr(self, f"last_{comp_type}_{comp_name}", new_value)
            
            # Convert to integer for CAN message
            value = int(new_value)
            
            try:
                # Send the message
                self.can_interface.send_command(
                    'STATUS',
                    comp_type,
                    comp_name,
                    command,
                    None, 
                    value
                )
                logger.info(f"Simulated sensor: {comp_type}/{comp_name}/{command} = {value}")
            except Exception as e:
                logger.error(f"Error simulating sensor {comp_type}/{comp_name}: {e}")
                success = False
                
        return success
    
    def _simulate_navigation(self):
        """Simulate navigation sensors including GPS, compass, etc."""
        current_time = time.time()
        elapsed_time = current_time - self.nav_time_offset
        
        # Update navigation state based on route type
        if self.nav_route_type == 'circle':
            # Move in a circle
            angle = (elapsed_time * 10) % 360  # Complete a circle every 36 seconds
            self.nav_latitude += self.nav_route_radius * math.sin(math.radians(angle)) * 0.01
            self.nav_longitude += self.nav_route_radius * math.cos(math.radians(angle)) * 0.01
            self.nav_heading = (angle + 90) % 360  # Heading is tangential to circle
            self.nav_speed = 20 + 5 * math.sin(elapsed_time / 5)  # Vary between 15-25 km/h
            
        elif self.nav_route_type == 'linear':
            # Move in a straight line with occasional direction changes
            if random.random() < 0.02:  # 2% chance to change direction
                self.nav_linear_direction = (self.nav_linear_direction + random.uniform(-30, 30)) % 360
            
            distance = 0.00001 * self.nav_speed  # Convert speed to approximate coordinate change
            self.nav_latitude += distance * math.cos(math.radians(self.nav_linear_direction))
            self.nav_longitude += distance * math.sin(math.radians(self.nav_linear_direction))
            self.nav_heading = self.nav_linear_direction
            self.nav_speed = 15 + 10 * math.sin(elapsed_time / 10)  # Vary between 5-25 km/h
            
        else:  # random movement
            # Random small movements
            self.nav_latitude += random.uniform(-0.0001, 0.0001)
            self.nav_longitude += random.uniform(-0.0001, 0.0001)
            self.nav_heading = (self.nav_heading + random.uniform(-5, 5)) % 360
            self.nav_speed = max(0, self.nav_speed + random.uniform(-2, 2))  # Vary speed with inertia
        
        # Occasionally vary altitude
        self.nav_altitude += random.uniform(-0.2, 0.2)
        
        # Occasionally vary satellite count
        if random.random() < 0.05:  # 5% chance to change
            self.nav_satellites = max(4, min(12, self.nav_satellites + random.choice([-1, 0, 0, 1])))
        
        # Occasionally vary fix quality
        if random.random() < 0.02:  # 2% chance to change
            self.nav_fix_quality = random.choices([0, 1, 2], weights=[1, 3, 6])[0]  # Favor better fix
        
        # Convert floating-point values to integers for CAN messages
        # Multiply by 10⁷ for lat/lon to preserve 7 decimal places
        lat_value = int(self.nav_latitude * 10000000)
        lon_value = int(self.nav_longitude * 10000000)
        alt_value = int(self.nav_altitude * 100)  # cm precision
        heading_value = int(self.nav_heading * 10)  # 0.1° precision
        speed_value = int(self.nav_speed * 10)  # 0.1 km/h precision
        
        # Send navigation messages
        success = True
        
        try:
            # GPS latitude
            self.can_interface.send_command(
                'STATUS',
                'NAVIGATION',  # Changed from 'SENSORS' to 'NAVIGATION'
                'GPS_PRIMARY', # Changed from 'GPS' to 'GPS_PRIMARY'
                'LATITUDE',
                None,
                lat_value
            )
            
            # GPS longitude
            self.can_interface.send_command(
                'STATUS',
                'NAVIGATION',
                'GPS_PRIMARY',
                'LONGITUDE',
                None, 
                lon_value
            )
            
            # Only send some messages occasionally to reduce bus load
            if random.random() < 0.3:
                # GPS altitude
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'GPS_PRIMARY',
                    'ALTITUDE',
                    None, 
                    alt_value
                )
            
            if random.random() < 0.3:
                # Heading/compass - use COURSE from protocol
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'GPS_PRIMARY',
                    'COURSE',  # Changed from 'HEADING' to 'COURSE'
                    None, 
                    heading_value
                )
            
            if random.random() < 0.3:
                # GPS speed
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'GPS_PRIMARY',
                    'SPEED',
                    None, 
                    speed_value
                )
            
            if random.random() < 0.1:
                # GPS satellites
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'GPS_PRIMARY',
                    'SATELLITES',
                    None, 
                    self.nav_satellites
                )
            
            if random.random() < 0.1:
                # GPS fix quality
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'GPS_PRIMARY',
                    'GPS_FIX_STATUS',  # Changed from 'FIX_QUALITY' to 'GPS_FIX_STATUS'
                    None, 
                    self.nav_fix_quality
                )
                
            # Add IMU data simulation
            if random.random() < 0.2:
                # Generate random acceleration data
                accel_x = int(random.uniform(-20, 20) * 100)  # Convert to int with 0.01g precision
                accel_y = int(random.uniform(-20, 20) * 100)
                accel_z = int(random.uniform(-20, 20) * 100)
                
                # Send IMU data
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'IMU_PRIMARY',
                    'ACCEL_X',
                    None,
                    accel_x
                )
                
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'IMU_PRIMARY',
                    'ACCEL_Y',
                    None,
                    accel_y
                )
                
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'IMU_PRIMARY',
                    'ACCEL_Z',
                    None,
                    accel_z
                )
            
            if random.random() < 0.2:
                # Generate temperature data (20-35°C with 0.1°C precision)
                temp = int(random.uniform(20, 35) * 10)
                
                # Send environment data
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'ENVIRONMENT_PRIMARY',
                    'TEMPERATURE_AMBIENT',
                    None,
                    temp
                )
                
                # Generate humidity data (30-80%)
                humidity = int(random.uniform(30, 80))
                
                self.can_interface.send_command(
                    'STATUS',
                    'NAVIGATION',
                    'ENVIRONMENT_PRIMARY',
                    'HUMIDITY_RELATIVE',
                    None,
                    humidity
                )
            
            logger.info(f"Simulated navigation - Lat: {self.nav_latitude:.6f}, Lon: {self.nav_longitude:.6f}, " +
                       f"Heading: {self.nav_heading:.1f}°, Speed: {self.nav_speed:.1f} km/h")
            return True
            
        except Exception as e:
            logger.error(f"Error simulating navigation: {e}")
            return False
    
    def start(self, pattern='realistic', duration=None):
        """
        Start the CAN message simulation.
        
        Args:
            pattern: The simulation pattern to use
            duration: Optional duration in seconds, None for indefinite
        """
        if pattern not in self.simulation_patterns:
            logger.error(f"Unknown simulation pattern: {pattern}")
            return False
            
        simulation_func = self.simulation_patterns[pattern]
        self.running = True
        start_time = time.time()
        msg_count = 0
        
        logger.info(f"Starting CAN simulation with pattern: {pattern}")
        
        try:
            while self.running:
                if duration and (time.time() - start_time) > duration:
                    logger.info(f"Simulation completed after {duration} seconds")
                    break
                    
                # Generate a message using the selected pattern
                if simulation_func():
                    msg_count += 1
                
                # Wait for the next interval
                time.sleep(self.interval)
                
        except KeyboardInterrupt:
            logger.info("Simulation stopped by user")
        finally:
            self.running = False
            logger.info(f"Simulation ended after sending {msg_count} messages")
            
        return True
    
    def stop(self):
        """Stop the CAN message simulation"""
        self.running = False

def main():
    parser = argparse.ArgumentParser(description='CAN Data Simulation Tool')
    parser.add_argument('--channel', default='vcan0', help='CAN channel to use')
    parser.add_argument('--node-id', type=int, default=0x10, help='CAN node ID for this simulator')
    parser.add_argument('--interval', type=float, default=0.1, help='Interval between messages in seconds')
    parser.add_argument('--pattern', choices=['random', 'realistic', 'vehicle', 'sensors', 'navigation'], 
                        default='realistic', help='Simulation pattern to use')
    parser.add_argument('--duration', type=int, help='Duration in seconds (default: run indefinitely)')
    parser.add_argument('--debug', action='store_true', help='Enable debug logging')
    
    args = parser.parse_args()
    
    # Set log level
    if args.debug:
        logging.getLogger().setLevel(logging.DEBUG)
    
    # Load protocol registry
    protocol = ProtocolRegistry()
    
    # Create and start the simulator
    simulator = CANSimulator(
        protocol_registry=protocol,
        channel=args.channel,
        interval=args.interval,
        node_id=args.node_id
    )
    
    simulator.start(pattern=args.pattern, duration=args.duration)

if __name__ == '__main__':
    main()