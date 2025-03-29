import time
import logging
import threading
import os
from flask import Flask, Blueprint, render_template
from flask_socketio import SocketIO
from flask_cors import CORS

# Import CAN interface
from lib.can.interface import CANInterfaceWrapper
from api.telemetry import register_telemetry_routes
from api.commands import register_command_routes
from api.direct_commands import register_direct_command_routes
from api.protocol import register_protocol_routes
from api.animations import register_animation_routes
from lib.can.protocol_registry import ProtocolRegistry
from lib.telemetry.store import TelemetryStore

# Let ProtocolRegistry autodetect the path
protocol_path = None

# Create telemetry store and protocol registry
telemetry_store = TelemetryStore()
protocol_registry = ProtocolRegistry(pb_path=protocol_path)

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
state_history = []

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
register_animation_routes(app, can_interface, socketio, os.path.join(os.getcwd(), './tools/animations'))

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
            # Process CAN messages is now handled automatically by the background thread
            # No need to call can_interface.process() here
            
            # Get the current state from telemetry store
            current_time = time.time()
            if current_time - last_update >= UPDATE_INTERVAL:
                state = telemetry_store.get_current_state()
                
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

if __name__ == "__main__":
    # Start the server
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)
