"""
API routes for animations on the go-kart lights
"""

import os
import json
import logging
from flask import jsonify, request, Blueprint
from lib.can.interface import CANInterfaceWrapper

logger = logging.getLogger(__name__)

# Path to animation files
ANIMATIONS_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), 
                              'tools', 'animations')

def register_animation_routes(app, can_interface: CANInterfaceWrapper):
    """Register API routes for animations"""
    
    # Create a blueprint for animation routes
    animation_bp = Blueprint('animation', __name__, url_prefix='/api/animation')
    
    @animation_bp.route('/list', methods=['GET'])
    def list_animations():
        """List available animations"""
        animations = []
        
        # Look for JSON animation files in the animations directory
        try:
            # Load car animations first (better designed for the actual car)
            car_animations_path = os.path.join(ANIMATIONS_DIR, 'all_car_lighting.json')
            if os.path.exists(car_animations_path):
                with open(car_animations_path, 'r') as f:
                    car_data = json.load(f)
                    for anim in car_data:
                        animations.append({
                            'name': anim.get('name', 'Unnamed Animation'),
                            'id': anim.get('id', f"car_{len(animations)}"),
                            'type': anim.get('type', 'custom'),
                            'source': 'car'
                        })
            
            # Add individual animations from subdirectories
            for dirname in os.listdir(ANIMATIONS_DIR):
                dir_path = os.path.join(ANIMATIONS_DIR, dirname)
                if os.path.isdir(dir_path) and not dirname.startswith('.'):
                    for filename in os.listdir(dir_path):
                        if filename.endswith('.json'):
                            file_path = os.path.join(dir_path, filename)
                            try:
                                with open(file_path, 'r') as f:
                                    data = json.load(f)
                                    # Handle both single animation and collections
                                    if isinstance(data, list):
                                        for idx, anim in enumerate(data):
                                            animations.append({
                                                'name': anim.get('name', f"{dirname}_{idx}"),
                                                'id': f"{dirname}_{filename}_{idx}",
                                                'type': anim.get('type', 'custom'),
                                                'source': dirname
                                            })
                                    else:
                                        animations.append({
                                            'name': data.get('name', dirname),
                                            'id': f"{dirname}_{filename}",
                                            'type': data.get('type', 'custom'),
                                            'source': dirname
                                        })
                            except Exception as e:
                                logger.error(f"Error loading animation file {file_path}: {e}")
        except Exception as e:
            logger.error(f"Error listing animations: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
        
        return jsonify({
            'status': 'success',
            'animations': animations
        })
    
    @animation_bp.route('/play', methods=['POST'])
    def play_animation():
        """Send animation data to the go-kart lights"""
        try:
            animation_data = request.json
            animation_id = animation_data.get('id')
            speed = animation_data.get('speed', 100)
            
            if not animation_id:
                return jsonify({'status': 'error', 'message': 'Animation ID is required'}), 400
            
            # Find and load the animation
            animation = None
            
            # Handle car animations
            if animation_id.startswith('car_'):
                car_animations_path = os.path.join(ANIMATIONS_DIR, 'all_car_lighting.json')
                if os.path.exists(car_animations_path):
                    with open(car_animations_path, 'r') as f:
                        car_data = json.load(f)
                        for anim in car_data:
                            if anim.get('id') == animation_id:
                                animation = anim
                                break
            else:
                # Parse the ID to find the file
                parts = animation_id.split('_')
                if len(parts) >= 2:
                    dirname = parts[0]
                    filename = parts[1]
                    idx = int(parts[2]) if len(parts) > 2 and parts[2].isdigit() else 0
                    
                    file_path = os.path.join(ANIMATIONS_DIR, dirname, f"{filename}")
                    if os.path.exists(file_path):
                        with open(file_path, 'r') as f:
                            data = json.load(f)
                            if isinstance(data, list) and idx < len(data):
                                animation = data[idx]
                            else:
                                animation = data
            
            if not animation:
                return jsonify({'status': 'error', 'message': 'Animation not found'}), 404
            
            # Modify speed if requested
            if speed != 100:
                animation['speed'] = speed
            
            logger.info(f"Sending animation {animation.get('name')} to lights")
            
            # Send a special command to indicate an animation is coming
            can_interface.send_command(
                component_type='LIGHTS',
                component_name='ALL',
                command_name='ANIMATION_START',
                direct_value=1
            )
            
            # Send the animation data as a series of frames
            frames = animation.get('frames', [])
            for i, frame in enumerate(frames):
                # Encode frame data as a direct value
                # This is simplified - in reality, you'd need to use a more
                # complex protocol to send the frame data over CAN
                can_interface.send_command(
                    component_type='LIGHTS',
                    component_name='ALL',
                    command_name='ANIMATION_FRAME',
                    command_data={'frame_index': i, 'frame_data': frame}
                )
            
            # Send end command to start playback
            can_interface.send_command(
                component_type='LIGHTS',
                component_name='ALL',
                command_name='ANIMATION_END',
                direct_value=len(frames)
            )
            
            return jsonify({
                'status': 'success',
                'message': f"Playing animation {animation.get('name')}",
                'frames_sent': len(frames)
            })
            
        except Exception as e:
            logger.error(f"Error playing animation: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
    
    @animation_bp.route('/stop', methods=['POST'])
    def stop_animation():
        """Stop any currently playing animation"""
        try:
            can_interface.send_command(
                component_type='LIGHTS',
                component_name='ALL',
                command_name='ANIMATION_STOP',
                direct_value=0
            )
            
            return jsonify({
                'status': 'success',
                'message': "Animation stopped"
            })
            
        except Exception as e:
            logger.error(f"Error stopping animation: {e}")
            return jsonify({'status': 'error', 'message': str(e)}), 500
    
    # Register the blueprint
    app.register_blueprint(animation_bp) 