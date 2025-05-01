#!/usr/bin/env python3

""" Telemetry Collector Main Entry Point """

import logging
import configparser
import signal
import sys
import os
import threading

# Add project root to Python path for shared module imports
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from shared.lib.python.can.interface import CANInterfaceWrapper
from shared.lib.python.can.protocol_registry import ProtocolRegistry
from shared.lib.python.telemetry.state import GoKartState
from shared.lib.python.telemetry.persistent_store import PersistentTelemetryStore
from api import create_api

# Global variables for graceful shutdown
stop_event = threading.Event()
can_interface = None
api_thread = None

# Configure logging
logger = logging.getLogger(__name__)

def load_config(config_path='config.ini'):
    """Load configuration from file."""
    config = configparser.ConfigParser()
    try:
        config.read(config_path)
        if not config.sections():
             # Handle case where file exists but is empty or invalid
             logger.warning(f"Config file '{config_path}' is empty or invalid. Using defaults.")
             # Set defaults explicitly if needed, or rely on get defaults
             return config['DEFAULT'] # Return default section even if empty
        return config['DEFAULT']
    except FileNotFoundError:
        logger.error(f"Configuration file not found: {config_path}. Exiting.")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Error loading configuration: {e}. Exiting.")
        sys.exit(1)

def _handle_message(telemetry_store, msg_type, comp_type, comp_id, cmd_id, val_type, value):
    """CAN message handler callback."""
    # Create a GoKartState object from the received data
    state_data = {
        'message_type': msg_type,
        'component_type': comp_type,
        'component_id': comp_id,
        'command_id': cmd_id,
        'value_type': val_type,
        'value': value
    }
    state = GoKartState(**state_data)
    # Update the persistent store
    telemetry_store.update_state(state)
    # logger.debug(f"Handled message: {state.to_dict()}") # Optional: Log handled messages

def signal_handler(signum, frame):
    """Handle termination signals for graceful shutdown."""
    logger.info(f"Received signal {signum}. Initiating shutdown...")
    stop_event.set()
    if can_interface:
        can_interface.stop_processing()
    # Note: Stopping Flask requires more complex handling if run directly.
    # If running via a WSGI server (like gunicorn), the server handles shutdown.
    logger.info("Shutdown complete.")
    sys.exit(0)

def run_api_server(app, host, port):
    """Run the Flask API server."""
    try:
        logger.info(f"Starting API server on {host}:{port}")
        app.run(host=host, port=port)
    except Exception as e:
        logger.error(f"API server error: {e}", exc_info=True)
        stop_event.set() # Signal main thread to stop

def main():
    global can_interface, api_thread

    # Load configuration
    config = load_config()
    log_level = config.get('LOG_LEVEL', 'INFO')
    logging.basicConfig(level=getattr(logging, log_level.upper(), logging.INFO),
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')

    logger.info("Starting Telemetry Collector Service")

    # Initialize components
    protocol_registry = ProtocolRegistry() # Autodetects path
    telemetry_store = PersistentTelemetryStore(
        protocol=protocol_registry,
        db_path=config.get('DB_PATH', 'telemetry.db'),
        max_history_records=config.getint('MAX_HISTORY_RECORDS', 10000)
    )
    can_interface = CANInterfaceWrapper(
        node_id=config.getint('CAN_NODE_ID', 0x02),
        channel=config.get('CAN_INTERFACE', 'can0'),
        baudrate=config.getint('CAN_BAUDRATE', 500000),
        telemetry_store=telemetry_store # Pass store to CAN wrapper
    )

    # Register the handler to use the specific telemetry_store instance
    # This handler will receive ALL messages by default if specific handlers aren't registered
    # We might want to register more specific handlers later if needed.
    # For now, let the CANInterfaceWrapper's default handler update the store.
    # NOTE: The CANInterfaceWrapper already has a _handle_message that updates
    # the provided telemetry_store. We don't need to register another one unless
    # we want *different* behavior here.
    # Let's rely on the wrapper's internal handler for now.
    logger.info("Using CANInterfaceWrapper's default handler to update TelemetryStore.")

    # Start CAN processing
    if can_interface.has_can_hardware:
        can_interface.start_processing()
        logger.info("Started CAN message processing thread.")
    else:
        logger.error("Failed to initialize CAN interface. Exiting.")
        sys.exit(1)

    # Create and start the API server in a separate thread
    api_app = create_api(telemetry_store)
    api_host = config.get('API_HOST', '0.0.0.0')
    api_port = config.getint('API_PORT', 5001)
    api_thread = threading.Thread(target=run_api_server, args=(api_app, api_host, api_port), daemon=True)
    api_thread.start()

    # Register signal handlers for graceful shutdown
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    logger.info("Telemetry Collector Service running. Press Ctrl+C to stop.")

    # Keep the main thread alive while checking the stop event
    while not stop_event.is_set():
        stop_event.wait(timeout=1.0) # Wait with timeout to allow signal handling

    logger.info("Main thread exiting.")

if __name__ == "__main__":
    main()
