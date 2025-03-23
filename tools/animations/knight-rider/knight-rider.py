import json
import math

def generate_knight_rider_animation(num_leds=60, tail_length=5, frames_per_direction=30, brightness_levels=10):
    """
    Generate a Knight Rider scanner animation JSON for an LED strip.
    
    Args:
        num_leds: Number of LEDs in the strip
        tail_length: Length of the fade-out trail
        frames_per_direction: Number of frames for one complete pass in one direction
        brightness_levels: Number of brightness steps in the fade-out tail
    
    Returns:
        JSON-formatted string with the Knight Rider animation
    """
    # Calculate step size for smooth movement
    step_size = (num_leds - 1) / (frames_per_direction - 1)
    
    # Generate frames for movement in both directions
    frames = []
    
    # First direction (left to right)
    for frame in range(frames_per_direction):
        position = frame * step_size
        leds = generate_scanner_frame(position, num_leds, tail_length, brightness_levels)
        frames.append({"leds": leds})
    
    # Second direction (right to left)
    for frame in range(frames_per_direction):
        position = num_leds - 1 - (frame * step_size)
        leds = generate_scanner_frame(position, num_leds, tail_length, brightness_levels)
        frames.append({"leds": leds})
    
    # Create complete animation
    animation = {
        "name": "Knight Rider Scanner",
        "type": "custom",
        "speed": 40,  # 40ms per frame for smooth movement
        "direction": "forward",
        "frames": frames
    }
    
    return json.dumps(animation, indent=2)

def generate_scanner_frame(position, num_leds, tail_length, brightness_levels):
    """
    Generate a single frame of the Knight Rider scanner effect.
    
    Args:
        position: Current position of the scanner (can be floating point for smooth movement)
        num_leds: Total number of LEDs
        tail_length: Length of the fade-out trail
        brightness_levels: Number of brightness steps in the fade
    
    Returns:
        List of LED objects with positions and colors
    """
    leds = []
    position_int = int(position)
    
    # Calculate positions for LEDs in the tail (before the main point)
    for i in range(tail_length):
        tail_pos = position_int - i - 1
        if 0 <= tail_pos < num_leds:
            brightness = (tail_length - i) / tail_length
            intensity = int(255 * brightness)
            # Convert intensity to hex color with fading red
            color = f"#{intensity:02x}0000"
            leds.append({
                "index": tail_pos,
                "color": color  # Fading red based on distance from main point
            })
    
    # Add the main point (brightest part of the scanner)
    if 0 <= position_int < num_leds:
        leds.append({
            "index": position_int,
            "color": "#ff0000"  # Full brightness red
        })
    
    # Calculate positions for LEDs in the tail (after the main point)
    for i in range(tail_length):
        tail_pos = position_int + i + 1
        if 0 <= tail_pos < num_leds:
            brightness = (tail_length - i) / tail_length
            intensity = int(255 * brightness)
            leds.append({
                "index": tail_pos,
                "color": f"#ff0000"  # Pure red with brightness
            })
    
    return leds

# Generate animation for a 60 LED strip with a tail length of 5 LEDs
knight_rider_json = generate_knight_rider_animation(num_leds=60, tail_length=5)
print(knight_rider_json)

# If you want to save to a file
with open("animation.json", "w") as f:
    f.write(knight_rider_json)