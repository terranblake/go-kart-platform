"""
API routes for animations on the go-kart lights
"""

import os
import json
import logging
import time
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
            
            # Process the animation frames
            frames = animation.get('frames', [])
            
            # For each frame, construct a binary packet and send it
            for i, frame in enumerate(frames):
                # Extract LED data from the frame
                leds = frame.get('leds', [])
                
                # Create binary packet for this frame
                # Format: [frame_index(1), num_leds(1), led_data...]
                # Each led_data is: [x(1), y(1), r(1), g(1), b(1)]
                
                packet = bytearray()
                packet.append(i)  # Frame index
                packet.append(len(leds))  # Number of LEDs
                
                # Add LED data
                for led in leds:
                    # Support both index-based and coordinate-based formats
                    if 'index' in led:
                        # Convert linear index to x,y
                        dimensions = animation.get('dimensions', {'length': 60, 'height': 1})
                        width = dimensions.get('length', 60)
                        index = led['index']
                        
                        # For a 1D strip, y is always 0
                        x = index % width
                        y = index // width
                    else:
                        x = led.get('x', 0)
                        y = led.get('y', 0)
                    
                    # Get RGB color
                    color = led.get('color', '#ff0000')
                    if isinstance(color, str) and color.startswith('#'):
                        # Parse hex color
                        color = color.lstrip('#')
                        if len(color) == 6:
                            r = int(color[0:2], 16)
                            g = int(color[2:4], 16)
                            b = int(color[4:6], 16)
                        else:
                            # Default red for invalid colors
                            r, g, b = 255, 0, 0
                    elif isinstance(color, dict):
                        # Dict with r,g,b keys
                        r = color.get('r', 0)
                        g = color.get('g', 0)
                        b = color.get('b', 0)
                    else:
                        # Default red
                        r, g, b = 255, 0, 0
                    
                    # Add to packet
                    packet.append(x)
                    packet.append(y)
                    packet.append(r)
                    packet.append(g)
                    packet.append(b)
                
                # Send the binary frame data
                can_interface.send_binary_data(
                    component_type='LIGHTS',
                    component_name='ALL',
                    command_name='ANIMATION_FRAME',
                    value_type='UINT8',
                    data=bytes(packet)
                )
                
                # Short delay to avoid overwhelming the CAN bus
                time.sleep(0.01)
            
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