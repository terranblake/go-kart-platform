""" Telemetry Collector API Endpoints - Supports HTTP REST and WebSocket Upload """

import logging
import json
import threading
import time
from flask import Flask, jsonify, Blueprint, request
from shared.lib.python.telemetry.state import GoKartState

logger = logging.getLogger(__name__)

# Global variable to hold the store instance for WebSocket handler
# This is a simple approach; dependency injection might be better for complex apps.
_telemetry_store_instance = None

def _run_flask_app(app, host, port):
    """Runs the Flask app in a separate thread."""
    logger.info(f"Starting Flask HTTP server on {host}:{port}")
    # Use werkzeug server directly; avoid Flask's default reload/debug features in production
    app.run(host=host, port=port, threaded=True)

# Modified create_api to accept role and config, and manage servers
def create_api_server_manager(telemetry_store, role: str, http_host: str, http_port: int):
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

    logger.info(f"API Manager started Flask thread ({role} role), returning to main loop.")

# Example of how to run (will be called from collector.py)
# if __name__ == '__main__':
#     from shared.lib.python.telemetry.persistent_store import PersistentTelemetryStore
#     from shared.lib.python.can.protocol_registry import ProtocolRegistry
#
#     proto = ProtocolRegistry()
#     # Example: Run as remote
#     store = PersistentTelemetryStore(protocol=proto, db_path='remote_test.db', role='remote')
#     create_api_server_manager(store, role='remote', http_host='0.0.0.0', http_port=5001) 