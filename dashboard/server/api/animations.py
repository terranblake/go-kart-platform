import os
import json
import logging
from flask import Blueprint, jsonify, request
from pathlib import Path

logger = logging.getLogger(__name__)

def register_animation_routes(app, can_interface):
    """Register animation routes with the app."""
    animation_routes = Blueprint('animation', __name__, url_prefix='/api/animation')
    
    # Calculate animation directory paths
    server_dir = Path(os.path.dirname(os.path.abspath(__file__))).parent
    tools_animations_dir = os.path.join(server_dir.parent.parent, 'tools', 'animations')
    api_animations_dir = os.path.join(server_dir, 'api', 'animations')
    
    # Ensure animations directory exists
    os.makedirs(api_animations_dir, exist_ok=True)
    
    @animation_routes.route('/list', methods=['GET'])
    def list_animations():
        """List all available animations."""
        return _list_animations()
    
    @animation_routes.route('/play', methods=['POST'])
    def play_animation():
        """Play an animation."""
        data = request.json
        animation_id = data.get('id')
        speed = data.get('speed', 100)  # Default to 100ms per frame
        
        if not animation_id:
            return jsonify({'status': 'error', 'message': 'Animation ID is required'})
        
        # Load the animation
        animation = load_animation(animation_id, tools_animations_dir, api_animations_dir)
        if not animation:
            return jsonify({'status': 'error', 'message': f'Animation {animation_id} not found'})
        
        # Process the animation (send commands to controllers)
        result = _process_animation(can_interface, animation_id, speed, animation)
        
        if result:
            return jsonify({'status': 'success', 'message': f'Playing animation {animation_id}'})
        else:
            return jsonify({'status': 'error', 'message': f'Failed to play animation {animation_id}'})
    
    @animation_routes.route('/stop', methods=['POST'])
    def stop_animation():
        """Stop the currently playing animation."""
        result = _stop_animation(can_interface)
        
        if result:
            return jsonify({'status': 'success', 'message': 'Animation stopped'})
        else:
            return jsonify({'status': 'error', 'message': 'Failed to stop animation'})
    
    @animation_routes.route('/upload', methods=['POST'])
    def upload_animation():
        """Upload a new animation."""
        try:
            data = request.json
            animation_data = data.get('animation')
            
            if not animation_data:
                return jsonify({'status': 'error', 'message': 'Animation data is required'})
                
            # Generate ID if not provided
            if 'id' not in animation_data:
                animation_data['id'] = f"custom_{len(os.listdir(api_animations_dir)) + 1}"
                
            # Create directory for animation
            animation_dir = os.path.join(api_animations_dir, animation_data['id'])
            os.makedirs(animation_dir, exist_ok=True)
            
            # Extract frames to separate files
            frames = animation_data.pop('frames', [])
            
            # Save metadata
            with open(os.path.join(animation_dir, 'metadata.json'), 'w') as f:
                json.dump(animation_data, f, indent=2)
                
            # Save frames
            frames_dir = os.path.join(animation_dir, 'frames')
            os.makedirs(frames_dir, exist_ok=True)
            
            for i, frame in enumerate(frames):
                with open(os.path.join(frames_dir, f'frame{i:03d}.json'), 'w') as f:
                    json.dump(frame, f, indent=2)
                    
            return jsonify({
                'status': 'success', 
                'message': 'Animation uploaded successfully',
                'id': animation_data['id']
            })
                
        except Exception as e:
            logger.error(f"Error uploading animation: {e}")
            return jsonify({'status': 'error', 'message': str(e)})
    
    # Register the blueprint
    app.register_blueprint(animation_routes)

def _list_animations():
    """List all available animations (internal helper)."""
    try:
        animations = []
        
        # Get animations from the tools directory
        server_dir = Path(os.path.dirname(os.path.abspath(__file__))).parent
        tools_animations_dir = os.path.join(server_dir.parent.parent, 'tools', 'animations')
        
        # Process different animation directories
        # First, check the 'car' subdirectory for car animations
        car_dir = os.path.join(tools_animations_dir, 'car')
        if os.path.exists(car_dir):
            car_animations_file = os.path.join(car_dir, 'animations.json')
            if os.path.exists(car_animations_file):
                try:
                    with open(car_animations_file, 'r') as f:
                        car_anims = json.load(f)
                        for anim in car_anims:
                            # Add source information
                            anim['source'] = 'car'
                            animations.append(anim)
                except Exception as e:
                    logger.warning(f"Failed to load car animations: {e}")
        
        # Next, recursively scan animation directories
        for anim_dir in os.listdir(tools_animations_dir):
            dir_path = os.path.join(tools_animations_dir, anim_dir)
            if os.path.isdir(dir_path) and anim_dir != 'car' and not anim_dir.startswith('.'):
                # Look for top-level animations.json files
                json_files = [f for f in os.listdir(dir_path) if f.endswith('.json')]
                for json_file in json_files:
                    try:
                        with open(os.path.join(dir_path, json_file), 'r') as f:
                            anim_data = json.load(f)
                            if isinstance(anim_data, list):
                                for anim in anim_data:
                                    if isinstance(anim, dict) and 'name' in anim:
                                        anim['source'] = anim_dir
                                        animations.append(anim)
                            elif isinstance(anim_data, dict) and 'name' in anim_data:
                                anim_data['source'] = anim_dir
                                animations.append(anim_data)
                    except Exception as e:
                        logger.warning(f"Failed to load animation from {json_file}: {e}")
        
        # Add custom animations from the API directory
        api_animations_dir = os.path.join(server_dir, 'api', 'animations')
        if os.path.exists(api_animations_dir):
            for anim_id in os.listdir(api_animations_dir):
                anim_dir = os.path.join(api_animations_dir, anim_id)
                if os.path.isdir(anim_dir):
                    metadata_file = os.path.join(anim_dir, 'metadata.json')
                    if os.path.exists(metadata_file):
                        try:
                            with open(metadata_file, 'r') as f:
                                metadata = json.load(f)
                                metadata['source'] = 'custom'
                                metadata['id'] = anim_id
                                animations.append(metadata)
                        except Exception as e:
                            logger.warning(f"Failed to load custom animation {anim_id}: {e}")
        
        return jsonify({'status': 'success', 'animations': animations})
    
    except Exception as e:
        logger.error(f"Error listing animations: {e}")
        return jsonify({'status': 'error', 'message': str(e)})

def load_animation(animation_id, tools_animations_dir, api_animations_dir):
    """Load an animation by ID."""
    # First check custom animations
    api_animation_path = os.path.join(api_animations_dir, animation_id)
    if os.path.exists(api_animation_path):
        metadata_file = os.path.join(api_animation_path, 'metadata.json')
        if os.path.exists(metadata_file):
            try:
                with open(metadata_file, 'r') as f:
                    metadata = json.load(f)
                
                # Load the frames
                frames_dir = os.path.join(api_animation_path, 'frames')
                if os.path.exists(frames_dir):
                    frames = []
                    frame_files = sorted([f for f in os.listdir(frames_dir) if f.startswith('frame') and f.endswith('.json')])
                    for frame_file in frame_files:
                        with open(os.path.join(frames_dir, frame_file), 'r') as f:
                            frame_data = json.load(f)
                            frames.append(frame_data)
                    
                    metadata['frames'] = frames
                    return metadata
            except Exception as e:
                logger.warning(f"Failed to load animation from {api_animation_path}: {e}")
    
    # Check for car animations
    car_animations_path = os.path.join(tools_animations_dir, 'car')
    if os.path.exists(car_animations_path):
        car_animations_file = os.path.join(car_animations_path, 'animations.json')
        if os.path.exists(car_animations_file):
            with open(car_animations_file, 'r') as f:
                car_animations = json.load(f)
                for anim in car_animations:
                    if anim.get('id') == animation_id:
                        return anim
    
    # If not found, look in other directories
    for root, dirs, files in os.walk(tools_animations_dir):
        for file in files:
            if file.endswith('.json'):
                filepath = os.path.join(root, file)
                try:
                    with open(filepath, 'r') as f:
                        animation = json.load(f)
                        if isinstance(animation, dict) and animation.get('id') == animation_id:
                            return animation
                        elif isinstance(animation, list):
                            for anim in animation:
                                if anim.get('id') == animation_id:
                                    return anim
                except Exception as e:
                    logger.warning(f"Failed to load animation from {filepath}: {e}")
    
    logger.error(f"Animation {animation_id} not found")
    return None

def prepare_animation_frame(frame, frame_index):
    """Convert an animation frame to binary data for transmission."""
    # Handle different frame formats
    leds = []
    
    if 'leds' in frame:
        leds = frame.get('leds', [])
    elif 'frameIndex' in frame:
        leds = frame.get('leds', [])
    
    # Log frame data for debugging
    logger.debug(f"Preparing frame {frame_index} with {len(leds)} LEDs")
    
    # Create binary packet for this frame
    packet = bytearray([frame_index, len(leds)])  # Frame index and LED count
    
    # Add LED data
    for led in leds:
        try:
            # Support both index-based and coordinate-based formats
            if 'index' in led:
                # Convert linear index to x,y
                width = 60  # Default width
                index = int(led['index'])
                x = index % width
                y = index // width
            else:
                x = int(led.get('x', 0))
                y = int(led.get('y', 0))
            
            # Get RGB color
            if 'r' in led and 'g' in led and 'b' in led:
                # Direct RGB values
                r = int(led.get('r', 0))
                g = int(led.get('g', 0))
                b = int(led.get('b', 0))
            else:
                color = led.get('color', '#ff0000')
                if isinstance(color, dict) and 'r' in color:
                    # Handle object format: {r: 255, g: 0, b: 0}
                    r = int(color.get('r', 0))
                    g = int(color.get('g', 0))
                    b = int(color.get('b', 0))
                elif isinstance(color, str) and color.startswith('#'):
                    # Parse hex color
                    color = color.lstrip('#')
                    if len(color) == 6:
                        r = int(color[0:2], 16)
                        g = int(color[2:4], 16)
                        b = int(color[4:6], 16)
                    else:
                        # Default red for invalid colors
                        r, g, b = 255, 0, 0
                else:
                    # Default red
                    r, g, b = 255, 0, 0
            
            # Clamp RGB values to 0-255 range
            r = max(0, min(255, r))
            g = max(0, min(255, g))
            b = max(0, min(255, b))
            
            # Add to packet
            packet.extend([x, y, r, g, b])
        except Exception as e:
            logger.warning(f"Error processing LED data: {e}, LED data: {led}")
            # Skip this LED and continue with the next one
    
    return bytes(packet)

def _process_animation(can_interface, animation_id, speed, animation):
    """Process an animation and send it to the CAN interface."""
    try:
        frames = animation.get('frames', [])
        if not frames:
            logger.error("Animation has no frames")
            return False
        
        # First, send a start command to initialize animation mode
        logger.info("Sending animation start command")
        result = can_interface.send_command(
            component_type='LIGHTS',
            component_name='ALL',
            command_name='ANIMATION_START',
            direct_value=1
        )
        
        if not result:
            logger.error("Failed to send animation start command")
            return False
        
        # Small delay to give the controller time to prepare
        import time
        time.sleep(0.1)
        
        # Send each frame
        for i, frame in enumerate(frames):
            logger.info(f"Sending frame {i+1} of {len(frames)}")
            
            # Prepare the binary data for this frame
            binary_data = prepare_animation_frame(frame, i)
            
            # Send the binary frame data
            result = can_interface.send_binary_data(
                component_type_name="LIGHTS",
                component_name="ALL",
                command_name="ANIMATION_FRAME",
                value_type="UINT8",
                data=binary_data
            )
            
            if not result:
                logger.error(f"Failed to send frame {i}")
                return False
            
            # Small delay to prevent flooding the bus
            time.sleep(0.05)
        
        # Finally, send end command to start playback
        logger.info("Sending animation end command")
        result = can_interface.send_command(
            component_type='LIGHTS',
            component_name='ALL',
            command_name='ANIMATION_END',
            direct_value=len(frames)
        )
        
        if not result:
            logger.error("Failed to send animation end command")
            return False
        
        return True
    except Exception as e:
        logger.error(f"Error processing animation: {e}")
        return False

def _stop_animation(can_interface):
    """Stop the currently playing animation."""
    try:
        logger.info("Stopping animation playback")
        result = can_interface.send_command(
            component_type='LIGHTS',
            component_name='ALL',
            command_name='ANIMATION_STOP',
            direct_value=0
        )
        
        if not result:
            logger.error("Failed to send animation stop command")
            return False
        
        return True
    except Exception as e:
        logger.error(f"Error stopping animation: {e}")
        return False 