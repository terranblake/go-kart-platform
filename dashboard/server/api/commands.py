"""
API routes for sending commands to the go-kart
"""

from flask import jsonify, request, Blueprint
import logging
from lib.can.interface import CANInterfaceWrapper

logger = logging.getLogger(__name__)

def register_command_routes(app, can_interface: CANInterfaceWrapper):
    """Register API routes for commanding the go-kart"""
    
    # Create a blueprint for command routes
    command_bp = Blueprint('command', __name__, url_prefix='/api/command')
    
    @command_bp.route('', methods=['POST'])
    def send_command():
        """Send a command to the go-kart"""
        try:
            command_data = request.json
            logger.info(f"Received command request: {command_data}")
            
            # Extract parameters with defaults for missing fields
            component_type = command_data.get('component_type')
            component_name = command_data.get('component_name')
            command_name = command_data.get('command_name')
            value_name = command_data.get('value_name')
            direct_value = command_data.get('direct_value')
            cmd_data = command_data.get('command_data', {})
            
            # For backward compatibility with older frontend
            if 'value' in command_data and direct_value is None:
                direct_value = command_data['value']
            
            # Build component path (e.g., "lights.front")
            component_path = f"{component_type.lower()}.{component_name}" if component_type and component_name else None
            
            # Log the parsed command
            logger.info(f"Sending command: {component_path}.{command_name} = {value_name} ({direct_value})")
            
            # Call the CAN interface with all required parameters
            # Ensure direct_value is an integer if provided
            if direct_value is not None:
                try:
                    direct_value = int(direct_value)
                except (ValueError, TypeError):
                    logger.error(f"direct_value must be an integer, got {direct_value}")
                    return jsonify({
                        "status": "error",
                        "message": "direct_value must be an integer",
                        "details": {"direct_value": direct_value}
                    }), 400
                
            logger.info(f"component_type: {component_type}, component_name: {component_name}, command_name: {command_name}, value_name: {value_name}, direct_value: {direct_value}")

            result = can_interface.send_command(
                message_type_name='COMMAND',
                component_type_name=component_type,
                component_name=component_name,
                command_name=command_name,
                value_name=value_name,
                direct_value=direct_value
            )

            return jsonify({
                'status': 'success' if result else 'error',
                'message': 'Command sent successfully' if result else 'Command failed',
                'details': {
                    'component_path': component_path,
                    'command_name': command_name,
                    'value_name': value_name,
                    'direct_value': direct_value
                }
            })
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
    
    # Register the blueprint with the app
    app.register_blueprint(command_bp)