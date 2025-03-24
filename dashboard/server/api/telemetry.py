# this should be for getting telemetry data from the go-kart
from flask import Blueprint, jsonify
import logging

logger = logging.getLogger(__name__)

def register_telemetry_routes(app, command_generator):
    """Register API routes for telemetry data"""
    
    @app.route('/api/state', methods=['GET'])
    def get_state():
        """Get the current state of the go-kart"""
        try:
            state = command_generator.get_current_state()
            return jsonify(state)
        except Exception as e:
            logger.error(f"Error getting state: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    @app.route('/api/history', methods=['GET'])
    def get_history():
        """Get telemetry history"""
        try:
            # For now, just return a simple list
            # In a real implementation, this would query stored telemetry data
            return jsonify([])
        except Exception as e:
            logger.error(f"Error getting history: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
