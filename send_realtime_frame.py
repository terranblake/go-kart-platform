#!/usr/bin/env python3
"""
Script to demonstrate sending real-time animation frames using WebSockets.
This script creates a simple animation effect by sending individual frames
directly to the LED hardware in real-time.
"""

import socketio
import time
import math
import colorsys

# Server configuration
WS_URL = "http://localhost:5000"
NAMESPACE = "/animations"

# LED strip configuration
NUM_LEDS = 60

# Animation parameters
DURATION = 15  # seconds to run the animation
EFFECT_SPEED = 0.2  # Speed of the animation (lower is faster)
HUE_SHIFT_RATE = 0.01  # How quickly the base hue changes

# Initialize the Socket.IO client
sio = socketio.Client()

@sio.event
def connect():
    print("Connected to server")

@sio.event
def disconnect():
    print("Disconnected from server")

@sio.on("error", namespace=NAMESPACE)
def on_error(data):
    print(f"Error: {data['message']}")

def hsv_to_rgb(h, s, v):
    """Convert HSV color to RGB tuple (0-255 range)."""
    r, g, b = colorsys.hsv_to_rgb(h, s, v)
    return int(r * 255), int(g * 255), int(b * 255)

def create_rainbow_frame(base_hue, time_value):
    """Create a frame with a rainbow pattern that shifts over time."""
    frame = []
    
    for i in range(NUM_LEDS):
        # Calculate hue based on position and time
        hue = (base_hue + (i / NUM_LEDS)) % 1.0
        
        # Use sine wave for brightness pulsing
        brightness = 0.5 + 0.5 * math.sin(time_value + i * EFFECT_SPEED)
        
        # Convert HSV to RGB
        r, g, b = hsv_to_rgb(hue, 1.0, brightness)
        
        # Add to frame data
        frame.append((r, g, b))
        
    return frame

def create_pulse_frame(base_hue, time_value):
    """Create a frame with a pulsing effect."""
    frame = []
    
    for i in range(NUM_LEDS):
        # Calculate wave position based on LED position and time
        wave_pos = math.sin(time_value + i * EFFECT_SPEED)
        brightness = (wave_pos + 1) / 2  # Scale from -1,1 to 0,1
        
        # Alternate between two colors
        hue = base_hue if i % 2 == 0 else (base_hue + 0.5) % 1.0
        
        # Convert HSV to RGB
        r, g, b = hsv_to_rgb(hue, 1.0, brightness)
        
        # Add to frame data
        frame.append((r, g, b))
        
    return frame

def run_animation():
    """Run the animation by sending frames in real-time."""
    try:
        # Connect to the server
        print(f"Connecting to {WS_URL}...")
        sio.connect(WS_URL, namespaces=[NAMESPACE])
        
        # Store the start time
        start_time = time.time()
        base_hue = 0.0
        
        # Animation loop
        print(f"Starting animation for {DURATION} seconds...")
        
        while time.time() - start_time < DURATION:
            current_time = time.time() - start_time
            
            # Create a frame based on the current time
            if int(current_time) % 2 == 0:
                # Even seconds: rainbow effect
                frame = create_rainbow_frame(base_hue, current_time)
            else:
                # Odd seconds: pulse effect
                frame = create_pulse_frame(base_hue, current_time)
            
            # Send the frame through WebSocket
            sio.emit('send_animation_frame', {
                'frame_data': frame
            }, namespace=NAMESPACE)
            
            # Shift the base hue for next frame
            base_hue = (base_hue + HUE_SHIFT_RATE) % 1.0
            
            # Wait a bit to control frame rate (about 30 FPS)
            time.sleep(1/30)
            
        print("Animation complete!")
        
    except socketio.exceptions.ConnectionError as e:
        print(f"Connection error: {e}")
    except Exception as e:
        print(f"Error: {e}")
    finally:
        if sio.connected:
            # Disconnect from server
            sio.disconnect()

if __name__ == "__main__":
    run_animation() 