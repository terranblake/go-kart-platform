#!/usr/bin/env python3
"""
Simple script to test the animation API by sending a test animation.
"""

import requests
import json
import time
import sys

# Server configuration - Default with command line override option
BASE_URL = "http://localhost:5000/api/animations"
if len(sys.argv) > 1:
    BASE_URL = f"http://{sys.argv[1]}:5000/api/animations"

print(f"Using server: {BASE_URL}")

def list_animations():
    """List all available animations."""
    try:
        response = requests.get(BASE_URL)
        if response.status_code == 200:
            animations = response.json()
            print(f"Found {len(animations)} animations:")
            for anim in animations:
                print(f"  - {anim.get('name', 'Unnamed')} (ID: {anim.get('id', 'unknown')})")
            return animations
        else:
            print(f"Failed to list animations: {response.status_code}")
            return []
    except Exception as e:
        print(f"Error listing animations: {str(e)}")
        return []

if __name__ == "__main__":
    print("Checking server status...")
    if not check_server_status():
        print("Server may not be running correctly.")
        print("Continuing anyway for testing...")
    
    # List existing animations
    list_animations()
    
    print("\nCreating test animation...")
    animation = create_test_animation()
    
    # Print first frame for debugging
    print(f"First frame sample: {animation['frames'][0]}")
    
    print("\nSending animation to server...")
    temp_id = send_test_animation(animation, loop=True)
    
    if temp_id:
        print(f"Animation playing with ID: {temp_id}")
        print("Animation will play for 3 seconds...")
        try:
            time.sleep(3)  # Let the animation play for just 3 seconds
        except KeyboardInterrupt:
            print("Playback interrupted by user")
            
        print("Stopping animation...")
        stop_animation()
    else:
        print("Failed to send animation") 