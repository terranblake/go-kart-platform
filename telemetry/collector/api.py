""" Telemetry Collector API Endpoints """

import logging
from flask import Flask, jsonify, Blueprint, request

logger = logging.getLogger(__name__)

def create_api(telemetry_store):
    app = Flask(__name__)
    api_bp = Blueprint('api', __name__, url_prefix='/api')

    @api_bp.route('/state/current', methods=['GET'])
    def get_current_state():
        # Get current state from telemetry_store
        state = telemetry_store.get_current_state(readable=True)
        return jsonify(state)

    @api_bp.route('/state/history', methods=['GET'])
    def get_state_history():
        # Get history from persistent telemetry store with pagination
        limit = request.args.get('limit', 100, type=int)
        offset = request.args.get('offset', 0, type=int)
        
        # Limit to reasonable values
        limit = min(limit, 1000)  # Cap at 1000 records
        
        try:
            # Try to call the get_history method with limit/offset if it exists
            if hasattr(telemetry_store, 'get_history_with_pagination'):
                history = telemetry_store.get_history_with_pagination(limit=limit, offset=offset)
            else:
                # Fallback if pagination isn't supported
                history = telemetry_store.get_history(limit=limit)
                
            return jsonify(history)
        except Exception as e:
            logger.error(f"Error retrieving history: {e}")
            return jsonify({"error": str(e)}), 500

    @api_bp.route('/state/component/<string:comp_type>/<string:comp_id>', methods=['GET'])
    def get_component_state(comp_type, comp_id):
        # Get the latest state for the specific component from the store
        state = telemetry_store.get_component_state(comp_type, comp_id)
        if state:
            return jsonify(state)
        else:
            return jsonify({"error": f"No state found for {comp_type}/{comp_id}"}), 404
            
    @api_bp.route('/status', methods=['GET'])
    def get_status():
        # Return collector status information
        try:
            status = {
                'running': True,
                'database': telemetry_store.get_database_stats() if hasattr(telemetry_store, 'get_database_stats') else {},
                'last_message': telemetry_store.get_last_update_time() if hasattr(telemetry_store, 'get_last_update_time') else None
            }
            return jsonify(status)
        except Exception as e:
            logger.error(f"Error retrieving status: {e}")
            return jsonify({"error": str(e)}), 500

    app.register_blueprint(api_bp)
    return app

# Example of how to run (will be called from collector.py)
# if __name__ == '__main__':
#     # This part is just for testing the API directly
#     from storage import PersistentTelemetryStore # Assuming this exists
#     from shared.lib.python.can.protocol_registry import ProtocolRegistry # Relative path might differ
#     
#     # Create dummy store for testing
#     proto = ProtocolRegistry()
#     store = PersistentTelemetryStore(protocol=proto, db_path=':memory:') 
#     
#     app = create_api(store)
#     app.run(host='0.0.0.0', port=5001) # Use a different port than dashboard 