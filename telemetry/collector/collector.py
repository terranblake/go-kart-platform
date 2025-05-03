#!/usr/bin/env python3

""" Telemetry Collector Main Entry Point - Unified for Vehicle/Remote Roles """

import logging
import configparser
import signal
import sys
import os
import threading
import time # For shutdown wait
import argparse # Import argparse
import asyncio
import redis # Add redis import for potential config checks

# Add project root to Python path for shared module imports
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
if PROJECT_ROOT not in sys.path:
    sys.path.insert(0, PROJECT_ROOT)

from shared.lib.python.can.interface import CANInterfaceWrapper
from shared.lib.python.can.protocol_registry import ProtocolRegistry
# from shared.lib.python.telemetry.state import GoKartState # Not directly used here now
from shared.lib.python.telemetry.persistent_store import PersistentTelemetryStore
from api import create_api_server_manager # Use the new manager function
from ping_broadcaster import PingBroadcaster # Import PingBroadcaster


# Global variables for graceful shutdown
stop_event = threading.Event()
can_interface = None
time_sync_thread = None

# Configure logging
logger = logging.getLogger(__name__)

# --- Argument Parsing ---
def parse_arguments():
    parser = argparse.ArgumentParser(description="Telemetry Collector Service")
    parser.add_argument(
        '--config', 
        type=str, 
        default='config.ini', # Default config file name
        help='Path to the configuration file (default: telemetry/collector/config.ini relative to script)'
    )
    return parser.parse_args()

def load_config(config_path='config.ini'):
    """Load configuration from file."""
    abs_config_path = os.path.abspath(config_path)
    # Check if it exists directly first
    if not os.path.exists(abs_config_path):
        # If not found directly, try relative to the script directory as fallback
        script_dir = os.path.dirname(__file__)
        path_relative_to_script = os.path.join(script_dir, config_path)
        if os.path.exists(path_relative_to_script):
            abs_config_path = path_relative_to_script
            logger.debug(f"Config path '{config_path}' resolved relative to script: {abs_config_path}")
        else:
             # If neither exists, log warning and rely purely on defaults
             logger.warning(f"Config file '{config_path}' (resolved to '{abs_config_path}') not found. Using defaults.")
             abs_config_path = None # Signal that file wasn't read

    # Initialize parser, NO DEFAULTS HERE
    config = configparser.ConfigParser(interpolation=None) # Disable interpolation for Redis URLs

    try:
        if abs_config_path:
            read_ok = config.read(abs_config_path)
            if not read_ok:
                 logger.warning(f"Config file '{abs_config_path}' exists but failed to read. Using defaults.")
        # Return the ConfigParser object itself
        return config
    except Exception as e:
        logger.error(f"Error loading configuration from {abs_config_path or 'defaults'}: {e}. Exiting.")
        sys.exit(1)


def signal_handler(signum, frame):
    """Handle termination signals for graceful shutdown."""
    global stop_event, time_sync_thread, can_interface
    if not stop_event.is_set(): # Prevent double handling
        logger.info(f"Received signal {signum}. Initiating shutdown...")
        stop_event.set() # Signal all loops to stop
        
        if time_sync_thread: # Stop time sync thread
             logger.info("Stopping Time Sync Broadcaster...")
             time_sync_thread.stop() # Relies on wait timeout

        if can_interface:
            logger.info("Stopping CAN processing...")
            can_interface.stop_processing()
            
            # Also stop the pruning timer in the telemetry store
            # REMOVED: No stop_pruning in Redis implementation
            
        # Stop the asyncio loop if it's running (for remote role)
        try:
            loop = asyncio.get_running_loop()
            if loop.is_running():
                logger.info("Stopping asyncio event loop...")
                loop.stop()
        except RuntimeError: # No running loop
            pass 
        except Exception as e:
             logger.error(f"Error stopping asyncio loop: {e}")

        # API server shutdown: Flask thread is daemon, WS server stopped by loop.stop()
        logger.info("Shutdown signal processed. Main thread will exit shortly.")
        # No need for sleep or join here, let main loop check stop_event
    else:
        logger.info("Shutdown already in progress.")
        exit(0)


def main():
    global can_interface, time_sync_thread

    args = parse_arguments()
    config_file_path = args.config
    # config is now a ConfigParser object
    config = load_config(config_file_path)

    # Setup logging using config.get with fallback
    log_level = config.get('DEFAULT', 'LOG_LEVEL', fallback='INFO')
    logging.basicConfig(level=getattr(logging, log_level.upper(), logging.INFO),
                        format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')

    # Determine role using config.get with fallback
    role = config.get('DEFAULT', 'ROLE', fallback='vehicle')
    # Log the config file path provided via argument
    logger.info(f"Attempting to use configuration file: {config_file_path}") 
    logger.info(f"Starting Telemetry Collector Service in ROLE: {role}")

    protocol_registry = ProtocolRegistry()
    
    # --- Redis Configuration ---
    local_redis_config = {
        'host': config.get('REDIS', 'HOST', fallback='localhost'),
        'port': config.getint('REDIS', 'PORT', fallback=6379),
        'db': config.getint('REDIS', 'DB', fallback=0)
    }
    remote_redis_config = None
    if role == 'vehicle':
        remote_host = config.get('REDIS_REMOTE', 'HOST', fallback=None)
        if remote_host: # Only configure remote if host is specified
            remote_redis_config = {
                'host': remote_host,
                'port': config.getint('REDIS_REMOTE', 'PORT', fallback=6379),
                'db': config.getint('REDIS_REMOTE', 'DB', fallback=0)
                # Consider adding password/auth options here if needed
            }
        else:
             logger.info("Remote Redis host not specified in config, remote replication disabled.")

    # --- Instantiate Redis-based Store ---
    logger.info(f"Initializing Redis Telemetry Store (Local: {local_redis_config['host']}:{local_redis_config['port']})")
    telemetry_store = PersistentTelemetryStore(
        protocol=protocol_registry,
        local_redis_config=local_redis_config,
        remote_redis_config=remote_redis_config, # Pass remote config (can be None)
        max_history_records=config.getint('REDIS', 'MAX_HISTORY', fallback=10000), # Use REDIS section
        role=role,
        retention_seconds=config.getint('REDIS', 'RETENTION_SECONDS', fallback=600) # Use REDIS section
        # Removed SQLite specific args: db_path, dashboard_retention_s, local_uplinked_retention_s
    )

    # --- Conditional Initialization based on Role ---
    if role == 'vehicle':
        logger.info("Initializing CAN Interface (Role: Vehicle)...")
        try:
            can_interface = CANInterfaceWrapper(
                # Use config.get with type and fallback
                node_id=int(str(config.get('DEFAULT', 'CAN_NODE_ID', fallback='0x02')), 0),
                channel=config.get('DEFAULT', 'CAN_INTERFACE', fallback='can0'),
                baudrate=config.getint('DEFAULT', 'CAN_BAUDRATE', fallback=500000),
                telemetry_store=telemetry_store
            )

            can_interface.start_processing()
            logger.info("Started CAN message processing thread.")
            
            # Start Time Sync Broadcaster if vehicle and CAN is up
            time_sync_interval = config.getfloat('DEFAULT', 'TIME_SYNC_INTERVAL_S', fallback=0.1)
            time_sync_thread = PingBroadcaster(can_interface, telemetry_store, protocol_registry, time_sync_interval, stop_event)
            time_sync_thread.start()
        except Exception as e:
             logger.error(f"Error initializing CAN interface: {e}. CAN listener disabled.", exc_info=True)
             can_interface = None
    else:
        logger.info("CAN Listener is disabled (Role: Remote).")

    # Register signal handlers AFTER potentially starting threads
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    logger.info("Starting API Server Manager...")
    try:
        # Need to pass abs_config_path to log correctly which config was used
        # Need to re-check path resolution in load_config
        create_api_server_manager(
            telemetry_store=telemetry_store,
            role=role,
            # Use config.get with type and fallback
            http_host=config.get('DEFAULT', 'HTTP_HOST', fallback='0.0.0.0'),
            http_port=config.getint('DEFAULT', 'HTTP_PORT', fallback=5001),
        )
    except Exception as e:
        logger.error(f"API Server Manager failed to start or crashed: {e}", exc_info=True)
        signal_handler(signal.SIGTERM, None) # Simulate signal

    # If create_api_server_manager returns cleanly (e.g., after catching KeyboardInterrupt),
    # or if threads are daemonized, the main thread might exit here.
    # --- Keep main thread alive for vehicle role until stop signal --- 
    if role == 'vehicle':
        logger.info("Vehicle collector entering main wait loop...")
        if time_sync_thread:
            time_sync_thread.join()
            logger.debug("Time sync thread joined.")
        
        logger.info("Vehicle collector main wait loop finished.")
    # For remote role, asyncio.run() blocks until stopped by signal handler
    # If API Server Manager crashes, signal_handler is called which should stop loops.
    
    logger.info("Telemetry Collector Service main function finished.")


if __name__ == "__main__":
    main()
