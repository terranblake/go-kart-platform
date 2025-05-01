from flask import jsonify, request, Blueprint
import logging
from shared.lib.python.can.interface import CANInterfaceWrapper

logger = logging.getLogger(__name__)

def register_direct_command_routes(app, can_interface):
    # Create a blueprint for direct command routes
    direct_command_bp = Blueprint('direct_command', __name__, url_prefix='/api/direct_command')
    
    @direct_command_bp.route("", methods=["POST"])
    def send_direct_command():
        try:
            command_data = request.json
            logger.info(f"Received direct command request: {command_data}")
            
            msg_type = int(command_data.get("msg_type", 0))
            comp_type = int(command_data.get("comp_type", 0))
            comp_id = int(command_data.get("comp_id", 0))
            cmd_id = int(command_data.get("cmd_id", 0))
            val_type = int(command_data.get("val_type", 0))
            value = int(command_data.get("value", 0))
            
            logger.info(f"Sending direct CAN command: msg_type={msg_type}, comp_type={comp_type}, comp_id={comp_id}, cmd_id={cmd_id}, val_type={val_type}, value={value}")
            
            result = can_interface.send_message(
                msg_type,
                comp_type,
                comp_id,
                cmd_id,
                val_type,
                value
            )

            return jsonify({
                "status": "success" if result else "error",
                "message": "Direct command sent successfully" if result else "Direct command failed",
                "details": {
                    "msg_type": msg_type,
                    "comp_type": comp_type, 
                    "comp_id": comp_id,
                    "cmd_id": cmd_id,
                    "val_type": val_type,
                    "value": value
                }
            })
        except Exception as e:
            logger.error(f"Error sending direct command: {e}")
            return jsonify({
                "status": "error",
                "message": f"Error sending direct command: {str(e)}"
            }), 400
    
    # Register the blueprint with the app
    app.register_blueprint(direct_command_bp)