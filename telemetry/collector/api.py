""" Telemetry Collector API Endpoints - Supports HTTP REST and WebSocket Upload """

import logging
import asyncio
import json
import threading
import time
from flask import Flask, jsonify, Blueprint, request
import websockets
from shared.lib.python.telemetry.state import GoKartState

logger = logging.getLogger(__name__)

# Global variable to hold the store instance for WebSocket handler
# This is a simple approach; dependency injection might be better for complex apps.
_telemetry_store_instance = None

async def _websocket_handler(websocket, path):
    """Handles incoming WebSocket connections for telemetry upload."""
    global _telemetry_store_instance
    client_addr = websocket.remote_address
    logger.info(f"WebSocket client connected: {client_addr} requesting path: {path}")
    try:
        async for message in websocket:
            handler_entry_time = time.time() # Timestamp when message processing starts
            processed_count_in_batch = 0
            batch_processing_start_time = time.time()
            try:
                data = json.loads(message)
                # Expecting a single record or a list of records
                if isinstance(data, list):
                    records = data
                else:
                    records = [data]

                processed_ids = [] # Track processed IDs instead of timestamps
                for record_dict in records:
                    # Basic validation (can be expanded) - Check for ID now too
                    if not all(k in record_dict for k in ['id', 'timestamp', 'message_type', 'component_type', 'component_id', 'command_id', 'value_type', 'value']):
                        logger.warning(f"Received invalid record format (missing fields) from {client_addr}: {record_dict}")
                        continue # Skip this record

                    record_id = record_dict.get('id')
                    if record_id is None: # Should be caught by above check, but belts and suspenders
                         logger.warning(f"Received record without ID from {client_addr}: {record_dict}")
                         continue
                         
                    # Prepare data for GoKartState (original timestamp, no id or uplinked)
                    state_data = record_dict.copy()
                    state_data.pop('id', None) # Remove id
                    state_data.pop('uplinked', None) # Remove uplinked if present (shouldn't be)

                    try:
                        # --- Measure pre-update_state duration ---
                        pre_update_time = time.time()
                        handler_delay = pre_update_time - handler_entry_time
                        if handler_delay > 0.1: # Log if delay > 100ms
                            logger.warning(f"Handler delay before update_state for record ID {record_id}: {handler_delay:.4f}s")
                        # -------------------------------------
                        
                        state = GoKartState(**state_data) # Use original timestamp for state
                        # The store update handles DB insertion using original timestamp as recorded_at
                        
                        # --- Measure update_state duration ---
                        update_start_time = time.time()
                        _telemetry_store_instance.update_state(state)
                        update_duration = time.time() - update_start_time
                        if update_duration > 0.1: # Log if duration exceeds 100ms
                            logger.warning(f"update_state took {update_duration:.4f}s for record ID {record_id}")
                        # -------------------------------------
                        
                        processed_ids.append(record_id) # Add ID to ack list
                    except Exception as state_update_exc:
                         logger.error(f"Error processing record ID {record_id} from {client_addr}: {state_update_exc}", exc_info=True)
                         # Decide if we should ack this as failed or just skip?
                         # For now, skip adding to processed_ids

                # Send acknowledgement for successfully processed records using IDs
                if processed_ids:
                    ack_message = json.dumps({"status": "ok", "processed_ids": processed_ids})
                    await websocket.send(ack_message)
                    logger.debug(f"Sent ack for {len(processed_ids)} record IDs to {client_addr}")

                # Log batch processing time if it was slow
                batch_duration = time.time() - batch_processing_start_time
                if batch_duration > 0.5: # Log if batch took > 500ms
                     logger.warning(f"Processed batch of {processed_count_in_batch} records in {batch_duration:.4f}s")

            except json.JSONDecodeError:
                logger.error(f"Received invalid JSON from {client_addr}: {message[:100]}...")
                await websocket.send(json.dumps({"status": "error", "message": "Invalid JSON"}))
            except Exception as e:
                logger.error(f"Error processing message from {client_addr}: {e}", exc_info=True)
                try:
                    await websocket.send(json.dumps({"status": "error", "message": "Internal server error"}))
                except websockets.ConnectionClosed:
                    pass # Client might have disconnected already
    except websockets.ConnectionClosedOK:
        logger.info(f"WebSocket client disconnected normally: {client_addr}")
    except websockets.ConnectionClosedError as e:
        logger.warning(f"WebSocket client connection closed with error: {client_addr} - {e}")
    except Exception as e:
        logger.error(f"WebSocket handler error for {client_addr}: {e}", exc_info=True)
    finally:
        logger.info(f"WebSocket connection closed for {client_addr}")

async def _start_websocket_server(host, port):
    """Starts the WebSocket server."""
    logger.info(f"Starting WebSocket server on {host}:{port}")
    # Revert to async with websockets.serve, but keep lambda for handler
    async with websockets.serve(
        lambda ws: _websocket_handler(ws, "/"), # Pass dummy path via lambda
        host, 
        port
    ):
        await asyncio.Future()  # Run forever until cancelled

def _run_flask_app(app, host, port):
    """Runs the Flask app in a separate thread."""
    logger.info(f"Starting Flask HTTP server on {host}:{port}")
    # Use werkzeug server directly; avoid Flask's default reload/debug features in production
    app.run(host=host, port=port, threaded=True)

# Modified create_api to accept role and config, and manage servers
def create_api_server_manager(telemetry_store, role: str, http_host: str, http_port: int, ws_host: str, ws_port: int):
    """Creates the Flask app and manages starting servers based on role."""
    global _telemetry_store_instance
    _telemetry_store_instance = telemetry_store

    app = Flask(__name__)
    api_bp = Blueprint('api', __name__, url_prefix='/api')

    # --- Existing HTTP Endpoints --- 
    @api_bp.route('/state/current', methods=['GET'])
    def get_current_state():
        # Get current state from telemetry_store
        state = telemetry_store.get_current_state(readable=True)
        return jsonify(state)

    @api_bp.route('/state/history', methods=['GET'])
    def get_state_history():
        # Get history from persistent telemetry store
        limit = request.args.get('limit', 100, type=int)
        offset = request.args.get('offset', 0, type=int)
        limit = min(limit, 1000) # Cap limit

        try:
            # The store now handles role-based filtering
            if hasattr(telemetry_store, 'get_history_with_pagination'):
                history = telemetry_store.get_history_with_pagination(limit=limit, offset=offset)
            else:
                history = telemetry_store.get_history(limit=limit)
            return jsonify(history)
        except Exception as e:
            logger.error(f"Error retrieving history: {e}")
            return jsonify({"error": str(e)}), 500

    @api_bp.route('/state/component/<string:comp_type>/<string:comp_id>', methods=['GET'])
    def get_component_state(comp_type, comp_id):
        state = telemetry_store.get_component_state(comp_type, comp_id)
        if state:
            return jsonify(state)
        else:
            return jsonify({"error": f"No state found for {comp_type}/{comp_id}"}), 404

    @api_bp.route('/status', methods=['GET'])
    def get_status():
        try:
            status = {
                'running': True,
                'role': telemetry_store.role, # Assuming role is accessible
                'database': telemetry_store.get_database_stats() if hasattr(telemetry_store, 'get_database_stats') else {},
                'last_message': telemetry_store.get_last_update_time() if hasattr(telemetry_store, 'get_last_update_time') else None
            }
            return jsonify(status)
        except Exception as e:
            logger.error(f"Error retrieving status: {e}")
            return jsonify({"error": str(e)}), 500

    app.register_blueprint(api_bp)

    # --- Server Management --- 
    flask_thread = threading.Thread(target=_run_flask_app, args=(app, http_host, http_port), daemon=True)
    flask_thread.start()

    if role == 'remote':
        # Start WebSocket server in the main thread using asyncio
        # This call now blocks until the loop is stopped by signal_handler
        try:
            logger.info("API Manager entering asyncio loop for WebSocket (remote role)...")
            asyncio.run(_start_websocket_server(ws_host, ws_port))
            logger.info("API Manager asyncio loop finished.")
        except KeyboardInterrupt: # Should be caught by signal_handler now primarily
            logger.info("WebSocket server received KeyboardInterrupt (likely handled by signal)...")
        except Exception as e:
             logger.error(f"WebSocket server failed: {e}", exc_info=True)
    else:
        # If only vehicle role, main thread should handle waiting.
        # Do not join the Flask thread here.
        logger.info("API Manager started Flask thread (vehicle role), returning to main loop.")
        # flask_thread.join() # REMOVED - prevents main thread exit

# Example of how to run (will be called from collector.py)
# if __name__ == '__main__':
#     from shared.lib.python.telemetry.persistent_store import PersistentTelemetryStore
#     from shared.lib.python.can.protocol_registry import ProtocolRegistry
#
#     proto = ProtocolRegistry()
#     # Example: Run as remote
#     store = PersistentTelemetryStore(protocol=proto, db_path='remote_test.db', role='remote')
#     create_api_server_manager(store, role='remote', http_host='0.0.0.0', http_port=5001, ws_host='0.0.0.0', ws_port=8765) 