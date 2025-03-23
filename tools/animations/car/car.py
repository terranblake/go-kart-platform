import json
import math
import colorsys
import random

class CarLightingAnimator:
    def __init__(self,
                 headlight_width=30,      # Width of headlight strip
                 headlight_height=2,      # Height of headlight strip 
                 taillight_width=30,      # Width of taillight strip
                 taillight_height=2,      # Height of taillight strip
                 simulation_steps=60,     # Number of frames in animation
                 frame_speed=30):         # ms per frame
        
        self.headlight_width = headlight_width
        self.headlight_height = headlight_height
        self.taillight_width = taillight_width
        self.taillight_height = taillight_height
        self.simulation_steps = simulation_steps
        self.frame_speed = frame_speed
        
        # Light state colors
        self.colors = {
            # Headlight colors
            'headlight_off': '#000000',
            'headlight_low': '#f0f0ff',
            'headlight_high': '#ffffff',
            'headlight_drl': '#a0a0ff',
            'headlight_turn_signal': '#ffaa00',
            'headlight_fog': '#ffffd0',
            'headlight_welcome': '#80b0ff',
            
            # Taillight colors
            'taillight_off': '#000000',
            'taillight_running': '#660000',
            'taillight_brake': '#ff0000',
            'taillight_turn_signal': '#ff8800',
            'taillight_reverse': '#ffffff',
            'taillight_fog': '#ff2000',
            'taillight_hazard': '#ff5500'
        }
        
        # Animation duration (in frames) for each effect
        self.durations = {
            'turn_signal': 15,      # Full turn signal cycle
            'welcome_seq': 40,      # Welcome sequence
            'hazard': 15,           # Hazard light cycle
            'warn_fade': 30,        # Warning fade effect
            'brake_pulse': 10,      # Brake pulse effect
            'scanning': 20,         # Scanning effect
        }
    
    def _color_to_rgb(self, color_hex):
        """Convert hex color to RGB tuple."""
        if color_hex.startswith('#'):
            color_hex = color_hex[1:]
        return (
            int(color_hex[0:2], 16) / 255.0,
            int(color_hex[2:4], 16) / 255.0,
            int(color_hex[4:6], 16) / 255.0
        )
    
    def _blend_colors(self, color1_hex, color2_hex, blend_factor):
        """Blend two colors by the given factor (0 = all color1, 1 = all color2)."""
        color1 = self._color_to_rgb(color1_hex)
        color2 = self._color_to_rgb(color2_hex)
        
        blended = (
            color1[0] * (1 - blend_factor) + color2[0] * blend_factor,
            color1[1] * (1 - blend_factor) + color2[1] * blend_factor,
            color1[2] * (1 - blend_factor) + color2[2] * blend_factor
        )
        
        # Convert back to hex
        r = int(max(0, min(1, blended[0])) * 255)
        g = int(max(0, min(1, blended[1])) * 255)
        b = int(max(0, min(1, blended[2])) * 255)
        return f"#{r:02x}{g:02x}{b:02x}"
    
    def _create_headlight_frame(self, state, frame_index):
        """Create LED data for a headlight frame in the given state."""
        leds = []
        
        # Handle different headlight states
        if state == 'off':
            # All LEDs off
            pass
        
        elif state == 'low_beam':
            # Low beam headlights
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    # Optional: Add slight brightness variation for realism
                    brightness_var = 1.0 - (random.random() * 0.05)
                    color = self._blend_colors(self.colors['headlight_off'], 
                                             self.colors['headlight_low'], 
                                             brightness_var)
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'high_beam':
            # High beam headlights - brighter than low beams
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    # Optional: Add slight brightness variation for realism
                    brightness_var = 1.0 - (random.random() * 0.05)
                    color = self._blend_colors(self.colors['headlight_off'], 
                                             self.colors['headlight_high'], 
                                             brightness_var)
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'drl':
            # Daytime running lights - typically the top row or outer portions
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    if y == 0 or x < 5 or x >= self.headlight_width - 5:
                        leds.append({
                            "x": x,
                            "y": y,
                            "color": self.colors['headlight_drl']
                        })
        
        elif state == 'left_turn':
            # Left turn signal (usually the left portion of headlights)
            signal_width = min(10, self.headlight_width // 3)
            
            # Calculate turn signal state (on/off)
            turn_cycle = self.durations['turn_signal']
            is_on = (frame_index % turn_cycle) < (turn_cycle // 2)
            
            # Basic headlights always on
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    # Default headlight state
                    color = self.colors['headlight_low']
                    
                    # Left side turn signal
                    if x < signal_width and is_on:
                        color = self.colors['headlight_turn_signal']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'right_turn':
            # Right turn signal (usually the right portion of headlights)
            signal_width = min(10, self.headlight_width // 3)
            
            # Calculate turn signal state (on/off)
            turn_cycle = self.durations['turn_signal']
            is_on = (frame_index % turn_cycle) < (turn_cycle // 2)
            
            # Basic headlights always on
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    # Default headlight state
                    color = self.colors['headlight_low']
                    
                    # Right side turn signal
                    if x >= self.headlight_width - signal_width and is_on:
                        color = self.colors['headlight_turn_signal']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'hazard':
            # Hazard lights - both turn signals blinking
            signal_width = min(10, self.headlight_width // 3)
            
            # Calculate hazard state (on/off)
            hazard_cycle = self.durations['hazard']
            is_on = (frame_index % hazard_cycle) < (hazard_cycle // 2)
            
            # Basic headlights always on
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    # Default headlight state
                    color = self.colors['headlight_low']
                    
                    # Both sides blinking for hazard
                    if (x < signal_width or x >= self.headlight_width - signal_width) and is_on:
                        color = self.colors['headlight_turn_signal']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'fog':
            # Fog lights - often yellower than regular headlights
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    # Optional: Add slight brightness variation for realism
                    brightness_var = 1.0 - (random.random() * 0.05)
                    color = self._blend_colors(self.colors['headlight_off'], 
                                             self.colors['headlight_fog'], 
                                             brightness_var)
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'welcome':
            # Welcome sequence animation - sweeping effect from center
            welcome_duration = self.durations['welcome_seq']
            progress = min(1.0, (frame_index % welcome_duration) / welcome_duration)
            
            # Calculate sweep position
            half_width = self.headlight_width // 2
            sweep_pos = int(progress * half_width)
            
            for y in range(self.headlight_height):
                for x in range(self.headlight_width):
                    distance_from_center = abs(x - half_width)
                    
                    # LEDs light up in sequence from center outward
                    if distance_from_center <= sweep_pos:
                        # Fade from center (brightest) to edges
                        brightness = 1.0 - (distance_from_center / half_width) * 0.3
                        color = self._blend_colors(self.colors['headlight_off'], 
                                                 self.colors['headlight_welcome'], 
                                                 brightness)
                        leds.append({
                            "x": x,
                            "y": y,
                            "color": color
                        })
        
        return leds
    
    def _create_taillight_frame(self, state, frame_index):
        """Create LED data for a taillight frame in the given state."""
        leds = []
        
        # Handle different taillight states
        if state == 'off':
            # All LEDs off
            pass
        
        elif state == 'running':
            # Running lights (dim red)
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": self.colors['taillight_running']
                    })
        
        elif state == 'brake':
            # Brake lights (bright red)
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": self.colors['taillight_brake']
                    })
        
        elif state == 'brake_pulse':
            # Pulsing brake lights (varies intensity)
            pulse_duration = self.durations['brake_pulse']
            pulse_progress = (frame_index % pulse_duration) / pulse_duration
            
            # Pulse intensity (high to low and back)
            if pulse_progress < 0.5:
                intensity = 1.0 - pulse_progress * 0.4  # 1.0 down to 0.8
            else:
                intensity = 0.8 + (pulse_progress - 0.5) * 0.4  # 0.8 up to 1.0
            
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    color = self._blend_colors(self.colors['taillight_running'], 
                                             self.colors['taillight_brake'], 
                                             intensity)
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'left_turn':
            # Left turn signal (usually left portion of taillights)
            signal_width = min(10, self.taillight_width // 3)
            
            # Calculate turn signal state (on/off)
            turn_cycle = self.durations['turn_signal']
            is_on = (frame_index % turn_cycle) < (turn_cycle // 2)
            
            # Basic running lights always on
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default taillight state
                    color = self.colors['taillight_running']
                    
                    # Left side turn signal
                    if x < signal_width and is_on:
                        color = self.colors['taillight_turn_signal']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'right_turn':
            # Right turn signal (usually right portion of taillights)
            signal_width = min(10, self.taillight_width // 3)
            
            # Calculate turn signal state (on/off)
            turn_cycle = self.durations['turn_signal']
            is_on = (frame_index % turn_cycle) < (turn_cycle // 2)
            
            # Basic running lights always on
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default taillight state
                    color = self.colors['taillight_running']
                    
                    # Right side turn signal
                    if x >= self.taillight_width - signal_width and is_on:
                        color = self.colors['taillight_turn_signal']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'brake_left_turn':
            # Brake lights with left turn signal
            signal_width = min(10, self.taillight_width // 3)
            
            # Calculate turn signal state (on/off)
            turn_cycle = self.durations['turn_signal']
            is_on = (frame_index % turn_cycle) < (turn_cycle // 2)
            
            # Brake lights always on
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default brake state
                    color = self.colors['taillight_brake']
                    
                    # Left side turn signal
                    if x < signal_width and is_on:
                        color = self.colors['taillight_turn_signal']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'brake_right_turn':
            # Brake lights with right turn signal
            signal_width = min(10, self.taillight_width // 3)
            
            # Calculate turn signal state (on/off)
            turn_cycle = self.durations['turn_signal']
            is_on = (frame_index % turn_cycle) < (turn_cycle // 2)
            
            # Brake lights always on
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default brake state
                    color = self.colors['taillight_brake']
                    
                    # Right side turn signal
                    if x >= self.taillight_width - signal_width and is_on:
                        color = self.colors['taillight_turn_signal']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'hazard':
            # Hazard lights - both turn signals blinking
            signal_width = min(10, self.taillight_width // 3)
            
            # Calculate hazard state (on/off)
            hazard_cycle = self.durations['hazard']
            is_on = (frame_index % hazard_cycle) < (hazard_cycle // 2)
            
            # Running lights always on
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default taillight state
                    color = self.colors['taillight_running']
                    
                    # Both sides blinking for hazard
                    if (x < signal_width or x >= self.taillight_width - signal_width) and is_on:
                        color = self.colors['taillight_hazard']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'reverse':
            # Reverse lights (white)
            reverse_width = min(8, self.taillight_width // 4)
            
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default taillight state
                    color = self.colors['taillight_running']
                    
                    # Center section for reverse lights
                    center_start = (self.taillight_width - reverse_width) // 2
                    if center_start <= x < center_start + reverse_width:
                        color = self.colors['taillight_reverse']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'brake_reverse':
            # Brake lights with reverse lights
            reverse_width = min(8, self.taillight_width // 4)
            
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default brake state
                    color = self.colors['taillight_brake']
                    
                    # Center section for reverse lights
                    center_start = (self.taillight_width - reverse_width) // 2
                    if center_start <= x < center_start + reverse_width:
                        color = self.colors['taillight_reverse']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'fog':
            # Rear fog light - brighter red, often on one side
            fog_width = min(8, self.taillight_width // 4)
            fog_start = self.taillight_width - fog_width
            
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Default taillight state
                    color = self.colors['taillight_running']
                    
                    # Fog light on right side
                    if x >= fog_start:
                        color = self.colors['taillight_fog']
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        elif state == 'scanning':
            # Knight Rider style scanning effect
            scan_duration = self.durations['scanning']
            scan_progress = (frame_index % scan_duration) / scan_duration
            
            # Calculate scanning position
            if scan_progress < 0.5:
                scan_pos = scan_progress * 2 * self.taillight_width
            else:
                scan_pos = (1 - scan_progress) * 2 * self.taillight_width
            
            scan_width = max(3, self.taillight_width // 10)
            half_scan = scan_width // 2
            
            for y in range(self.taillight_height):
                for x in range(self.taillight_width):
                    # Base running lights
                    color = self.colors['taillight_running']
                    
                    # Scanning effect
                    distance = abs(x - int(scan_pos))
                    if distance <= half_scan:
                        # Fade intensity based on distance from scan center
                        scan_intensity = 1.0 - (distance / half_scan)
                        color = self._blend_colors(self.colors['taillight_running'], 
                                                self.colors['taillight_brake'], 
                                                scan_intensity)
                    
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": color
                    })
        
        return leds
    
    def generate_headlight_animation(self, state):
        """Generate a complete animation for a headlight state."""
        frames = []
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            # Create headlight data for this frame
            headlight_leds = self._create_headlight_frame(state, frame_index)
            
            # Add to frames
            frames.append({"leds": headlight_leds})
        
        # Create the animation JSON
        animation = {
            "name": f"Headlight: {state}",
            "type": "custom",
            "speed": self.frame_speed,
            "direction": "forward",
            "dimensions": {
                "length": self.headlight_width,
                "height": self.headlight_height
            },
            "frames": frames
        }
        
        return animation
    
    def generate_taillight_animation(self, state):
        """Generate a complete animation for a taillight state."""
        frames = []
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            # Create taillight data for this frame
            taillight_leds = self._create_taillight_frame(state, frame_index)
            
            # Add to frames
            frames.append({"leds": taillight_leds})
        
        # Create the animation JSON
        animation = {
            "name": f"Taillight: {state}",
            "type": "custom",
            "speed": self.frame_speed,
            "direction": "forward",
            "dimensions": {
                "length": self.taillight_width,
                "height": self.taillight_height
            },
            "frames": frames
        }
        
        return animation
    
    def generate_all_state_animations(self):
        """Generate separate animations for all common car lighting states."""
        animations = []
        
        # Headlight states
        headlight_states = [
            'off', 'low_beam', 'high_beam', 'drl', 'left_turn', 
            'right_turn', 'hazard', 'fog', 'welcome'
        ]
        
        # Taillight states
        taillight_states = [
            'off', 'running', 'brake', 'brake_pulse', 'left_turn', 
            'right_turn', 'brake_left_turn', 'brake_right_turn', 
            'hazard', 'reverse', 'brake_reverse', 'fog', 'scanning'
        ]
        
        # Generate animations for each headlight state
        for state in headlight_states:
            animation = self.generate_headlight_animation(state)
            animations.append(animation)
            
            # Save to file
            filename = f"headlight_{state}.json"
            with open(filename, "w") as f:
                json.dump(animation, f, indent=2)
        
        # Generate animations for each taillight state
        for state in taillight_states:
            animation = self.generate_taillight_animation(state)
            animations.append(animation)
            
            # Save to file
            filename = f"taillight_{state}.json"
            with open(filename, "w") as f:
                json.dump(animation, f, indent=2)
        
        # Generate a combined file with all animations
        combined = {"animations": animations}
        with open("all_car_lighting.json", "w") as f:
            json.dump(combined, f, indent=2)
        
        print(f"Generated {len(animations)} car lighting animations")
        print(f"Saved individual files and combined file 'all_car_lighting.json'")
        
        return combined


# Generate all car animations
def generate_car_animations():
    animator = CarLightingAnimator(
        headlight_width=60,
        headlight_height=1,
        taillight_width=60,
        taillight_height=1,
        simulation_steps=60,
        frame_speed=30
    )
    
    # Generate all state animations and save to files
    combined = animator.generate_all_state_animations()
    return combined

# Generate the animations
animations = generate_car_animations()