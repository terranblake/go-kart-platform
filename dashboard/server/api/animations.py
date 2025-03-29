"""
Animation API endpoints.

This module provides REST endpoints and WebSocket handlers for animation management,
playback control, and real-time status updates.
"""

import json
import logging
import os
import threading
import time
from typing import Dict, Any, List, Optional, Tuple
import uuid

from flask import Blueprint, request, jsonify, current_app, abort
from flask_socketio import emit

from lib.animations.manager import AnimationManager
from lib.animations.protocol import AnimationProtocol, CONFIG_FPS, CONFIG_BRIGHTNESS, CONFIG_NUM_LEDS, MODE_ANIMATION

# Configure logging
logger = logging.getLogger(__name__)

# Animation status tracking for WebSocket
animation_status = {
    'active_animation': None,
    'current_frame': 0,
    'total_frames': 0,
    'fps': 30,
    'is_playing': False,
    'loop': True
}

# Global references (will be set by register_animation_routes)
animation_manager = None
socketio = None

# Create blueprint for animation routes
animations_bp = Blueprint('animations', __name__, url_prefix='/api/animations')

# WebSocket event handlers
def handle_animation_playback_started(animation_id: str) -> None:
    """
    Handle animation playback started event.
    
    Args:
        animation_id: ID of the animation that started playing
    """
    logger.info(f"Animation playback started: {animation_id}")
    
    # Get animation data
    animation_data = animation_manager.get_animation(animation_id)
    if not animation_data:
        return
        
    # Update status
    animation_status['active_animation'] = animation_id
    animation_status['current_frame'] = 0
    animation_status['total_frames'] = animation_data.get('frame_count', 0)
    animation_status['is_playing'] = True
    
    # Broadcast update
    if socketio:
        socketio.emit('animation_status', animation_status, namespace='/animations')

def handle_animation_playback_stopped(animation_id: str) -> None:
    """
    Handle animation playback stopped event.
    
    Args:
        animation_id: ID of the animation that stopped playing
    """
    logger.info(f"Animation playback stopped: {animation_id}")
    
    # Update status
    animation_status['is_playing'] = False
    
    # Broadcast update
    if socketio:
        socketio.emit('animation_status', animation_status, namespace='/animations')

def handle_animation_frame_played(animation_id: str, frame_num: int) -> None:
    """
    Handle animation frame played event.
    
    Args:
        animation_id: ID of the animation
        frame_num: Frame number that was played
    """
    # Update status
    animation_status['current_frame'] = frame_num
    
    # Broadcast update (limit to 5 FPS to avoid flooding)
    if socketio and frame_num % max(1, int(animation_status['fps'] / 5)) == 0:
        socketio.emit('animation_frame', {
            'animation_id': animation_id,
            'frame': frame_num,
            'total_frames': animation_status['total_frames']
        }, namespace='/animations')

# REST API endpoint implementations
@animations_bp.route('/', methods=['GET'])
def get_animations():
    """
    Get a list of all available animations.
    
    Returns:
        JSON array of animations
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    animations = animation_manager.get_all_animations()
    return jsonify(animations)

@animations_bp.route('/<animation_id>', methods=['GET'])
def get_animation(animation_id):
    """
    Get details for a specific animation.
    
    Args:
        animation_id: ID of the animation
        
    Returns:
        JSON animation object
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    animation = animation_manager.get_animation(animation_id)
    if not animation:
        abort(404, f"Animation {animation_id} not found")
        
    return jsonify(animation)

@animations_bp.route('/', methods=['POST'])
def create_animation():
    """
    Create a new animation.
    
    Request Body:
        JSON animation data
        
    Returns:
        JSON with the new animation ID
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    if not request.is_json:
        abort(400, "Request must be JSON")
        
    animation_data = request.get_json()
    
    # Validate animation data (basic checks)
    if not isinstance(animation_data, dict):
        abort(400, "Invalid animation data format")
        
    # Save the animation
    animation_id = animation_manager.save_animation(animation_data)
    if not animation_id:
        abort(500, "Failed to save animation")
        
    # Return the ID
    return jsonify({'id': animation_id, 'status': 'created'})

@animations_bp.route('/<animation_id>', methods=['PUT'])
def update_animation(animation_id):
    """
    Update an existing animation.
    
    Args:
        animation_id: ID of the animation to update
        
    Request Body:
        JSON animation data
        
    Returns:
        JSON with success status
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    if not request.is_json:
        abort(400, "Request must be JSON")
        
    # Check if animation exists
    existing = animation_manager.get_animation(animation_id)
    if not existing:
        abort(404, f"Animation {animation_id} not found")
        
    # Get updated data
    animation_data = request.get_json()
    
    # Delete the old animation
    animation_manager.delete_animation(animation_id)
    
    # Save with the same ID
    animation_data['id'] = animation_id
    new_id = animation_manager.save_animation(animation_data)
    if not new_id or new_id != animation_id:
        abort(500, "Failed to update animation")
        
    return jsonify({'id': animation_id, 'status': 'updated'})

@animations_bp.route('/<animation_id>', methods=['DELETE'])
def delete_animation(animation_id):
    """
    Delete an animation.
    
    Args:
        animation_id: ID of the animation to delete
        
    Returns:
        JSON with success status
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    # Check if animation exists
    existing = animation_manager.get_animation(animation_id)
    if not existing:
        abort(404, f"Animation {animation_id} not found")
        
    # Delete the animation
    result = animation_manager.delete_animation(animation_id)
    if not result:
        abort(500, "Failed to delete animation")
        
    return jsonify({'id': animation_id, 'status': 'deleted'})

@animations_bp.route('/<animation_id>/frames/<int:frame_index>', methods=['GET'])
def get_animation_frame(animation_id, frame_index):
    """
    Get a preview of a specific animation frame.
    
    Args:
        animation_id: ID of the animation
        frame_index: Index of the frame to preview
        
    Returns:
        JSON array of LED RGB values
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    # Check if animation exists
    existing = animation_manager.get_animation(animation_id)
    if not existing:
        abort(404, f"Animation {animation_id} not found")
        
    # Get frame data
    frame_data = animation_manager.preview_frame(animation_id, frame_index)
    if not frame_data:
        abort(404, f"Frame {frame_index} not found")
        
    # Convert to hex color format for easier client-side use
    formatted_frame = []
    for r, g, b in frame_data:
        formatted_frame.append({
            'r': r,
            'g': g,
            'b': b,
            'hex': f'#{r:02x}{g:02x}{b:02x}'
        })
        
    return jsonify({
        'frame_index': frame_index,
        'leds': formatted_frame
    })

@animations_bp.route('/<animation_id>/play', methods=['POST'])
def play_animation(animation_id):
    """
    Play an animation on the LED hardware.
    
    Args:
        animation_id: ID of the animation to play
        
    Query Parameters:
        loop (bool): Whether to loop the animation (default: true)
        fps (int): Maximum frames per second (optional)
        
    Returns:
        JSON with success status
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    # Check if animation exists
    existing = animation_manager.get_animation(animation_id)
    if not existing:
        abort(404, f"Animation {animation_id} not found")
        
    # Get query parameters
    loop = request.args.get('loop', 'true').lower() == 'true'
    fps_str = request.args.get('fps')
    fps = int(fps_str) if fps_str and fps_str.isdigit() else None
    
    # Play the animation
    result = animation_manager.play_animation(animation_id, loop, fps)
    if not result:
        abort(500, "Failed to play animation")
        
    # Update status
    animation_status['loop'] = loop
    if fps:
        animation_status['fps'] = fps
        
    return jsonify({
        'id': animation_id,
        'status': 'playing',
        'loop': loop,
        'fps': fps or existing.get('fps', 30)
    })

@animations_bp.route('/stop', methods=['POST'])
def stop_animation():
    """
    Stop the currently playing animation.
    
    Returns:
        JSON with success status
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    # Stop playback
    result = animation_manager.stop_playback()
    
    return jsonify({'status': 'stopped' if result else 'not_playing'})

@animations_bp.route('/status', methods=['GET'])
def get_animation_status():
    """
    Get the current animation playback status.
    
    Returns:
        JSON with playback status
    """
    return jsonify(animation_status)

@animations_bp.route('/test', methods=['POST'])
def test_animation():
    """
    Test an animation on the LED hardware without saving it.
    
    Request Body:
        JSON with animation_data and loop option
        
    Returns:
        JSON with success status
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    if not request.is_json:
        abort(400, "Request must be JSON")
        
    data = request.get_json()
    
    # Check for required fields
    if 'animation_data' not in data:
        abort(400, "Missing animation_data field")
        
    animation_data = data['animation_data']
    loop = data.get('loop', False)
    
    # Create a temporary animation
    temp_id = f"temp_{uuid.uuid4().hex[:8]}"
    
    try:
        # Validate animation data
        if not isinstance(animation_data, dict):
            if isinstance(animation_data, dict) and 'animations' in animation_data and len(animation_data['animations']) > 0:
                # Handle case where we receive a full export object with multiple animations
                animation_data = animation_data['animations'][0]
            else:
                abort(400, "Invalid animation data format")
        
        # Store the temporary animation
        animation_data['id'] = temp_id
        animation_data['temp'] = True
        animation_manager.save_animation(animation_data)
        
        # Play the animation
        success = animation_manager.play_animation(temp_id, loop)
        if not success:
            abort(500, "Failed to play test animation")
            
        # Return success
        return jsonify({
            'status': 'success',
            'message': 'Test animation is playing',
            'temp_id': temp_id,
            'loop': loop
        })
    
    except Exception as e:
        logger.error(f"Error testing animation: {str(e)}")
        # Clean up temp animation if it was created
        try:
            animation_manager.delete_animation(temp_id)
        except:
            pass
        
        abort(500, f"Error testing animation: {str(e)}")

@animations_bp.route('/import', methods=['POST'])
def import_animation():
    """
    Import an animation from uploaded JSON file.
    
    Returns:
        JSON with success status and imported animation IDs
    """
    if not animation_manager:
        abort(503, "Animation service not available")
        
    if 'file' not in request.files:
        abort(400, "No file provided")
        
    file = request.files['file']
    if file.filename == '':
        abort(400, "No file selected")
        
    if not file.filename.endswith('.json'):
        abort(400, "File must be JSON")
        
    try:
        # Load the JSON data
        data = json.load(file)
        
        if not isinstance(data, dict) or 'animations' not in data:
            abort(400, "Invalid animation file format")
            
        animations = data['animations']
        if not isinstance(animations, list):
            abort(400, "Invalid animations format")
            
        # Import each animation
        imported_ids = []
        for anim_data in animations:
            if isinstance(anim_data, dict) and 'name' in anim_data:
                anim_id = animation_manager.save_animation(anim_data)
                if anim_id:
                    imported_ids.append(anim_id)
                    
        if not imported_ids:
            abort(400, "No valid animations found in file")
            
        return jsonify({
            'status': 'imported',
            'count': len(imported_ids),
            'animations': imported_ids
        })
        
    except json.JSONDecodeError:
        abort(400, "Invalid JSON format")
    except Exception as e:
        abort(500, f"Error importing animations: {str(e)}")

# WebSocket event handlers
# Handle animation cleanup for temporary animations
def cleanup_temp_animations():
    """Periodically clean up temporary animations."""
    if not animation_manager:
        return
        
    while True:
        try:
            # Get all animations
            animations = animation_manager.get_all_animations()
            
            # Find temporary ones
            for anim in animations:
                if anim.get('temp', False) and not animation_status['is_playing']:
                    # Delete if it exists and is not playing
                    animation_manager.delete_animation(anim['id'])
                    logger.info(f"Cleaned up temporary animation {anim['id']}")
                    
        except Exception as e:
            logger.error(f"Error in temp animation cleanup: {str(e)}")
            
        # Sleep for a while before checking again
        time.sleep(60)  # Check every minute

def register_websocket_events(socket_io):
    """
    Register WebSocket event handlers.
    
    Args:
        socket_io: SocketIO instance
    """
    global socketio
    socketio = socket_io
    
    @socketio.on('connect', namespace='/animations')
    def handle_connect():
        """Handle client connection to animations namespace."""
        logger.info("Client connected to animations namespace")
        # Send current status on connect
        emit('animation_status', animation_status)
    
    @socketio.on('disconnect', namespace='/animations')
    def handle_disconnect():
        """Handle client disconnection from animations namespace."""
        logger.info("Client disconnected from animations namespace")
    
    @socketio.on('play_animation', namespace='/animations')
    def handle_play_animation(data):
        """
        Handle play animation request via WebSocket.
        
        Args:
            data: Request data
        """
        animation_id = data.get('animation_id')
        if not animation_id:
            emit('error', {'message': 'No animation ID provided'})
            return
            
        loop = data.get('loop', True)
        fps = data.get('fps')
        
        if animation_manager:
            if animation_manager.play_animation(animation_id, loop, fps):
                emit('success', {'message': f'Playing animation {animation_id}'})
            else:
                emit('error', {'message': f'Failed to play animation {animation_id}'})
        else:
            emit('error', {'message': 'Animation service not available'})
    
    @socketio.on('stop_animation', namespace='/animations')
    def handle_stop_animation():
        """Handle stop animation request via WebSocket."""
        if animation_manager:
            if animation_manager.stop_playback():
                emit('success', {'message': 'Animation stopped'})
            else:
                emit('success', {'message': 'No animation playing'})
        else:
            emit('error', {'message': 'Animation service not available'})
    
    @socketio.on('get_animations', namespace='/animations')
    def handle_get_animations():
        """Handle get animations request via WebSocket."""
        if animation_manager:
            animations = animation_manager.get_all_animations()
            emit('animations', animations)
        else:
            emit('error', {'message': 'Animation service not available'})
    
    @socketio.on('get_animation_frame', namespace='/animations')
    def handle_get_animation_frame(data):
        """
        Handle get animation frame request via WebSocket.
        
        Args:
            data: Request data with animation_id and frame_index
        """
        animation_id = data.get('animation_id')
        frame_index = data.get('frame_index', 0)
        
        if not animation_id:
            emit('error', {'message': 'No animation ID provided'})
            return
            
        if animation_manager:
            frame_data = animation_manager.preview_frame(animation_id, frame_index)
            if frame_data:
                # Convert to hex format
                formatted_frame = []
                for r, g, b in frame_data:
                    formatted_frame.append({
                        'r': r, 
                        'g': g, 
                        'b': b,
                        'hex': f'#{r:02x}{g:02x}{b:02x}'
                    })
                
                emit('animation_frame_data', {
                    'animation_id': animation_id,
                    'frame_index': frame_index,
                    'leds': formatted_frame
                })
            else:
                emit('error', {'message': f'Frame {frame_index} not found'})
        else:
            emit('error', {'message': 'Animation service not available'})

    @socketio.on('send_animation_frame', namespace='/animations')
    def handle_send_animation_frame(data):
        """
        Handle receiving a single animation frame from the client to be sent to hardware in real-time.
        """
        logger.debug(f"Received send_animation_frame event") # Avoid logging potentially large data by default
        if not animation_manager:
            emit('error', {'message': 'Animation service not available'})
            logger.warning("send_animation_frame: Animation manager not available.")
            return

        frame_data = data.get('frame_data') # Expecting a list of RGB tuples/dicts or similar
        if not frame_data or not isinstance(frame_data, list):
             emit('error', {'message': 'Invalid or missing frame_data.'})
             logger.warning(f"send_animation_frame: Invalid frame data received: {data}")
             return

        try:
            # TODO: Confirm the correct method on animation_manager or protocol for sending a single frame.
            # Assuming a method like 'send_realtime_frame' exists for now.
            # This method would likely need to format the frame data according to the
            # AnimationProtocol and send it via the CAN interface.
            success = animation_manager.send_realtime_frame(frame_data) # Placeholder method call

            if success:
                # Acknowledge frame receipt/processing if needed, but be mindful of performance.
                # emit('frame_ack', {'status': 'success'})
                logger.debug("Successfully processed/sent real-time frame.")
                pass
            else:
                emit('error', {'message': 'Failed to send frame data via CAN.'})
                logger.error("send_animation_frame: AnimationManager failed to send real-time frame.")

        except AttributeError:
             logger.error("send_animation_frame: Method 'send_realtime_frame' not found on AnimationManager. Needs implementation.")
             emit('error', {'message': 'Server function to send real-time frame not implemented.'})
        except Exception as e:
            logger.error(f"Error processing send_animation_frame: {str(e)}")
            emit('error', {'message': f'Internal server error processing frame: {str(e)}'})


def register_animation_routes(app, can_interface, socketio, animation_dir=None):
    """
    Register animation API routes and WebSocket handlers.
    
    Args:
        app: Flask application
        can_interface: CANInterfaceWrapper instance
        socketio: SocketIO instance
        animation_dir: Directory to load/save animations from/to
    """
    """
    Register animation API routes and WebSocket handlers.
    
    Args:
        app: Flask application
        can_interface: CANInterfaceWrapper instance
        socketio: SocketIO instance
        animation_dir: Directory to load/save animations from/to
    """
    global animation_manager
    
    # Create animation protocol and manager
    protocol = AnimationProtocol(can_interface)
    animation_manager = AnimationManager(protocol, animation_dir)
    
    # Set up callbacks
    animation_manager.set_callbacks(
        on_playback_started=handle_animation_playback_started,
        on_playback_stopped=handle_animation_playback_stopped,
        on_frame_played=handle_animation_frame_played
    )
    
    # Register REST API blueprint
    app.register_blueprint(animations_bp)
    
    # Register WebSocket event handlers
    register_websocket_events(socketio)
    
    # Start cleanup thread for temporary animations
    cleanup_thread = threading.Thread(target=cleanup_temp_animations, daemon=True)
    cleanup_thread.start()
    
    logger.info("Animation API routes and WebSocket handlers registered")
    
    return animation_manager
