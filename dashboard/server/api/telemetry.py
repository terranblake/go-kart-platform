"""
API routes for getting telemetry data from the go-kart
"""

from flask import jsonify
import logging
import time
from datetime import datetime
from lib.telemetry.store import TelemetryStore

logger = logging.getLogger(__name__)

def register_telemetry_routes(app, telemetry_store: TelemetryStore):
    """Register API routes for telemetry data"""
    
    @app.route('/api/state', methods=['GET'])
    def get_state():
        """Get the current state of the go-kart"""
        try:
            state = telemetry_store.get_current_state()
            return jsonify(state)
        except Exception as e:
            logger.error(f"Error getting state: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    @app.route('/api/history', methods=['GET'])
    def get_history():
        """Get telemetry history"""
        try:
            history = telemetry_store.get_history()
            return jsonify(history)
        except Exception as e:
            logger.error(f"Error getting history: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
            
    @app.route('/api/telemetry/status', methods=['GET'])
    def get_telemetry_status():
        """Get telemetry connection status"""
        try:
            state = telemetry_store.get_current_state()
            connected = state.get('connected', False)
            last_update = state.get('last_update', 0)
            current_time = time.time()
            
            status = {
                'connected': connected,
                'last_update': last_update,
                'last_update_formatted': datetime.fromtimestamp(last_update).strftime('%Y-%m-%d %H:%M:%S') if last_update else 'Never',
                'uptime': current_time - last_update if last_update > 0 else 0,
                'interface_type': 'CAN' if telemetry_store.can_interface else 'Mock'
            }
            
            return jsonify(status)
        except Exception as e:
            logger.error(f"Error getting telemetry status: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
