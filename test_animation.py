#!/usr/bin/env python3
"""
Simple script to test the animation API by sending a test animation.
"""

import requests
import json
import time

# Server configuration
BASE_URL = "http://localhost:5000/api/animations"

# Create a basic animation
def create_test_animation():
    # Define a simple chase animation with 3 colors
    animation = {
        "name": "Test Rainbow Chase",
        "type": "chase",
        "speed": 100,  # Lower is faster
        "dimensions": {
            "length": 60,
            "height": 1
        },
        "colors": ["#ff0000", "#00ff00", "#0000ff"],  # Red, Green, Blue
        "frames": []
    }
    
    # Create 30 frames for a simple chase animation
    for i in range(30):
        frame = {"leds": []}
        # Light up 3 consecutive LEDs with different colors, moving forward
        for j in range(3):
            position = (i + j) % animation["dimensions"]["length"]
            color_idx = j % len(animation["colors"])
            frame["leds"].append({
                "index": position,
                "color": animation["colors"][color_idx]
            })
        animation["frames"].append(frame)
    
    return animation

def send_test_animation(animation, loop=True):
    """Send the animation to the test endpoint."""
    
    data = {
        "animation_data": animation,
        "loop": loop
    }
    
    try:
        response = requests.post(f"{BASE_URL}/test", json=data)
        
        if response.status_code == 200:
            print("Animation sent successfully!")
            print(response.json())
            return response.json().get("temp_id")
        else:
            print(f"Error: {response.status_code}")
            print(response.text)
            return None
            
    except Exception as e:
        print(f"Request failed: {str(e)}")
        return None

def stop_animation():
    """Stop any currently playing animation."""
    try:
        response = requests.post(f"{BASE_URL}/stop")
        
        if response.status_code == 200:
            print("Animation stopped successfully!")
            print(response.json())
            return True
        else:
            print(f"Error stopping animation: {response.status_code}")
            print(response.text)
            return False
            
    except Exception as e:
        print(f"Stop request failed: {str(e)}")
        return False

if __name__ == "__main__":
    print("Creating test animation...")
    animation = create_test_animation()
    
    print("Sending animation to server...")
    temp_id = send_test_animation(animation, loop=True)
    
    if temp_id:
        print(f"Animation playing with ID: {temp_id}")
        print("Animation will play for 10 seconds...")
        try:
            time.sleep(10)  # Let the animation play for 10 seconds
        except KeyboardInterrupt:
            print("Playback interrupted by user")
            
        print("Stopping animation...")
        stop_animation()
    else:
        print("Failed to send animation") 