"""
API routes for getting telemetry data from the go-kart
"""

from flask import jsonify, Blueprint, render_template
import logging
import time
from datetime import datetime
from lib.telemetry.store import TelemetryStore
from lib.can.interface import CANInterfaceWrapper
logger = logging.getLogger(__name__)

def register_telemetry_routes(app, telemetry_store: TelemetryStore, can_interface: CANInterfaceWrapper):
    """Register API routes for telemetry data"""
    
    # Create a blueprint for telemetry routes
    telemetry_bp = Blueprint('telemetry', __name__, url_prefix='/api/telemetry')
    
    @telemetry_bp.route('/state', methods=['GET'])
    def get_telemetry_state():
        """Get the current state of the go-kart"""
        try:
            state = telemetry_store.get_current_state()
            return jsonify(state)
        except Exception as e:
            logger.error(f"Error getting state: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    @telemetry_bp.route('/history', methods=['GET'])
    def get_telemetry_history():
        """Get telemetry history"""
        try:
            history = telemetry_store.get_history()
            return jsonify(history)
        except Exception as e:
            logger.error(f"Error getting history: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
            
    @telemetry_bp.route('/status', methods=['GET'])
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
                'interface_type': 'CAN' if can_interface.has_can_hardware else 'SIM'
            }
            
            return jsonify(status)
        except Exception as e:
            logger.error(f"Error getting telemetry status: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
        
    @telemetry_bp.route('/stream', methods=['GET'])
    def get_telemetry_stream():
        """Display a real-time telemetry stream"""
        try:
            # Get initial state fields to determine table columns
            current_state = telemetry_store.get_current_state()
            state_fields = []
            if current_state:
                # Get a list of fields we want to display
                state_fields = ['timestamp', 'message_type', 'component_type', 
                               'component_id', 'command_id', 'value_type', 'value']
            
            return render_template('telemetry_stream.html', 
                                  state_fields=state_fields)
        except Exception as e:
            logger.error(f"Error rendering telemetry stream: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    # Register the blueprint with the app
    app.register_blueprint(telemetry_bp)
