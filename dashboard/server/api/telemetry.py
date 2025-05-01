"""
API routes for getting telemetry data from the go-kart
"""

from flask import jsonify, Blueprint, render_template, request
import logging
import time
import requests
from datetime import datetime
from shared.lib.python.telemetry.store import TelemetryStore
from shared.lib.python.can.interface import CANInterfaceWrapper

logger = logging.getLogger(__name__)

# Telemetry Collector API URL
COLLECTOR_API_URL = "http://localhost:5001"

def register_telemetry_routes(app, telemetry_store: TelemetryStore, can_interface: CANInterfaceWrapper):
    """Register API routes for telemetry data"""
    
    # Create a blueprint for telemetry routes
    telemetry_bp = Blueprint('telemetry', __name__, url_prefix='/api/telemetry')
    
    @telemetry_bp.route('/state', methods=['GET'])
    def get_telemetry_state():
        """Get the current state of the go-kart"""
        try:
            # Fetch the current state from the collector API
            response = requests.get(f"{COLLECTOR_API_URL}/api/state/current", timeout=1.0)
            response.raise_for_status()
            return jsonify(response.json())
        except Exception as e:
            logger.error(f"Error getting state from collector: {e}")
            # Fall back to local state if collector is unavailable
            try:
                state = telemetry_store.get_current_state()
                return jsonify(state)
            except Exception as e2:
                logger.error(f"Error getting local state: {e2}")
                return jsonify({'status': 'error', 'message': str(e)}), 500

    @telemetry_bp.route('/history', methods=['GET'])
    def get_telemetry_history():
        """Get telemetry history from the collector API"""
        try:
            # Get query parameters
            limit = request.args.get('limit', 100, type=int)
            offset = request.args.get('offset', 0, type=int)
            
            # Fetch history from the collector API
            response = requests.get(
                f"{COLLECTOR_API_URL}/api/state/history", 
                params={'limit': limit, 'offset': offset},
                timeout=1.0
            )
            response.raise_for_status()
            return jsonify(response.json())
        except Exception as e:
            logger.error(f"Error getting history from collector: {e}")
            # Fall back to local history as last resort
            try:
                history = telemetry_store.get_history(limit=limit)
                return jsonify(history)
            except Exception as e2:
                logger.error(f"Error getting local history: {e2}")
                return jsonify({'status': 'error', 'message': str(e)}), 500
            
    @telemetry_bp.route('/status', methods=['GET'])
    def get_telemetry_status():
        """Get telemetry connection status"""
        try:
            # Try to get status from collector first
            try:
                response = requests.get(f"{COLLECTOR_API_URL}/api/status", timeout=0.5)
                response.raise_for_status()
                return jsonify(response.json())
            except:
                # Fall back to local status if collector unavailable
                state = telemetry_store.get_current_state()
                connected = state.get('connected', False)
                last_update = state.get('last_update', 0)
                current_time = time.time()
                
                status = {
                    'connected': connected,
                    'last_update': last_update,
                    'last_update_formatted': datetime.fromtimestamp(last_update).strftime('%Y-%m-%d %H:%M:%S') if last_update else 'Never',
                    'uptime': current_time - last_update if last_update > 0 else 0,
                    'interface_type': 'CAN' if can_interface.has_can_hardware else 'SIM',
                    'collector_connected': False
                }
                
                return jsonify(status)
        except Exception as e:
            logger.error(f"Error getting telemetry status: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
        
    @telemetry_bp.route('/stream', methods=['GET'])
    def get_telemetry_stream():
        """Display a real-time telemetry stream"""
        try:
            # Fetch sample data from collector to determine fields
            try:
                response = requests.get(f"{COLLECTOR_API_URL}/api/state/history?limit=1", timeout=0.5)
                response.raise_for_status()
                history_data = response.json()
                if history_data and len(history_data) > 0:
                    state_fields = list(history_data[0].keys())
                else:
                    # Fallback fields if no data available
                    state_fields = ['timestamp', 'message_type', 'component_type', 
                                   'component_id', 'command_id', 'value_type', 'value']
            except:
                # Fallback if collector unavailable
                state_fields = ['timestamp', 'message_type', 'component_type', 
                               'component_id', 'command_id', 'value_type', 'value']
            
            return render_template('telemetry_stream.html', 
                                  state_fields=state_fields,
                                  collector_url=COLLECTOR_API_URL)
        except Exception as e:
            logger.error(f"Error rendering telemetry stream: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    # Register the blueprint with the app
    app.register_blueprint(telemetry_bp)
