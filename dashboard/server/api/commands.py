# this should be for sending specific commands to the go-kart
from flask import jsonify, request, Blueprint
import logging

logger = logging.getLogger(__name__)

def register_command_routes(app, command_generator):
    """Register API routes for commanding the go-kart"""
    
    @app.route('/api/command', methods=['POST'])
    def send_command():
        """Send a command to the go-kart"""
        try:
            command_data = request.json
            
            if 'speed' in command_data:
                command_generator.send_speed_command(command_data['speed'])
                
            if 'steering' in command_data:
                command_generator.send_steering_command(command_data['steering'])
                
            if 'brake' in command_data:
                command_generator.send_brake_command(command_data['brake'])
                
            if 'emergency_stop' in command_data and command_data['emergency_stop']:
                command_generator.send_emergency_stop()
                
            return jsonify({'status': 'success', 'message': 'Command sent successfully'})
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
    
    @app.route('/api/lights/<mode>', methods=['POST'])
    def control_lights(mode):
        """Control the go-kart lights"""
        try:
            if mode not in ['off', 'low', 'high', 'hazard', 'left', 'right', 'brake']:
                return jsonify({'status': 'error', 'message': 'Invalid light mode'}), 400
                
            # Handle different light modes
            if mode in ['off', 'low', 'high', 'hazard']:
                command_generator.send_light_mode_by_name(mode)
            elif mode in ['left', 'right']:
                command_generator.send_signal_by_name(mode)
            elif mode == 'brake':
                command_generator.send_brake_light(True)
                
            return jsonify({'status': 'success', 'message': f'Lights set to {mode}'})
        except Exception as e:
            logger.error(f"Error controlling lights: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
    
    @app.route('/api/camera/status', methods=['GET'])
    def get_camera_status():
        """Get camera status"""
        return jsonify({'status': 'active'})
    
    @app.route('/api/settings', methods=['GET'])
    def get_settings():
        """Get go-kart settings"""
        return jsonify({
            'max_speed': 50,
            'telemetry_rate': 10
        })
    
    @app.route('/api/settings', methods=['POST'])
    def update_settings():
        """Update go-kart settings"""
        settings = request.json
        return jsonify({'status': 'success', 'settings': settings})
