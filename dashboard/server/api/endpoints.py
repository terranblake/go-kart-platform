import os
import time
import threading
import logging
import can

from flask import Flask, render_template
from flask_cors import CORS
from flask_socketio import SocketIO

from lib.can_interface import CANCommandGenerator
from api.telemetry import register_telemetry_routes
from api.commands import register_command_routes

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# Create Flask app and SocketIO for real-time updates
app = Flask(__name__, template_folder='../templates', static_folder='../static')
CORS(app)
socketio = SocketIO(app, cors_allowed_origins="*")

# Create CAN command generator
command_generator = CANCommandGenerator(channel='can0', bitrate=500000)

# Register API endpoints
register_telemetry_routes(app, command_generator)
register_command_routes(app, command_generator)

# Serve the React frontend
@app.route('/')
def index():
    return render_template('index.html')

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

# Start telemetry thread
telemetry_thread = threading.Thread(target=command_generator.listen_telemetry, daemon=True)
telemetry_thread.start()

if __name__ == "__main__":
    # Start update threads for SocketIO
    update_thread = threading.Thread(target=send_updates, daemon=True)
    update_thread.start()
    
    try:
        # Ensure network is up
        try:
            os.system('sudo ip link set can0 up type can bitrate 500000')
        except:
            print('CAN network check failed')
            exit(1)
            
        # Start the server
        socketio.run(app, host='0.0.0.0', port=5000, debug=False)
    finally:
        print('socketio server shutdown') 