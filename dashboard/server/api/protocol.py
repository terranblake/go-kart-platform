"""
API routes for viewing and exploring the protocol structure
"""

from flask import jsonify, Blueprint
import logging

from lib.can.protocol import load_protocol_definitions, view_protocol_structure

logger = logging.getLogger(__name__)

# Create Blueprint for protocol routes
protocol_bp = Blueprint('protocol', __name__)

@protocol_bp.route('/api/protocol', methods=['GET'])
def get_protocol_structure():
    """Get the complete protocol structure"""
    try:
        # Load and return the protocol structure
        protocol = load_protocol_definitions()
        structure = view_protocol_structure(protocol)
        return jsonify(structure)
    except Exception as e:
        logger.error(f"Error retrieving protocol structure: {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@protocol_bp.route('/api/protocol/<component_type>', methods=['GET'])
def get_component_type(component_type):
    """Get protocol information for a specific component type"""
    try:
        protocol = load_protocol_definitions()
        
        # Get components for this type
        components = protocol.get('components', {}).get(component_type, {})
        if not components:
            return jsonify({'status': 'error', 'message': f"Component type '{component_type}' not found"}), 404
            
        # Get commands for this type
        commands = protocol.get('commands', {}).get(component_type, {})
        
        return jsonify({
            'components': components,
            'commands': commands
        })
    except Exception as e:
        logger.error(f"Error retrieving component type '{component_type}': {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@protocol_bp.route('/api/protocol/<component_type>/<component_name>', methods=['GET'])
def get_component(component_type, component_name):
    """Get protocol information for a specific component"""
    try:
        protocol = load_protocol_definitions()
        
        # Get component ID
        component_id = protocol.get('components', {}).get(component_type, {}).get(component_name)
        if component_id is None:
            return jsonify({'status': 'error', 'message': f"Component '{component_type}.{component_name}' not found"}), 404
            
        # Get all commands that can apply to this component
        commands = protocol.get('commands', {}).get(component_type, {})
        
        return jsonify({
            'component_id': component_id,
            'commands': commands
        })
    except Exception as e:
        logger.error(f"Error retrieving component '{component_type}.{component_name}': {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

@protocol_bp.route('/api/protocol/<component_type>/<component_name>/<command_name>', methods=['GET'])
def get_command(component_type, component_name, command_name):
    """Get protocol information for a specific command"""
    try:
        protocol = load_protocol_definitions()
        
        # Get component ID
        component_id = protocol.get('components', {}).get(component_type, {}).get(component_name)
        if component_id is None:
            return jsonify({'status': 'error', 'message': f"Component '{component_type}.{component_name}' not found"}), 404
            
        # Get command
        command = protocol.get('commands', {}).get(component_type, {}).get(command_name)
        if command is None:
            return jsonify({'status': 'error', 'message': f"Command '{component_type}.{command_name}' not found"}), 404
            
        return jsonify({
            'component_id': component_id,
            'command_id': command.get('id'),
            'values': command.get('values', {})
        })
    except Exception as e:
        logger.error(f"Error retrieving command '{component_type}.{component_name}.{command_name}': {e}")
        return jsonify({'status': 'error', 'message': str(e)}), 500

def register_protocol_routes(app):
    """Register protocol routes with the Flask app"""
    app.register_blueprint(protocol_bp) 