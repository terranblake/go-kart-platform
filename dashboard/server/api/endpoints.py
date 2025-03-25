import os
import time
import json
import logging
import threading
from datetime import datetime
from flask import Flask, request, jsonify, Blueprint
from flask_socketio import SocketIO
from flask_cors import CORS

# Import CAN interface
from lib.can.interface import can_interface
from api.telemetry import register_telemetry_routes
from api.commands import register_command_routes
from api.protocol import register_protocol_routes

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Initialize Flask app
app = Flask(__name__, static_folder='../static', template_folder='../templates')
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

# Setup API routes
api = Blueprint('api', __name__)

# Background thread for sending periodic state updates
update_thread = None
thread_lock = threading.Lock()

# Variables for background thread
UPDATE_INTERVAL = 0.1  # seconds
clients_connected = False
running = True
state_history = []

# Routes for telemetry
@api.route('/telemetry/current', methods=['GET'])
def get_current_state():
    """Get the current state of the Go-Kart."""
    state = can_interface.get_current_state()
    return jsonify(state)

@api.route('/telemetry/history', methods=['GET'])
def get_telemetry_history():
    """Get historical telemetry data."""
    history = can_interface.get_history()
    return jsonify(history)

# Routes for commands
@api.route('/commands/emergency', methods=['POST'])
def emergency_command():
    """Send an emergency command."""
    data = request.json
    if 'stop' in data:
        result = can_interface.send_emergency_stop(data['stop'])
        return jsonify({'success': result})
    return jsonify({'success': False, 'error': 'Missing stop parameter'})

@api.route('/commands/speed', methods=['POST'])
def speed_command():
    """Set the motor speed."""
    data = request.json
    if 'speed' in data:
        # Send speed command to main motor
        result = can_interface.send_command('motors.main', 'SPEED', direct_value=data['speed'])
        return jsonify({'success': result})
    return jsonify({'success': False, 'error': 'Missing speed parameter'})

@api.route('/commands/throttle', methods=['POST'])
def throttle_command():
    """Set the throttle position."""
    data = request.json
    if 'position' in data:
        # Send throttle command
        result = can_interface.send_command('controls.throttle', 'POSITION', direct_value=data['position'])
        return jsonify({'success': result})
    return jsonify({'success': False, 'error': 'Missing position parameter'})

@api.route('/commands/brake', methods=['POST'])
def brake_command():
    """Set the brake position."""
    data = request.json
    if 'position' in data:
        # Send brake command
        result = can_interface.send_command('controls.brake', 'POSITION', direct_value=data['position'])
        return jsonify({'success': result})
    return jsonify({'success': False, 'error': 'Missing position parameter'})

@api.route('/commands/steering', methods=['POST'])
def steering_command():
    """Set the steering angle."""
    data = request.json
    if 'angle' in data:
        # Send steering command
        result = can_interface.send_command('controls.steering', 'ANGLE', direct_value=data['angle'])
        return jsonify({'success': result})
    return jsonify({'success': False, 'error': 'Missing angle parameter'})

@api.route('/commands/headlights', methods=['POST'])
def headlights_command():
    """Set the headlight state."""
    data = request.json
    if 'state' in data:
        # Convert boolean to ON/OFF
        value = 'ON' if data['state'] else 'OFF'
        # Send headlight command
        result = can_interface.send_command('lights.headlights', 'POWER', value_name=value)
        return jsonify({'success': result})
    return jsonify({'success': False, 'error': 'Missing state parameter'})

# Routes for protocol
@api.route('/protocol', methods=['GET'])
def get_protocol():
    """Get the protocol definition."""
    return jsonify(can_interface.protocol.registry)

@api.route('/protocol/components', methods=['GET'])
def get_components():
    """Get the components definition."""
    return jsonify(can_interface.protocol.registry.get('components', {}))

@api.route('/protocol/commands', methods=['GET'])
def get_commands():
    """Get the commands definition."""
    return jsonify(can_interface.protocol.registry.get('commands', {}))

# Home page route
@app.route('/')
def index():
    """Render the dashboard."""
    return app.send_static_file('index.html')

# Register blueprint
app.register_blueprint(api)

# Register routes from the other modules
register_telemetry_routes(app, can_interface)
register_command_routes(app, can_interface)
register_protocol_routes(app)

# Socket.IO handlers
@socketio.on('connect')
def handle_connect():
    """Handle client connection."""
    global clients_connected, update_thread
    
    logger.info("Client connected")
    clients_connected = True
    
    # Start the update thread if it's not already running
    with thread_lock:
        if update_thread is None or not update_thread.is_alive():
            update_thread = threading.Thread(target=send_updates, daemon=True)
            update_thread.start()
            logger.info("Started update thread")

@socketio.on('disconnect')
def handle_disconnect():
    """Handle client disconnection."""
    global clients_connected
    
    logger.info("Client disconnected")
    # Check if there are still clients connected
    clients_connected = len(socketio.server.manager.rooms.get('/', [])) > 0

def send_updates():
    """Send periodic state updates to connected clients."""
    global running, clients_connected
    
    logger.info("Starting update thread")
    running = True
    last_update = time.time()
    
    while running and clients_connected:
        try:
            # Process CAN messages
            can_interface.process()
            
            # Get the current state
            current_time = time.time()
            if current_time - last_update >= UPDATE_INTERVAL:
                state = can_interface.get_current_state()
                
                # Send the state to all connected clients
                socketio.emit('state_update', state)
                last_update = current_time
                
                # Add to history (limit to 1000 entries)
                state_history.append(state)
                if len(state_history) > 1000:
                    state_history.pop(0)
            
            # Sleep a bit to avoid hogging CPU
            time.sleep(0.01)
        except Exception as e:
            logger.error(f"Error in update thread: {e}")
            time.sleep(1.0)  # Longer sleep on error
    
    logger.info("Update thread stopped")

# Generate test data in debug mode
if os.environ.get('DEBUG', 'False').lower() in ('true', '1', 't'):
    import random
    
    def generate_test_data():
        """Generate random test data."""
        last_update = time.time()
        speed = 0
        throttle = 0
        
        while True:
            current_time = time.time()
            if current_time - last_update >= 1.0:  # Update every second
                # Simulate speed based on throttle
                target_speed = throttle
                if speed < target_speed:
                    speed = min(speed + 5, target_speed)
                elif speed > target_speed:
                    speed = max(speed - 3, target_speed)
                
                # Randomly change throttle
                if random.random() < 0.3:
                    throttle = random.randint(0, 100)
                
                # Send fake messages to update state
                can_interface._handle_motor_speed(0, 0, 0, 0, 0, speed)
                can_interface._handle_throttle(0, 0, 0, 0, 0, throttle)
                can_interface._handle_battery_voltage(0, 0, 0, 0, 0, 120)  # 12.0V
                
                last_update = current_time
            
            time.sleep(0.1)
    
    # Start test data generator
    test_thread = threading.Thread(target=generate_test_data, daemon=True)
    test_thread.start()

if __name__ == "__main__":
    # Start the server
    socketio.run(app, host='0.0.0.0', port=5000, debug=False) 