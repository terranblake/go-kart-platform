"""
API routes for viewing and exploring the protocol structure
"""

from flask import jsonify, render_template, Blueprint
import logging
from shared.lib.python.can.protocol_registry import ProtocolRegistry

logger = logging.getLogger(__name__)

def register_protocol_routes(app, protocol_registry: ProtocolRegistry):
    # Create blueprints for protocol routes
    protocol_view_bp = Blueprint('protocol_view', __name__, url_prefix='/protocol')
    protocol_api_bp = Blueprint('protocol_api', __name__, url_prefix='/api/protocol')

    @protocol_view_bp.route('', methods=['GET'])
    def protocol():
        """Render the protocol documentation page"""
        try:
            return render_template('protocol.html')
        except Exception as e:
            logger.error(f"Error retrieving protocol structure: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    @protocol_api_bp.route('', methods=['GET'])
    def get_protocol_structure_api():
        """Get the complete protocol structure"""
        try:
            return jsonify(protocol_registry.registry)
        except Exception as e:
            logger.error(f"Error retrieving protocol structure: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    @protocol_api_bp.route('/components/<component_name>', methods=['GET'])
    def get_component_api(component_name):
        """Get protocol information for a specific component"""
        try:
            component = protocol_registry.get_component_by_name(component_name)
            if component:
                return jsonify(component)
            return jsonify({'status': 'error', 'message': f"Component '{component_name}' not found"}), 404
        except Exception as e:
            logger.error(f"Error retrieving component '{component_name}': {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500

    @protocol_api_bp.route('/components/<component_name>/commands/<command_name>', methods=['GET'])
    def get_command_api(component_name, command_name):
        """Get protocol information for a specific command"""
        try:
            command = protocol_registry.get_command_by_name(component_name, command_name)
            if command:
                return jsonify(command)
            return jsonify({'status': 'error', 'message': f"Command '{command_name}' not found for component '{component_name}'"}), 404
        except Exception as e:
            logger.error(f"Error retrieving command '{component_name}.{command_name}': {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
    
    # Register the blueprints with the app
    app.register_blueprint(protocol_view_bp)
    app.register_blueprint(protocol_api_bp)