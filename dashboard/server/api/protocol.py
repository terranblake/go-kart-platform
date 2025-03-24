"""
API routes for viewing and exploring the protocol structure
"""

from flask import jsonify, Blueprint, request
import logging

# Fix the import to use the correct path
from lib.can.protocol_registry import ProtocolRegistry

logger = logging.getLogger(__name__)

# Create Blueprint for protocol routes
protocol_bp = Blueprint('protocol', __name__)

@protocol_bp.route('/api/protocol', methods=['GET'])
def get_protocol_structure_api():
    """Get the complete protocol structure"""
    try:
        # Create a protocol registry instance
        protocol_registry = ProtocolRegistry()
        protocol = protocol_registry.registry
        
        logger.info(f"Protocol structure loaded: {list(protocol.keys())}")
        if 'components' in protocol:
            logger.info(f"Components: {list(protocol['components'].keys())}")
        return jsonify(protocol)
    except Exception as e:
        logger.error(f"Error retrieving protocol structure: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@protocol_bp.route('/api/protocol/components/<component_name>', methods=['GET'])
def get_component_api(component_name):
    """Get protocol information for a specific component"""
    try:
        # Create a protocol registry instance
        protocol_registry = ProtocolRegistry()
        protocol = protocol_registry.registry
        
        if 'components' in protocol and component_name.lower() in protocol['components']:
            return jsonify(protocol['components'][component_name.lower()])
        return jsonify({'status': 'error', 'message': f"Component '{component_name}' not found"}), 404
    except Exception as e:
        logger.error(f"Error retrieving component '{component_name}': {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@protocol_bp.route('/api/protocol/components/<component_name>/commands/<command_name>', methods=['GET'])
def get_command_api(component_name, command_name):
    """Get protocol information for a specific command"""
    try:
        # Create a protocol registry instance
        protocol_registry = ProtocolRegistry()
        protocol = protocol_registry.registry
        
        if ('commands' in protocol and 
            component_name.lower() in protocol['commands'] and 
            command_name.upper() in protocol['commands'][component_name.lower()]):
            command_id = protocol['commands'][component_name.lower()][command_name.upper()]
            return jsonify({
                'id': command_id,
                'name': command_name.upper()
            })
        return jsonify({'status': 'error', 'message': f"Command '{command_name}' not found for component '{component_name}'"}), 404
    except Exception as e:
        logger.error(f"Error retrieving command '{component_name}.{command_name}': {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@protocol_bp.route('/api/send_command', methods=['POST'])
def send_can_command():
    """Send a command to the CAN bus"""
    try:
        if not request.is_json:
            return jsonify({"error": "Request must be JSON"}), 400
        
        data = request.json
        component = data.get('component')
        command = data.get('command')
        value = data.get('value')
        component_type = data.get('component_type')  # Optional, for backward compatibility
        
        logger.info(f"Received command request: component={component}, command={command}, value={value}, component_type={component_type}")
        
        if not all([component, command, value is not None]):
            return jsonify({"error": "Missing required parameters: component, command, value"}), 400
        
        # Get CAN interface and send the command
        try:
            from lib.can.interface import can_interface
            logger.info(f"Sending command to CAN interface: component={component}, command={command}, value={value}")
            
            # Send the command using the interface
            result = can_interface.send_command(component, command, value)
            logger.info(f"Command sent successfully: {result}")
            
            return jsonify({"message": "Command sent successfully", "result": result})
        except ImportError as e:
            logger.error(f"Error importing CAN interface: {e}")
            return jsonify({"error": f"CAN interface not available: {str(e)}"}), 500
            
    except Exception as e:
        logger.error(f"Error sending command: {e}")
        return jsonify({"error": str(e)}), 500

def register_protocol_routes(app):
    """Register protocol routes with the Flask app"""
    app.register_blueprint(protocol_bp) 