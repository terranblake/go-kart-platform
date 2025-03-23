import can
import time
import threading
import struct
import logging
import collections
import os
from datetime import datetime
from flask import Flask, render_template, jsonify, request
from flask_cors import CORS
from flask_socketio import SocketIO
import os
import json

# Ensure network is up
try:
    os.system('sudo ip link set can0 up type can bitrate 500000')
except:
    print('CAN network check failed')
    exit(1)

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class GoKartState:
    def __init__(self):
        self.speed = 0.0
        self.throttle = 0
        self.battery_voltage = 0.0
        self.motor_temp = 0.0
        self.brake_pressure = 0.0
        self.steering_angle = 0.0
        self.timestamp = datetime.now().timestamp()
    
    def to_dict(self):
        return {
            'speed': round(self.speed, 1),
            'throttle': self.throttle,
            'battery_voltage': round(self.battery_voltage, 1),
            'motor_temp': round(self.motor_temp, 1),
            'brake_pressure': round(self.brake_pressure, 1),
            'steering_angle': round(self.steering_angle, 1),
            'timestamp': self.timestamp
        }

class CANCommandGenerator:
    """Class to generate CAN messages for the go-kart."""

    # Command IDs
    COMMAND_ID = 0x100
    DEVICE_NANO = 0x02
    CMD_EMERGENCY_STOP = 0x0A
    CMD_SPEED_CONTROL = 0x0B
    CMD_STEERING_CONTROL = 0x0C
    CMD_BRAKE_CONTROL = 0x0D
    CMD_LIGHTS_CONTROL = 0x0E

    def __init__(self, channel='vcan0', bitrate=500000):
        """Initialize CAN command generator."""
        try:
            self.bus = can.interface.Bus(channel=channel, bustype='socketcan', bitrate=bitrate)
            logger.info(f"CAN bus initialized on channel {channel}")
        except Exception as e:
            logger.error(f"Error initializing CAN bus: {e}")
            self.bus = None
            
        # Initialize state tracking
        self.state = GoKartState()
        self.history = collections.deque(maxlen=600)  # Store 1 minute of data at 10Hz
        self.light_states = {}
        
        # Load protocol definitions
        self.protocol = self.load_protocol_definitions('protocol.json')

    def load_protocol_definitions(self, filename):
        """Load protocol definitions from JSON file"""
        try:
            # Protocol definitions JSON path
            json_path = os.path.join(os.path.dirname(__file__), filename)
            logger.info(f"Loading protocol from: {json_path}")
            
            # Load JSON
            with open(json_path, 'r') as f:
                protocol = json.load(f)
            
            # Debug print some key parts of the protocol
            if 'kart' in protocol:
                light_modes = protocol.get('kart', {}).get('commands', {}).get('lights', {}).get('mode', {}).get('values', {})
                logger.info(f"Found light modes in protocol: {list(light_modes.keys())}")
            else:
                logger.warning("Protocol doesn't contain 'kart' key")
            
            logger.info("Kart protocol definitions loaded successfully")
            return protocol
        except FileNotFoundError:
            logger.error(f"Protocol file not found: {filename}")
            return {"kart": {}}
        except json.JSONDecodeError:
            logger.error(f"Invalid JSON in protocol file: {filename}")
            return {"kart": {}}

    def get_command_value(self, command_type, command, value_name):
        """Get numeric value for a command value by name."""
        try:
            value_hex = self.protocol["kart"]["commands"][command_type][command]["values"][value_name]
            return int(value_hex, 16)
        except (KeyError, ValueError):
            logger.error(f"Invalid command value: {command_type}.{command}.{value_name}")
            return 0

    def get_command_values(self, command_type, command):
        """Get all possible values for a command."""
        try:
            return self.protocol["kart"]["commands"][command_type][command]["values"]
        except KeyError:
            logger.error(f"Invalid command: {command_type}.{command}")
            return {}

    def get_component_type_id(self, component_type):
        """Get numeric ID for a component type."""
        try:
            type_id_hex = self.protocol["kart"]["component_types"][component_type]["id"]
            return int(type_id_hex, 16)
        except (KeyError, ValueError):
            logger.error(f"Invalid component type: {component_type}")
            return 0

    def get_component_id(self, component_type, component):
        """Get numeric ID for a component."""
        try:
            component_id_hex = self.protocol["kart"]["components"][component_type][component]["id"]
            return int(component_id_hex, 16)
        except (KeyError, ValueError):
            logger.error(f"Invalid component: {component_type}.{component}")
            return 0

    def get_command_id(self, command_type, command):
        """Get numeric ID for a command."""
        try:
            command_id_hex = self.protocol["kart"]["commands"][command_type][command]["id"]
            return int(command_id_hex, 16)
        except (KeyError, ValueError):
            logger.error(f"Invalid command: {command_type}.{command}")
            return 0

    def get_message_header(self, command_type, command):
        """Generate message header for a command."""
        component_type_id = self.get_component_type_id(command_type)
        command_id = self.get_command_id(command_type, command)
        
        if component_type_id is None or command_id is None:
            return None
            
        return (component_type_id << 4) | command_id

    def send_can_message(self, command_type, command, component, value):
        """Send a CAN message using the protocol."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
            
        header = self.get_message_header(command_type, command)
        component_id = self.get_component_id(command_type, component)
        
        if header is None or header == 0 or component_id is None or component_id == 0:
            logger.error(f"Invalid protocol definition for {command_type}.{command}.{component}")
            return False
            
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[header, component_id, value],
            is_extended_id=False
        )
        
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending {command_type}.{command} message: {e}")
            return False

    def listen_telemetry(self, test_mode=False):
        """Listen for incoming telemetry messages."""
        while True:
            try:
                if not self.bus:
                    logger.error("CAN bus not initialized")
                    time.sleep(0.1)
                    if test_mode:
                        return
                    continue
                    
                message = self.bus.recv(timeout=0.1)
                if message is None:
                    # Handle timeout case
                    logger.debug("CAN bus receive timeout")
                    if test_mode:
                        return
                    continue
                
                # Check if valid device ID
                if len(message.data) < 1 or message.data[0] != self.DEVICE_NANO:
                    logger.debug(f"Ignoring message with invalid device ID: {message.data[0] if len(message.data) > 0 else 'empty'}")
                    if test_mode:
                        return
                    continue
                
                # Parse messages based on ID
                if message.arbitration_id == 0x100:  # Speed Throttle
                    self.state.speed = struct.unpack('>h', message.data[1:3])[0] / 10.0
                    self.state.throttle = message.data[3]
                    logger.debug(f"Speed: {self.state.speed:.1f} km/h, Throttle: {self.state.throttle}%")
                
                elif message.arbitration_id == 0x101:  # Battery Motor
                    self.state.battery_voltage = struct.unpack('>h', message.data[1:3])[0] / 10.0
                    self.state.motor_temp = struct.unpack('>h', message.data[3:5])[0] / 10.0
                    logger.debug(f"Battery: {self.state.battery_voltage:.1f} V, Temp: {self.state.motor_temp:.1f} C")
                
                elif message.arbitration_id == 0x102:  # Brake Steering
                    self.state.brake_pressure = struct.unpack('>h', message.data[1:3])[0] / 10.0
                    self.state.steering_angle = struct.unpack('>h', message.data[3:5])[0] / 10.0
                    logger.debug(f"Brake: {self.state.brake_pressure:.1f} %, Steering: {self.state.steering_angle:.1f} deg")
                
                # Update timestamp and add to history
                self.state.timestamp = datetime.now().timestamp()
                self.history.append(self.state.to_dict())
                
                # If in test mode, just process one message and return
                if test_mode:
                    return
                
            except Exception as e:
                logger.error(f"Error in telemetry: {e}")
                time.sleep(0.1)
                # If in test mode, raise the exception so the test can fail properly
                if test_mode:
                    raise

    def get_current_state(self):
        """Return the current state as a dictionary."""
        state_dict = self.state.to_dict()
        
        # Add light states
        state_dict.update({
            'light_mode': self.light_states.get('lights_mode', 0),
            'light_signal': self.light_states.get('lights_signal', 0),
            'light_brake': 1 if self.light_states.get('lights_brake', 0) == 0xFF else 0,
            'light_test': 1 if self.light_states.get('lights_test', 0) == 0xFF else 0,
        })
        
        return state_dict

    def get_history(self):
        """Return the historical state data."""
        return list(self.history)

    def send_emergency_stop(self, activate):
        """Send emergency stop command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_EMERGENCY_STOP, 0xFF if activate else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending emergency stop command: {e}")
            return False

    def send_speed_command(self, speed):
        """Send speed control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        # Limit speed to 0-100%
        speed = max(0, min(100, speed))
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_SPEED_CONTROL, speed],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending speed command: {e}")
            return False

    def send_steering_command(self, angle):
        """Send steering control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        # Limit angle to -45 to +45 degrees
        angle = max(-45, min(45, angle))
        # Convert to 0-90 range for CAN message
        can_angle = angle + 45
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_STEERING_CONTROL, can_angle],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending steering command: {e}")
            return False

    def send_brake_command(self, pressure):
        """Send brake control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        # Limit pressure to 0-100%
        pressure = max(0, min(100, pressure))
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_BRAKE_CONTROL, pressure],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending brake command: {e}")
            return False

    def send_lights_command(self, lights_on):
        """Send lights control command."""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if lights_on else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            return True
        except Exception as e:
            logger.error(f"Error sending lights command: {e}")
            return False

    def send_light_mode_by_index(self, mode_index):
        """Set light mode by index (0=off, 1=low, 2=high, 3=hazard)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        # Validate mode index
        if mode_index < 0 or mode_index > 3:
            logger.error(f"Invalid light mode index: {mode_index}")
            return False
        
        # Convert mode index to value
        value = mode_index
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, value],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_mode'] = mode_index
            logger.info(f"Light Mode: {['OFF', 'LOW', 'HIGH', 'HAZARD'][mode_index]}")
            return True
        except Exception as e:
            logger.error(f"Error sending light mode command: {e}")
            return False

    def send_light_mode(self, mode_name):
        """Set light mode by name (off, low, high, hazard)"""
        mode_map = {'off': 0, 'low': 1, 'high': 2, 'hazard': 3}
        if mode_name.lower() not in mode_map:
            logger.error(f"Invalid light mode: {mode_name}")
            return False
        
        return self.send_light_mode_by_index(mode_map[mode_name.lower()])

    def send_light_signal_by_index(self, signal_index):
        """Set turn signal by index (0=off, 1=left, 2=right)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        # Validate signal index
        if signal_index < 0 or signal_index > 2:
            logger.error(f"Invalid turn signal index: {signal_index}")
            return False
        
        # Convert signal index to value
        value = signal_index
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, value],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_signal'] = signal_index
            logger.info(f"Turn Signal: {['OFF', 'LEFT', 'RIGHT'][signal_index]}")
            return True
        except Exception as e:
            logger.error(f"Error sending turn signal command: {e}")
            return False

    def send_light_signal(self, signal_name):
        """Set turn signal by name (off, left, right)"""
        signal_map = {'off': 0, 'left': 1, 'right': 2}
        if signal_name.lower() not in signal_map:
            logger.error(f"Invalid turn signal: {signal_name}")
            return False
        
        return self.send_light_signal_by_index(signal_map[signal_name.lower()])

    def send_light_brake(self, state):
        """Set brake lights (on/off)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if state else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_brake'] = 0xFF if state else 0x00
            logger.info(f"Brake Lights: {'ON' if state else 'OFF'}")
            return True
        except Exception as e:
            logger.error(f"Error sending brake lights command: {e}")
            return False

    def send_light_test(self, enable):
        """Enable/disable light test mode"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if enable else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            self.light_states['lights_test'] = 0xFF if enable else 0x00
            logger.info(f"Light Test Mode: {'ON' if enable else 'OFF'}")
            return True
        except Exception as e:
            logger.error(f"Error sending light test command: {e}")
            return False

    def send_light_location(self, is_front):
        """Set light controller location (front/rear)"""
        if not self.bus:
            logger.error("CAN bus not initialized")
            return False
        
        message = can.Message(
            arbitration_id=self.COMMAND_ID,
            data=[self.DEVICE_NANO, self.CMD_LIGHTS_CONTROL, 0xFF if is_front else 0x00],
            is_extended_id=False
        )
        try:
            self.bus.send(message)
            logger.info(f"Light Location Set: {'FRONT' if is_front else 'REAR'}")
            return True
        except Exception as e:
            logger.error(f"Error sending light location command: {e}")
            return False

# Create Flask app and SocketIO for real-time updates
app = Flask(__name__)
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

# Create CAN command generator
command_generator = CANCommandGenerator(channel='can0', bitrate=500000)

# Start telemetry thread
telemetry_thread = threading.Thread(target=command_generator.listen_telemetry, daemon=True)
telemetry_thread.start()

# Serve the React frontend
@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/lights', methods=['POST'])
def control_lights():
    data = request.json
    
    if 'mode' in data:
        if isinstance(data['mode'], int):
            command_generator.send_light_mode_by_index(data['mode'])
        else:
            command_generator.send_light_mode(data['mode'])
    
    if 'signal' in data:
        if isinstance(data['signal'], int):
            command_generator.send_light_signal_by_index(data['signal'])
        else:
            command_generator.send_light_signal(data['signal'])
    
    if 'brake' in data:
        command_generator.send_light_brake(data['brake'])
    
    if 'test' in data:
        command_generator.send_light_test(data['test'])
    
    if 'location' in data:
        if isinstance(data['location'], bool):
            command_generator.send_light_location(data['location'])
        else:
            command_generator.send_light_location(data['location'] == 'front')
    
    return jsonify({'success': True})

# API endpoints
@app.route('/api/state', methods=['GET'])
def get_state():
    return jsonify(command_generator.get_current_state())

@app.route('/api/history', methods=['GET'])
def get_history():
    return jsonify(command_generator.get_history())

@app.route('/api/camera/status', methods=['GET'])
def get_camera_status():
    """Get the current camera status."""
    return jsonify({'status': 'offline'})

@app.route('/api/settings', methods=['GET'])
def get_settings():
    """Get current settings."""
    return jsonify({
        'camera_status': {'status': 'offline'}
    })

@app.route('/api/settings', methods=['POST'])
def update_settings():
    data = request.json
    return jsonify({'success': True})

@app.route('/api/command', methods=['POST'])
def send_command():
    data = request.json
    
    if 'emergency_stop' in data:
        command_generator.send_emergency_stop(data['emergency_stop'])
    
    if 'speed' in data:
        command_generator.send_speed_command(data['speed'])
    
    if 'steering' in data:
        command_generator.send_steering_command(data['steering'])
    
    if 'brake' in data:
        command_generator.send_brake_command(data['brake'])
    
    return jsonify({'success': True})

# SocketIO for real-time updates
def send_updates():
    """Send state updates to connected clients."""
    last_update = time.time()
    
    while True:
        current_time = time.time()
        
        # Send update every 100ms
        if current_time - last_update >= 0.1:
            socketio.emit('state_update', command_generator.get_current_state())
            last_update = current_time
        
        # Don't burn CPU
        time.sleep(0.01)

@socketio.on('connect')
def handle_connect():
    logger.info('Client connected')

if __name__ == "__main__":
    # Create templates directory if it doesn't exist
    try:
        if not os.path.exists('templates'):
            os.makedirs('templates')
    except Exception as e:
        logger.error(f"Failed to create templates directory: {e}")
        
    # Write index.html
    with open('templates/index.html', 'w') as f:
        f.write(
            '''
                <!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Go-Kart Control Dashboard</title>
    <script src="https://cdn.socket.io/4.4.1/socket.io.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .dashboard {
            display: flex;
            flex-wrap: wrap;
            gap: 20px;
        }
        .panel {
            background-color: white;
            border-radius: 8px;
            padding: 15px;
            box-shadow: 0 2px 5px rgba(0,0,0,0.1);
            flex: 1;
            min-width: 300px;
        }
        .controls {
            display: flex;
            flex-direction: column;
            gap: 15px;
        }
        .control-group {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }
        .metrics {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
            gap: 15px;
        }
        .metric {
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            padding: 15px;
            border-radius: 8px;
            background-color: #eef;
        }
        .metric-value {
            font-size: 24px;
            font-weight: bold;
        }
        .metric-label {
            font-size: 14px;
            color: #666;
        }
        .chart-container {
            width: 100%;
            height: 300px;
            margin-top: 20px;
        }
        input[type="range"] {
            width: 100%;
        }
        button {
            padding: 10px;
            border: none;
            border-radius: 4px;
            background-color: #4a65ff;
            color: white;
            font-weight: bold;
            cursor: pointer;
        }
        button:hover {
            background-color: #354bd1;
        }
        button.stop {
            background-color: #ff4a4a;
        }
        button.stop:hover {
            background-color: #d13535;
        }
        .value-display {
            font-weight: bold;
            text-align: center;
        }
        /* Toggle switch styles */
        .toggle-container {
            display: flex;
            align-items: center;
            gap: 10px;
        }
        .switch {
            position: relative;
            display: inline-block;
            width: 60px;
            height: 34px;
        }
        .switch input {
            opacity: 0;
            width: 0;
            height: 0;
        }
        .slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background-color: #ccc;
            transition: .4s;
        }
        .slider:before {
            position: absolute;
            content: "";
            height: 26px;
            width: 26px;
            left: 4px;
            bottom: 4px;
            background-color: white;
            transition: .4s;
        }
        input:checked + .slider {
            background-color: #2196F3;
        }
        input:focus + .slider {
            box-shadow: 0 0 1px #2196F3;
        }
        input:checked + .slider:before {
            transform: translateX(26px);
        }
        .slider.round {
            border-radius: 34px;
        }
        .slider.round:before {
            border-radius: 50%;
        }
        /* Camera styles */
        .camera-panel {
            flex: 1;
            min-width: 320px;
            max-width: 600px;
        }
        .camera-container {
            position: relative;
            width: 100%;
            height: 240px;
            background-color: #000;
            margin-bottom: 10px;
            overflow: hidden;
        }
        #camera-feed {
            width: 100%;
            height: 100%;
            object-fit: cover;
            display: block;
        }
        .camera-overlay {
            position: absolute;
            top: 50%;
            left: 50%;
            transform: translate(-50%, -50%);
            background-color: rgba(0, 0, 0, 0.7);
            color: white;
            padding: 10px 20px;
            border-radius: 4px;
            font-weight: bold;
        }
        .camera-controls {
            display: flex;
            gap: 10px;
        }

        .button-group {
            display: flex;
            gap: 5px;
            margin-top: 5px;
        }
        
        .button-group button {
            flex: 1;
            padding: 8px;
            background-color: #e0e0e0;
            color: #333;
            border: none;
            border-radius: 4px;
            cursor: pointer;
        }
        
        .button-group button:hover {
            background-color: #d0d0d0;
        }
        
        .button-group button.active {
            background-color: #4a65ff;
            color: white;
        }
        
        .light-mode-btn.active#light-hazard {
            background-color: #ff9900;
        }
        
        .signal-btn.active#signal-left {
            background-color: #ff9900;
        }
        
        .signal-btn.active#signal-right {
            background-color: #ff9900;
        }
    </style>
</head>
<body>
    <h1>Go-Kart Control Dashboard</h1>
    
    <div class="panel camera-panel" id="camera-panel">
        <h2>Live Camera Feed</h2>
        <div class="camera-container">
            <img id="camera-feed" src="data:image/gif;base64,R0lGODlhAQABAAAAACH5BAEKAAEALAAAAAABAAEAAAICTAEAOw==" alt="Camera Feed">
            <div class="camera-overlay" id="camera-status">Connecting...</div>
        </div>
        <div class="camera-controls">
            <button id="camera-toggle">Pause Feed</button>
            <button id="camera-snapshot">Take Snapshot</button>
        </div>
    </div>
    
    <div class="dashboard">
        <div class="panel">
            <h2>Current Metrics</h2>
            <div class="metrics">
                <div class="metric">
                    <div class="metric-value" id="speed-value">0.0</div>
                    <div class="metric-label">Speed (km/h)</div>
                </div>
                <div class="metric">
                    <div class="metric-value" id="throttle-value">0</div>
                    <div class="metric-label">Throttle (%)</div>
                </div>
                <div class="metric">
                    <div class="metric-value" id="battery-value">0.0</div>
                    <div class="metric-label">Battery (V)</div>
                </div>
                <div class="metric">
                    <div class="metric-value" id="temp-value">0.0</div>
                    <div class="metric-label">Motor Temp (°C)</div>
                </div>
                <div class="metric">
                    <div class="metric-value" id="brake-value">0.0</div>
                    <div class="metric-label">Brake (%)</div>
                </div>
                <div class="metric">
                    <div class="metric-value" id="steering-value">0.0</div>
                    <div class="metric-label">Steering (°)</div>
                </div>
            </div>
        </div>
        
        <div class="panel">
            <h2>Controls</h2>
            <div class="controls">
                <button id="emergency-stop" class="stop">EMERGENCY STOP</button>
                
                <div class="control-group">
                    <label for="speed-control">Speed (km/h): <span id="speed-display">0</span></label>
                    <input type="range" id="speed-control" min="0" max="60" value="0">
                </div>
                
                <div class="control-group">
                    <label for="steering-control">Steering (°): <span id="steering-display">0</span></label>
                    <input type="range" id="steering-control" min="-45" max="45" value="0">
                </div>
                
                <div class="control-group">
                    <label for="brake-control">Brake (%): <span id="brake-display">0</span></label>
                    <input type="range" id="brake-control" min="0" max="100" value="0">
                </div>
                
                <button id="send-commands">Send Commands</button>
            </div>
        </div>

        <div class="panel">
    <h2>Lighting Controls</h2>
    <div class="controls">
        <div class="control-group">
            <label>Light Mode:</label>
            <div class="button-group">
                <button id="light-off" class="light-mode-btn active">Off</button>
                <button id="light-low" class="light-mode-btn">Low</button>
                <button id="light-high" class="light-mode-btn">High</button>
                <button id="light-hazard" class="light-mode-btn">Hazard</button>
            </div>
        </div>

        <div class="control-group">
            <label>Turn Signals:</label>
            <div class="button-group">
                <button id="signal-off" class="signal-btn active">Off</button>
                <button id="signal-left" class="signal-btn">Left</button>
                <button id="signal-right" class="signal-btn">Right</button>
            </div>
        </div>

        <div class="control-group">
            <label>Brake Lights:</label>
            <div class="toggle-container">
                <label class="switch">
                    <input type="checkbox" id="brake-toggle">
                    <span class="slider round"></span>
                </label>
                <span id="brake-status">Off</span>
            </div>
        </div>

        <div class="control-group">
            <label>Test Mode:</label>
            <div class="toggle-container">
                <label class="switch">
                    <input type="checkbox" id="test-toggle">
                    <span class="slider round"></span>
                </label>
                <span id="test-status">Off</span>
            </div>
        </div>

        <div class="control-group">
            <label>Controller Location:</label>
            <div class="button-group">
                <button id="location-front" class="location-btn active">Front</button>
                <button id="location-rear" class="location-btn">Rear</button>
            </div>
        </div>
    </div>
</div>
    </div>
    
    <div class="dashboard">
        <div class="panel">
            <h2>Settings</h2>
            <div class="controls">
                <div class="control-group">
                    <label for="simulation-toggle">Simulation Mode:</label>
                    <div class="toggle-container">
                        <label class="switch">
                            <input type="checkbox" id="simulation-toggle" checked>
                            <span class="slider round"></span>
                        </label>
                        <span id="simulation-status">Enabled</span>
                    </div>
                </div>
            </div>
        </div>
        
        <div class="panel">
            <h2>Historical Data</h2>
            <div class="chart-container">
                <canvas id="historyChart"></canvas>
            </div>
        </div>
    </div>

    <script>
        // Connect to WebSocket server
        const socket = io();
        
        // DOM elements
        const speedValue = document.getElementById('speed-value');
        const throttleValue = document.getElementById('throttle-value');
        const batteryValue = document.getElementById('battery-value');
        const tempValue = document.getElementById('temp-value');
        const brakeValue = document.getElementById('brake-value');
        const steeringValue = document.getElementById('steering-value');
        
        const speedControl = document.getElementById('speed-control');
        const steeringControl = document.getElementById('steering-control');
        const brakeControl = document.getElementById('brake-control');
        const speedDisplay = document.getElementById('speed-display');
        const steeringDisplay = document.getElementById('steering-display');
        const brakeDisplay = document.getElementById('brake-display');
        
        const emergencyStopBtn = document.getElementById('emergency-stop');
        const sendCommandsBtn = document.getElementById('send-commands');
        
        // Camera elements
        const cameraPanel = document.getElementById('camera-panel');
        const cameraFeed = document.getElementById('camera-feed');
        const cameraStatus = document.getElementById('camera-status');
        const cameraToggle = document.getElementById('camera-toggle');
        const cameraSnapshot = document.getElementById('camera-snapshot');
        let cameraActive = true;
        
        // Get simulation mode toggle and status
        const simulationToggle = document.getElementById('simulation-toggle');
        const simulationStatus = document.getElementById('simulation-status');
        
        // Hide simulation control
        simulationToggle.parentElement.parentElement.parentElement.style.display = 'none';
        
        // Update displays when controls change
        speedControl.addEventListener('input', () => {
            speedDisplay.textContent = speedControl.value;
        });
        
        steeringControl.addEventListener('input', () => {
            steeringDisplay.textContent = steeringControl.value;
        });
        
        brakeControl.addEventListener('input', () => {
            brakeDisplay.textContent = brakeControl.value;
        });
        
        // Emergency stop button
        let emergencyActive = false;
        emergencyStopBtn.addEventListener('click', () => {
            emergencyActive = !emergencyActive;
            
            if (emergencyActive) {
                emergencyStopBtn.textContent = 'RELEASE EMERGENCY STOP';
                emergencyStopBtn.style.backgroundColor = '#d13535';
            } else {
                emergencyStopBtn.textContent = 'EMERGENCY STOP';
                emergencyStopBtn.style.backgroundColor = '#ff4a4a';
            }
            
            fetch('/api/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    emergency_stop: emergencyActive
                }),
            });
        });
        
        // Send commands button
        sendCommandsBtn.addEventListener('click', () => {
            fetch('/api/command', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    speed: parseInt(speedControl.value),
                    steering: parseInt(steeringControl.value),
                    brake: parseInt(brakeControl.value)
                }),
            });
        });
        
        // Setup Chart.js
        const ctx = document.getElementById('historyChart').getContext('2d');
        const chart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [
                    {
                        label: 'Speed (km/h)',
                        borderColor: 'rgb(75, 192, 192)',
                        data: [],
                        pointRadius: 0,
                        borderWidth: 2
                    },
                    {
                        label: 'Motor Temp (°C)',
                        borderColor: 'rgb(255, 99, 132)',
                        data: [],
                        pointRadius: 0,
                        borderWidth: 2
                    },
                    {
                        label: 'Battery (V)',
                        borderColor: 'rgb(54, 162, 235)',
                        data: [],
                        pointRadius: 0,
                        borderWidth: 2
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: {
                    duration: 0 // Disable animations for better performance
                },
                elements: {
                    line: {
                        tension: 0.2 // Slight curve for better visualization
                    }
                },
                scales: {
                    x: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Time'
                        },
                        ticks: {
                            maxTicksLimit: 10, // Limit X-axis labels for readability
                            autoSkip: true
                        }
                    },
                    y: {
                        display: true,
                        title: {
                            display: true,
                            text: 'Value'
                        },
                        beginAtZero: true
                    }
                },
                plugins: {
                    legend: {
                        position: 'top'
                    }
                }
            }
        });
        
        // Fetch historical data on load
        fetch('/api/history')
            .then(response => response.json())
            .then(data => {
                if (data.length === 0) return;
                
                // Sort data by timestamp to ensure chronological order
                data.sort((a, b) => a.timestamp - b.timestamp);
                
                // Format timestamps for x-axis
                const labels = data.map(item => {
                    const date = new Date(item.timestamp * 1000);
                    return date.toLocaleTimeString();
                });
                
                // Extract data series
                const speedData = data.map(item => item.speed);
                const tempData = data.map(item => item.motor_temp);
                const batteryData = data.map(item => item.battery_voltage);
                
                // Update chart
                chart.data.labels = labels;
                chart.data.datasets[0].data = speedData;
                chart.data.datasets[1].data = tempData;
                chart.data.datasets[2].data = batteryData;
                chart.update('none');
                
                console.log(`Loaded ${data.length} historical data points`);
            })
            .catch(error => {
                console.error('Error fetching historical data:', error);
            });
        
        // Listen for real-time updates
        socket.on('state_update', (state) => {
            // Update dashboard metrics
            speedValue.textContent = parseFloat(state.speed).toFixed(2);
            throttleValue.textContent = parseInt(state.throttle);
            batteryValue.textContent = parseFloat(state.battery_voltage).toFixed(2);
            tempValue.textContent = parseFloat(state.motor_temp).toFixed(2);
            brakeValue.textContent = parseFloat(state.brake_pressure).toFixed(2);
            steeringValue.textContent = parseFloat(state.steering_angle).toFixed(2);
            
            // Update chart
            const timestamp = new Date(state.timestamp * 1000).toLocaleTimeString();
            
            // Add new data point
            chart.data.labels.push(timestamp);
            chart.data.datasets[0].data.push(state.speed);
            chart.data.datasets[1].data.push(state.motor_temp);
            chart.data.datasets[2].data.push(state.battery_voltage);
            
            // Keep only last 60 seconds of data in chart
            if (chart.data.labels.length > 600) {
                chart.data.labels.shift();
                chart.data.datasets.forEach(dataset => {
                    dataset.data.shift();
                });
            }
            
            // Force complete redraw on each update
            chart.update('none');
        });
        
        // Check camera availability
        fetch('/api/camera/status')
            .then(response => response.json())
            .then(data => {
                if (!data.available) {
                    // Hide camera panel if camera is not available
                    cameraPanel.style.display = 'none';
                    console.log('Camera not available');
                } else {
                    cameraStatus.textContent = 'Camera Ready';
                    setTimeout(() => {
                        cameraStatus.style.display = 'none';
                    }, 3000);
                    console.log('Camera available:', data);
                }
            })
            .catch(error => {
                console.error('Error checking camera status:', error);
                cameraPanel.style.display = 'none';
            });
        
        // Handle camera controls
        socket.on('camera_frame', (data) => {
            if (cameraActive) {
                cameraFeed.src = 'data:image/jpeg;base64,' + data.image;
                if (cameraStatus.style.display !== 'none') {
                    cameraStatus.style.display = 'none';
                }
            }
        });
        
        cameraToggle.addEventListener('click', () => {
            cameraActive = !cameraActive;
            cameraToggle.textContent = cameraActive ? 'Pause Feed' : 'Resume Feed';
            
            if (!cameraActive) {
                // Show last frame but grayed out
                cameraFeed.style.filter = 'grayscale(100%)';
            } else {
                cameraFeed.style.filter = 'none';
            }
        });
        
        cameraSnapshot.addEventListener('click', () => {
            // Create a download link for the current frame
            const link = document.createElement('a');
            link.download = 'gokart-snapshot-' + new Date().toISOString().replace(/:/g, '-') + '.jpg';
            link.href = cameraFeed.src;
            link.click();
        });

        // Light control elements
    const lightModeButtons = document.querySelectorAll('.light-mode-btn');
    const signalButtons = document.querySelectorAll('.signal-btn');
    const locationButtons = document.querySelectorAll('.location-btn');
    const brakeToggle = document.getElementById('brake-toggle');
    const brakeStatus = document.getElementById('brake-status');
    const testToggle = document.getElementById('test-toggle');
    const testStatus = document.getElementById('test-status');
    
    // Light mode control
    lightModeButtons.forEach((button, index) => {
        button.addEventListener('click', () => {
            lightModeButtons.forEach(btn => btn.classList.remove('active'));
            button.classList.add('active');
            
            fetch('/api/lights', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    mode: index
                }),
            });
        });
    });
    
    // Turn signal control
    signalButtons.forEach((button, index) => {
        button.addEventListener('click', () => {
            signalButtons.forEach(btn => btn.classList.remove('active'));
            button.classList.add('active');
            
            fetch('/api/lights', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    signal: index
                }),
            });
        });
    });
    
    // Brake lights control
    brakeToggle.addEventListener('change', () => {
        const isOn = brakeToggle.checked;
        brakeStatus.textContent = isOn ? 'On' : 'Off';
        
        fetch('/api/lights', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                brake: isOn
            }),
        });
    });
    
    // Test mode control
    testToggle.addEventListener('change', () => {
        const isOn = testToggle.checked;
        testStatus.textContent = isOn ? 'On' : 'Off';
        
        fetch('/api/lights', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                test: isOn
            }),
        });
    });
    
    // Location control
    locationButtons.forEach((button, index) => {
        button.addEventListener('click', () => {
            locationButtons.forEach(btn => btn.classList.remove('active'));
            button.classList.add('active');
            
            fetch('/api/lights', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    location: index === 0 ? 'front' : 'rear'
                }),
            });
        });
    });
    
    // Update light controls from state updates
    socket.on('state_update', (state) => {
        // Existing state updates...
        
        // Update light controls
        if ('light_mode' in state) {
            lightModeButtons.forEach((btn, idx) => {
                btn.classList.toggle('active', idx === state.light_mode);
            });
        }
        
        if ('light_signal' in state) {
            signalButtons.forEach((btn, idx) => {
                btn.classList.toggle('active', idx === state.light_signal);
            });
        }
        
        if ('light_brake' in state) {
            brakeToggle.checked = state.light_brake === 1;
            brakeStatus.textContent = state.light_brake === 1 ? 'On' : 'Off';
        }
        
        if ('light_test' in state) {
            testToggle.checked = state.light_test === 1;
            testStatus.textContent = state.light_test === 1 ? 'On' : 'Off';
        }
    });
    </script>
</body>
</html>
            '''
        )
    
    # Start update threads for SocketIO
    update_thread = threading.Thread(target=send_updates, daemon=True)
    update_thread.start()
    
    try:
        # Start the server
        socketio.run(app, host='0.0.0.0', port=5000, debug=False)
    finally:
        print('socketio server shutdown')