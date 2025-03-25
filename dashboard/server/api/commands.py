"""
API routes for sending commands to the go-kart
"""

from flask import jsonify, request
import logging
from lib.can.interface import CANInterfaceWrapper

logger = logging.getLogger(__name__)

def register_command_routes(app, can_interface: CANInterfaceWrapper):
    """Register API routes for commanding the go-kart"""
    
    @app.route('/api/command', methods=['POST'])
    def send_command():
        """Send a command to the go-kart"""
        try:
            command_data = request.json
            logger.info(f"Received command request: {command_data}")
            result = can_interface.send_command(
                command_data['component_type'],
                command_data['command_name'],
                command_data['command_data'],
                command_data['value_name'],
                command_data['direct_value'],
            )

            return jsonify({
                'status': 'success' if result else 'error',
                'message': 'Command sent successfully' if result else 'Command failed'
            })
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500