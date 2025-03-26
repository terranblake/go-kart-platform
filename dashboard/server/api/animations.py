"""
Animation API for the Go-Kart Platform.

This module defines the API routes for animations, including:
- List all available animations
- Get animation details
- Upload a new animation
- Delete an animation
- Play an animation on the LED strips
"""

import os
import json
import logging
import shutil
from pathlib import Path
from flask import Blueprint, request, jsonify
from werkzeug.utils import secure_filename

logger = logging.getLogger(__name__)

# Define animation directories
SERVER_DIR = Path(__file__).parent.parent
TOOLS_ANIMATION_DIR = SERVER_DIR.parent.parent / "tools" / "animations"
API_ANIMATION_DIR = SERVER_DIR / "api" / "animations"

# Create animation directory if it doesn't exist
if not os.path.exists(API_ANIMATION_DIR):
    os.makedirs(API_ANIMATION_DIR)

def register_animation_routes(app, can_interface):
    """Register animation routes with the Flask app."""
    
    animations_bp = Blueprint('animations', __name__, url_prefix='/api/animations')
    
    @animations_bp.route('/', methods=['GET'])
    def list_animations():
        """List all available animations."""
        animations = []
        
        # First check the API animations directory
        if os.path.exists(API_ANIMATION_DIR):
            for anim_dir in os.listdir(API_ANIMATION_DIR):
                anim_path = os.path.join(API_ANIMATION_DIR, anim_dir)
                if os.path.isdir(anim_path):
                    metadata_file = os.path.join(anim_path, 'metadata.json')
                    if os.path.exists(metadata_file):
                        try:
                            with open(metadata_file, 'r') as f:
                                metadata = json.load(f)
                                animations.append({
                                    'id': anim_dir,
                                    'name': metadata.get('name', 'Unknown'),
                                    'type': metadata.get('type', 'custom'),
                                    'frameCount': metadata.get('frameCount', 0),
                                    'source': 'api'
                                })
                        except Exception as e:
                            logger.error(f"Error loading animation metadata from {metadata_file}: {e}")
        
        # Then check the tools animations directory and its subdirectories
        for root, dirs, files in os.walk(TOOLS_ANIMATION_DIR):
            for file in files:
                if file.endswith('.json') and file != 'animations.json':
                    filepath = os.path.join(root, file)
                    try:
                        with open(filepath, 'r') as f:
                            animation = json.load(f)
                            if isinstance(animation, dict):
                                relative_path = os.path.relpath(filepath, TOOLS_ANIMATION_DIR)
                                animations.append({
                                    'id': relative_path,
                                    'name': animation.get('name', os.path.basename(filepath)),
                                    'type': animation.get('type', 'custom'),
                                    'frameCount': len(animation.get('frames', [])),
                                    'source': 'tools'
                                })
                    except Exception as e:
                        logger.warning(f"Failed to load animation from {filepath}: {e}")
        
        return jsonify(animations)
    
    @animations_bp.route('/<animation_id>', methods=['GET'])
    def get_animation(animation_id):
        """Get animation details."""
        # First check if it's in the API animations directory
        api_animation_path = os.path.join(API_ANIMATION_DIR, animation_id)
        if os.path.exists(api_animation_path):
            metadata_file = os.path.join(api_animation_path, 'metadata.json')
            if os.path.exists(metadata_file):
                try:
                    with open(metadata_file, 'r') as f:
                        metadata = json.load(f)
                    
                    # Load the frames
                    frames = []
                    frames_dir = os.path.join(api_animation_path, 'frames')
                    if os.path.exists(frames_dir):
                        frame_files = sorted([f for f in os.listdir(frames_dir) 
                                             if f.startswith('frame') and f.endswith('.json')])
                        for frame_file in frame_files:
                            with open(os.path.join(frames_dir, frame_file), 'r') as f:
                                frame_data = json.load(f)
                                frames.append(frame_data)
                        
                        metadata['frames'] = frames
                        return jsonify(metadata)
                except Exception as e:
                    logger.error(f"Error loading animation from {api_animation_path}: {e}")
                    return jsonify({'error': f"Failed to load animation: {str(e)}"}), 500
        
        # If not in the API dir, check if it's a direct path to a tool animation
        tools_animation_path = os.path.join(TOOLS_ANIMATION_DIR, animation_id)
        if os.path.exists(tools_animation_path):
            try:
                with open(tools_animation_path, 'r') as f:
                    animation = json.load(f)
                    return jsonify(animation)
            except Exception as e:
                logger.error(f"Error loading animation from {tools_animation_path}: {e}")
                return jsonify({'error': f"Failed to load animation: {str(e)}"}), 500
        
        return jsonify({'error': 'Animation not found'}), 404
    
    @animations_bp.route('/', methods=['POST'])
    def upload_animation():
        """Upload a new animation."""
        try:
            # Check if the post contains a file or JSON
            if 'file' in request.files:
                file = request.files['file']
                if file.filename == '':
                    return jsonify({'error': 'No file selected'}), 400
                
                if file and allowed_file(file.filename):
                    # Create a secure filename
                    filename = secure_filename(file.filename)
                    animation_id = os.path.splitext(filename)[0]
                    
                    # Create a directory for this animation
                    animation_dir = os.path.join(API_ANIMATION_DIR, animation_id)
                    os.makedirs(animation_dir, exist_ok=True)
                    
                    # Save the uploaded file as metadata.json
                    file_path = os.path.join(animation_dir, 'metadata.json')
                    file.save(file_path)
                    
                    # Create frames directory
                    frames_dir = os.path.join(animation_dir, 'frames')
                    os.makedirs(frames_dir, exist_ok=True)
                    
                    # Extract frames from the uploaded file and save them separately
                    with open(file_path, 'r') as f:
                        animation = json.load(f)
                        frames = animation.pop('frames', [])
                        
                        # Save frames
                        for i, frame in enumerate(frames):
                            frame_file = os.path.join(frames_dir, f'frame{i:04d}.json')
                            with open(frame_file, 'w') as frame_f:
                                json.dump(frame, frame_f)
                        
                        # Update metadata with frame count
                        animation['frameCount'] = len(frames)
                        
                        # Save updated metadata
                        with open(file_path, 'w') as meta_f:
                            json.dump(animation, meta_f)
                    
                    return jsonify({
                        'message': 'Animation uploaded successfully',
                        'id': animation_id
                    }), 201
                else:
                    return jsonify({'error': 'Invalid file format'}), 400
            
            elif request.is_json:
                animation = request.get_json()
                animation_id = animation.get('id', str(int(time.time())))
                
                # Create a directory for this animation
                animation_dir = os.path.join(API_ANIMATION_DIR, animation_id)
                os.makedirs(animation_dir, exist_ok=True)
                
                # Extract frames
                frames = animation.pop('frames', [])
                
                # Create metadata
                metadata = {
                    'name': animation.get('name', 'Unnamed Animation'),
                    'type': animation.get('type', 'custom'),
                    'speed': animation.get('speed', 30),
                    'frameCount': len(frames)
                }
                
                # Save metadata
                with open(os.path.join(animation_dir, 'metadata.json'), 'w') as f:
                    json.dump(metadata, f)
                
                # Create frames directory
                frames_dir = os.path.join(animation_dir, 'frames')
                os.makedirs(frames_dir, exist_ok=True)
                
                # Save frames
                for i, frame in enumerate(frames):
                    frame_file = os.path.join(frames_dir, f'frame{i:04d}.json')
                    with open(frame_file, 'w') as f:
                        json.dump(frame, f)
                
                return jsonify({
                    'message': 'Animation created successfully',
                    'id': animation_id
                }), 201
            else:
                return jsonify({'error': 'No animation data provided'}), 400
                
        except Exception as e:
            logger.error(f"Error uploading animation: {e}")
            return jsonify({'error': f"Failed to upload animation: {str(e)}"}), 500
    
    @animations_bp.route('/<animation_id>', methods=['DELETE'])
    def delete_animation(animation_id):
        """Delete an animation."""
        animation_dir = os.path.join(API_ANIMATION_DIR, animation_id)
        if os.path.exists(animation_dir) and os.path.isdir(animation_dir):
            try:
                shutil.rmtree(animation_dir)
                return jsonify({'message': 'Animation deleted successfully'}), 200
            except Exception as e:
                logger.error(f"Error deleting animation {animation_id}: {e}")
                return jsonify({'error': f"Failed to delete animation: {str(e)}"}), 500
        else:
            return jsonify({'error': 'Animation not found'}), 404
    
    @animations_bp.route('/<animation_id>/play', methods=['POST'])
    def play_animation(animation_id):
        """Play an animation on the LED strips."""
        try:
            # Load the animation
            animation = None
            
            # Check API animations directory first
            api_animation_path = os.path.join(API_ANIMATION_DIR, animation_id)
            if os.path.exists(api_animation_path):
                metadata_file = os.path.join(api_animation_path, 'metadata.json')
                if os.path.exists(metadata_file):
                    with open(metadata_file, 'r') as f:
                        animation = json.load(f)
                    
                    # Load the frames
                    frames = []
                    frames_dir = os.path.join(api_animation_path, 'frames')
                    if os.path.exists(frames_dir):
                        frame_files = sorted([f for f in os.listdir(frames_dir) 
                                             if f.startswith('frame') and f.endswith('.json')])
                        for frame_file in frame_files:
                            with open(os.path.join(frames_dir, frame_file), 'r') as f:
                                frame_data = json.load(f)
                                frames.append(frame_data)
                        
                        animation['frames'] = frames
            
            # If not found in API dir, check tools dir
            if animation is None:
                tools_animation_path = os.path.join(TOOLS_ANIMATION_DIR, animation_id)
                if os.path.exists(tools_animation_path):
                    with open(tools_animation_path, 'r') as f:
                        animation = json.load(f)
            
            if animation is None:
                return jsonify({'error': 'Animation not found'}), 404
            
            # Get animation frames
            frames = animation.get('frames', [])
            if not frames:
                return jsonify({'error': 'Animation has no frames'}), 400
            
            # Helper function to prepare binary data for a frame
            def prepare_frame(frame, frame_index):
                leds = frame.get('leds', [])
                packet = bytearray([frame_index, len(leds)])  # Frame index and LED count
                
                for led in leds:
                    # Support both index-based and coordinate-based formats
                    if 'index' in led:
                        # Get x,y based on linear index
                        width = 60  # Default width for 1D LED strip
                        index = led['index']
                        x = index % width
                        y = 0  # Use 0 for y since most animations are 1D
                    else:
                        x = led.get('x', 0)
                        y = led.get('y', 0)
                    
                    # Get RGB color
                    if 'r' in led and 'g' in led and 'b' in led:
                        r = led.get('r', 0)
                        g = led.get('g', 0)
                        b = led.get('b', 0)
                    else:
                        color = led.get('color', '#ff0000')
                        if isinstance(color, str) and color.startswith('#'):
                            color = color.lstrip('#')
                            if len(color) == 6:
                                r = int(color[0:2], 16)
                                g = int(color[2:4], 16)
                                b = int(color[4:6], 16)
                            else:
                                r, g, b = 255, 0, 0
                        else:
                            r, g, b = 255, 0, 0
                    
                    # Add to packet (x, y, r, g, b)
                    packet.extend([x, y, r, g, b])
                
                return bytes(packet)
            
            # Send animation to the lights controller
            logger.info(f"Playing animation: {animation_id} with {len(frames)} frames")
            
            # Send ANIMATION_START command
            result = can_interface.send_command(
                component_type_name="LIGHTS",
                component_name="ALL",
                command_name="ANIMATION_START",
                direct_value=1
            )
            
            if not result:
                logger.error("Failed to send animation start command")
                return jsonify({'error': 'Failed to send animation start command'}), 500
            
            # Send each frame
            for i, frame in enumerate(frames):
                # Prepare and send the binary frame data
                binary_data = prepare_frame(frame, i)
                result = can_interface.send_binary_data(
                    component_type_name="LIGHTS",
                    component_name="ALL",
                    command_name="ANIMATION_FRAME",
                    value_type="BINARY",
                    data=binary_data
                )
                
                if not result:
                    logger.error(f"Failed to send frame {i}")
                    return jsonify({'error': f'Failed to send frame {i}'}), 500
            
            # Send ANIMATION_END command to start playback
            result = can_interface.send_command(
                component_type_name="LIGHTS",
                component_name="ALL",
                command_name="ANIMATION_END",
                direct_value=len(frames)
            )
            
            if not result:
                logger.error("Failed to send animation end command")
                return jsonify({'error': 'Failed to send animation end command'}), 500
            
            return jsonify({'message': 'Animation playback started successfully'}), 200
            
        except Exception as e:
            logger.error(f"Error playing animation: {e}")
            return jsonify({'error': f"Failed to play animation: {str(e)}"}), 500
    
    @animations_bp.route('/stop', methods=['POST'])
    def stop_animation():
        """Stop the currently playing animation."""
        try:
            # Send ANIMATION_STOP command
            result = can_interface.send_command(
                component_type_name="LIGHTS",
                component_name="ALL",
                command_name="ANIMATION_STOP",
                direct_value=0
            )
            
            if not result:
                logger.error("Failed to send animation stop command")
                return jsonify({'error': 'Failed to send animation stop command'}), 500
            
            return jsonify({'message': 'Animation playback stopped successfully'}), 200
            
        except Exception as e:
            logger.error(f"Error stopping animation: {e}")
            return jsonify({'error': f"Failed to stop animation: {str(e)}"}), 500
    
    # Register the blueprint with the app
    app.register_blueprint(animations_bp)

def allowed_file(filename):
    """Check if the file has an allowed extension."""
    return '.' in filename and filename.rsplit('.', 1)[1].lower() in {'json'}

import time  # Add missing import for time.time() 