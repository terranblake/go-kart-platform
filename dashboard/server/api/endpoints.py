import time
import logging
import threading
import os
import requests # Import requests library
from flask import Flask, Blueprint, render_template
from flask_socketio import SocketIO
from flask_cors import CORS

# Import CAN interface
from can.interface import CANInterfaceWrapper
from api.telemetry import register_telemetry_routes
from api.commands import register_command_routes
from api.direct_commands import register_direct_command_routes
from api.protocol import register_protocol_routes
from can.protocol_registry import ProtocolRegistry
from telemetry.store import TelemetryStore

# Let ProtocolRegistry autodetect the path
protocol_path = None

# Create telemetry store and protocol registry
protocol_registry = ProtocolRegistry(pb_path=protocol_path)
telemetry_store = TelemetryStore(protocol=protocol_registry)

# Initialize CAN interface with telemetry store reference and protocol registry
can_interface = CANInterfaceWrapper(
    node_id=0x01, 
    channel='can0', 
    telemetry_store=telemetry_store
)

# Start automatic message processing
can_interface.start_processing()

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
# state_history = [] # Remove dashboard-local history

# Define Telemetry Collector API URL (ideally from config/env)
COLLECTOR_API_URL = "http://localhost:5001" # Default from collector config

# Home page route
@app.route('/')
def index():
    """Render the dashboard."""
    logger.debug("Index route accessed!")
    try:
        return render_template('index.html')
    except Exception as e:
        logger.debug(f"Error rendering template: {e}")
        return f"Error: {e}", 500

# Register blueprint
app.register_blueprint(api)

# Register routes from the other modules
register_telemetry_routes(app, telemetry_store, can_interface)
register_command_routes(app, can_interface)
register_direct_command_routes(app, can_interface)
register_protocol_routes(app, protocol_registry)

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
    """Send periodic state updates to connected clients by fetching from Telemetry Collector."""
    global running, clients_connected

    logger.info("Starting update thread (fetching from collector)")
    running = True
    last_update_time = time.time()

    while running and clients_connected:
        current_time = time.time()
        if current_time - last_update_time >= UPDATE_INTERVAL:
            try:
                # Fetch the current state from the Telemetry Collector API
                response = requests.get(f"{COLLECTOR_API_URL}/api/state/current", timeout=0.5) # Add timeout
                response.raise_for_status() # Raise exception for bad status codes (4xx or 5xx)
                state = response.json()

                # Send the fetched state to all connected clients
                socketio.emit('state_update', state)
                last_update_time = current_time

                # Remove local history management
                # state_history.append(state)
                # if len(state_history) > 1000:
                #     state_history.pop(0)

            except requests.exceptions.Timeout:
                 logger.warning("Timeout fetching state from Telemetry Collector.")
            except requests.exceptions.ConnectionError:
                logger.error("Connection error fetching state from Telemetry Collector. Is it running?")
                # Optional: Sleep longer if connection fails repeatedly
                time.sleep(2.0)
            except requests.exceptions.RequestException as e:
                logger.error(f"Error fetching state from Telemetry Collector: {e}")
                time.sleep(1.0) # Longer sleep on other request errors
            except Exception as e:
                logger.error(f"Error in update thread: {e}", exc_info=True)
                time.sleep(1.0) # Longer sleep on general error

        # Sleep a bit to avoid hogging CPU, especially if requests fail quickly
        time.sleep(0.05)

    logger.info("Update thread stopped")

if __name__ == "__main__":
    # Start the server
    socketio.run(app, host='0.0.0.0', port=5000, debug=False) 