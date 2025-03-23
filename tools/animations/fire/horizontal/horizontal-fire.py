import random
import json
import math
import time
from collections import deque

class HorizontalFireSimulation:
    def __init__(self, num_leds=60, simulation_steps=60,
                 flame_width=0.4, flame_speed=0.5, flame_density=0.7, 
                 color_temp=1800, intensity=1.0, color_palette_size=100,
                 red_bias=1.0, yellow_bias=1.0, blue_accent=0.0, white_edge=True,
                 ember_glow=0.3, outer_fade=0.2, inner_fade=0.6,
                 flicker_frequency=0.7, flicker_intensity=1.0,
                 flow_direction="right", symmetrical=False):
        """
        Initialize the fire simulation optimized for horizontal LED strips.
        
        Parameters:
        - num_leds: Number of LEDs in the strip
        - simulation_steps: Number of frames to generate
        - flame_width: Width of flame "tongues" (0.1-1.0)
        - flame_speed: How fast flames move along the strip (0.1-2.0)
        - flame_density: How many flame "tongues" appear (0.1-1.0)
        - color_temp: Color temperature in Kelvin (lower = redder, higher = yellower)
        - intensity: Overall brightness multiplier (0.0-2.0)
        - color_palette_size: Number of colors in the palette
        - red_bias: Multiplier for red component (0.5-2.0)
        - yellow_bias: Multiplier for yellow component (0.5-2.0)
        - blue_accent: Amount of blue to add to the flames (0.0-0.3)
        - white_edge: Whether to have white edges on the flames (boolean)
        - ember_glow: Intensity of the ember glow (0.0-1.0)
        - outer_fade: How quickly flames fade at strip edges (0.0-1.0)
        - inner_fade: How quickly flames fade in the middle (0.0-1.0)
        - flicker_frequency: How often brightness flickers (0.0-1.0)
        - flicker_intensity: How much the brightness varies (0.5-2.0)
        - flow_direction: Direction flames flow ("left", "right", "both", "center_out")
        - symmetrical: Whether to make the pattern symmetrical (boolean)
        """
        self.num_leds = num_leds
        self.heat = [0] * num_leds
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Flame pattern parameters
        self.flame_width = max(0.1, min(1.0, flame_width))
        self.flame_speed = max(0.1, min(2.0, flame_speed))
        self.flame_density = max(0.1, min(1.0, flame_density))
        
        # Color and intensity parameters
        self.color_temp = max(1000, min(10000, color_temp))
        self.intensity = max(0.1, min(2.0, intensity))
        self.red_bias = max(0.5, min(2.0, red_bias))
        self.yellow_bias = max(0.5, min(2.0, yellow_bias))
        self.blue_accent = max(0.0, min(0.3, blue_accent))
        self.white_edge = white_edge
        self.ember_glow = max(0.0, min(1.0, ember_glow))
        
        # Horizontal-specific parameters
        self.outer_fade = max(0.0, min(1.0, outer_fade))
        self.inner_fade = max(0.0, min(1.0, inner_fade))
        self.flicker_frequency = max(0.0, min(1.0, flicker_frequency))
        self.flicker_intensity = max(0.5, min(2.0, flicker_intensity))
        self.flow_direction = flow_direction
        self.symmetrical = symmetrical
        
        # Initialize noise parameters for flame movement
        self.noise_offsets = [random.random() * 100 for _ in range(4)]
        
        # Create color palette from deep red to bright yellow
        self.palette = self._create_fire_palette(color_palette_size)
        
        # Generate flame wave patterns based on the density
        num_flames = int(max(2, self.flame_density * 10))
        self.flame_positions = []
        self.flame_speeds = []
        self.flame_heights = []
        self.flame_widths = []
        
        for _ in range(num_flames):
            if self.flow_direction == "right":
                self.flame_positions.append(random.random() * 0.3) # Start at left side
            elif self.flow_direction == "left":
                self.flame_positions.append(0.7 + random.random() * 0.3) # Start at right side
            elif self.flow_direction == "both":
                self.flame_positions.append(random.random()) # Start anywhere
            elif self.flow_direction == "center_out":
                self.flame_positions.append(0.5) # Start in center
            
            # Randomize speeds but keep the direction consistent
            base_speed = self.flame_speed * (0.7 + random.random() * 0.6)
            if self.flow_direction == "right":
                self.flame_speeds.append(base_speed)
            elif self.flow_direction == "left":
                self.flame_speeds.append(-base_speed)
            elif self.flow_direction == "both":
                self.flame_speeds.append(base_speed * random.choice([-1, 1]))
            elif self.flow_direction == "center_out":
                self.flame_speeds.append(base_speed * (1 if self.flame_positions[-1] >= 0.5 else -1))
            
            # Random height and width of each flame
            self.flame_heights.append(0.4 + random.random() * 0.6)
            self.flame_widths.append(self.flame_width * (0.5 + random.random() * 1.0))
    
    def _create_fire_palette(self, num_colors):
        """Create a fire color palette with customizable parameters."""
        palette = []
        
        # Helper function to convert color temperature to RGB (simplified algorithm)
        def temp_to_rgb(temp_k):
            # Temperature range check
            temp_k = max(1000, min(40000, temp_k))
            
            # Simplified temperature to RGB conversion
            # Based on approximation of blackbody radiation
            # Adjusted for fire appearance
            temp_k = temp_k / 100.0
            
            # Red component
            if temp_k <= 66:
                r = 255
            else:
                r = temp_k - 60
                r = 329.698727446 * (r ** -0.1332047592)
                r = max(0, min(255, r))
            
            # Green component
            if temp_k <= 66:
                g = temp_k
                g = 99.4708025861 * math.log(g) - 161.1195681661
            else:
                g = temp_k - 60
                g = 288.1221695283 * (g ** -0.0755148492)
            g = max(0, min(255, g))
            
            # Blue component
            if temp_k >= 66:
                b = 255
            elif temp_k <= 19:
                b = 0
            else:
                b = temp_k - 10
                b = 138.5177312231 * math.log(b) - 305.0447927307
                b = max(0, min(255, b))
                
            return r, g, b
        
        # Base temperature for the palette
        base_temp_k = self.color_temp
        
        # Ember glow (first 10% of palette)
        ember_ratio = 0.1
        for i in range(int(num_colors * ember_ratio)):
            ratio = i / (num_colors * ember_ratio)
            # Embers are darker and redder
            r = int(min(255, 80 * ratio * self.red_bias * self.ember_glow * self.intensity))
            g = int(min(255, 20 * ratio * self.ember_glow * self.intensity))
            b = int(min(255, 5 * ratio * self.blue_accent * self.ember_glow * self.intensity))
            palette.append((r, g, b))
        
        # Black to red transition (next 20% of palette)
        red_ratio = 0.2
        for i in range(int(num_colors * red_ratio)):
            ratio = i / (num_colors * red_ratio)
            temp = 1000 + (base_temp_k - 1000) * ratio * 0.3  # Gradual temperature increase
            r, g, b = temp_to_rgb(temp)
            r = int(min(255, r * self.red_bias * self.intensity))
            g = int(min(255, g * 0.7 * self.intensity))  # Reduce green for redder flames
            b = int(min(255, b * self.blue_accent * self.intensity))
            palette.append((r, g, b))
        
        # Red to orange transition (next 30% of palette)
        orange_ratio = 0.3
        for i in range(int(num_colors * orange_ratio)):
            ratio = i / (num_colors * orange_ratio)
            temp = base_temp_k * (0.3 + ratio * 0.3)  # Continue temperature increase
            r, g, b = temp_to_rgb(temp)
            r = int(min(255, r * self.red_bias * self.intensity))
            g = int(min(255, g * (0.7 + ratio * 0.3) * self.intensity))
            b = int(min(255, b * self.blue_accent * self.intensity))
            palette.append((r, g, b))
        
        # Orange to yellow (next 30% of palette)
        yellow_ratio = 0.3
        for i in range(int(num_colors * yellow_ratio)):
            ratio = i / (num_colors * yellow_ratio)
            temp = base_temp_k * (0.6 + ratio * 0.4)  # Continue temperature increase
            r, g, b = temp_to_rgb(temp)
            r = int(min(255, r * self.intensity))
            g = int(min(255, g * self.yellow_bias * self.intensity))
            b = int(min(255, (b + ratio * 20) * self.blue_accent * self.intensity))
            palette.append((r, g, b))
        
        # Yellow to white/blue tips (remaining palette)
        if self.white_edge:
            for i in range(num_colors - len(palette)):
                ratio = i / max(1, (num_colors - len(palette)))
                r = int(min(255, 255 * self.intensity))
                g = int(min(255, 255 * self.intensity))
                b = int(min(255, (100 + ratio * 155) * self.intensity))
                palette.append((r, g, b))
        else:
            # Just extend the yellow palette segment if no white tips
            remaining = num_colors - len(palette)
            if remaining > 0:
                last_color = palette[-1]
                for _ in range(remaining):
                    palette.append(last_color)
                    
        return palette
    
    def _heat_to_color(self, heat_value):
        """Convert a heat value (0-255) to a color from the palette."""
        # Scale heat value to palette index
        index = int((heat_value / 255.0) * (len(self.palette) - 1))
        index = max(0, min(len(self.palette) - 1, index))  # Ensure index is in range
        
        r, g, b = self.palette[index]
        return f"#{r:02x}{g:02x}{b:02x}"  # Convert to hex color string
    
    def _random_between(self, min_val, max_val):
        """Generate a random number between min_val and max_val."""
        return min_val + random.random() * (max_val - min_val)
    
    def _perlin_noise_1d(self, x, offset):
        """Generate a simple 1D noise value."""
        return math.sin(x * 0.1 + offset) * 0.5 + 0.5
    
    def _update_flame_positions(self, time_factor):
        """Update the positions of all flames based on their speeds."""
        for i in range(len(self.flame_positions)):
            # Move flame position based on speed
            self.flame_positions[i] += self.flame_speeds[i] * 0.01 * time_factor
            
            # Handle wrapping around or bouncing based on flow direction
            if self.flow_direction == "right" or self.flow_direction == "both":
                # Wrap around if going off the right edge
                if self.flame_positions[i] > 1.2:
                    self.flame_positions[i] = -0.2
                # Wrap around if going off the left edge
                elif self.flame_positions[i] < -0.2:
                    self.flame_positions[i] = 1.2
            elif self.flow_direction == "left":
                # Wrap around if going off the left edge
                if self.flame_positions[i] < -0.2:
                    self.flame_positions[i] = 1.2
                # Wrap around if going off the right edge
                elif self.flame_positions[i] > 1.2:
                    self.flame_positions[i] = -0.2
            elif self.flow_direction == "center_out":
                # Bounce if reaching the edges
                if self.flame_positions[i] > 1.0 or self.flame_positions[i] < 0.0:
                    self.flame_speeds[i] *= -1
                    self.flame_positions[i] = max(0.0, min(1.0, self.flame_positions[i]))
    
    def _apply_flame_to_heat(self, position, width, height):
        """Apply a flame at the given position with the specified width and height."""
        # Convert position to LED index
        center_led = int(position * self.num_leds)
        
        # Calculate flame width in LEDs
        flame_width_leds = int(width * self.num_leds)
        
        # Apply flame heat to LEDs within range
        for i in range(max(0, center_led - flame_width_leds), min(self.num_leds, center_led + flame_width_leds + 1)):
            # Calculate distance from center (normalized to 0-1)
            distance = abs(i - center_led) / flame_width_leds if flame_width_leds > 0 else 1.0
            
            # Use a smoother falloff function (cosine based)
            if distance <= 1.0:
                # Cosine falloff gives a nice rounded flame shape
                falloff = (math.cos(distance * math.pi) + 1) * 0.5
                
                # Base heat value depends on the flame height
                heat_value = 255.0 * height * falloff
                
                # Apply the heat value to this LED if it's hotter than current
                self.heat[i] = max(self.heat[i], heat_value)
    
    def _apply_edge_fading(self):
        """Apply fading effect to the edges of the strip."""
        # Fade the outer edges
        fade_length = int(self.num_leds * self.outer_fade)
        if fade_length > 0:
            # Left edge fade
            for i in range(min(fade_length, self.num_leds // 2)):
                fade_factor = i / fade_length
                self.heat[i] *= fade_factor
            
            # Right edge fade
            for i in range(max(0, self.num_leds - fade_length), self.num_leds):
                fade_factor = (self.num_leds - 1 - i) / fade_length
                self.heat[i] *= fade_factor
        
        # Fade the inner section if specified
        if self.inner_fade > 0:
            center = self.num_leds // 2
            fade_length = int(self.num_leds * self.inner_fade * 0.5)
            
            if fade_length > 0:
                # Only apply inner fade if we're using center_out flow
                if self.flow_direction == "center_out":
                    for i in range(center - fade_length, center + fade_length):
                        if 0 <= i < self.num_leds:
                            fade_factor = abs(i - center) / fade_length
                            self.heat[i] *= fade_factor
    
    def _apply_flicker(self):
        """Apply random flickering to the entire strip."""
        if random.random() < self.flicker_frequency:
            # Global flicker factor
            flicker_amount = 1.0 - (random.random() * 0.3 * self.flicker_intensity)
            
            # Apply flicker to all LEDs
            for i in range(self.num_leds):
                self.heat[i] *= flicker_amount
                
                # Add some individual LED flicker for more realism
                if random.random() < 0.1:  # 10% chance per LED
                    individual_flicker = 1.0 - (random.random() * 0.4 * self.flicker_intensity)
                    self.heat[i] *= individual_flicker
    
    def _apply_symmetry(self):
        """Make the pattern symmetrical if enabled."""
        if self.symmetrical:
            mid_point = self.num_leds // 2
            for i in range(mid_point):
                mirror_idx = self.num_leds - 1 - i
                # Take the maximum of the two values for a more interesting pattern
                max_value = max(self.heat[i], self.heat[mirror_idx])
                self.heat[i] = max_value
                self.heat[mirror_idx] = max_value
    
    def _update_fire(self, frame_index):
        """Update the fire simulation for one frame."""
        # Calculate time factor for animation
        time_factor = 1.0
        
        # Reset the heat array
        self.heat = [0] * self.num_leds
        
        # Update flame positions
        self._update_flame_positions(time_factor)
        
        # Apply all flames to the heat array
        for i in range(len(self.flame_positions)):
            self._apply_flame_to_heat(
                self.flame_positions[i],
                self.flame_widths[i],
                self.flame_heights[i]
            )
        
        # Apply edge fading
        self._apply_edge_fading()
        
        # Apply flickering
        self._apply_flicker()
        
        # Apply symmetry if enabled
        if self.symmetrical:
            self._apply_symmetry()
    
    def _get_frame(self):
        """Convert the current heat array to a frame of LED colors."""
        leds = []
        for i in range(self.num_leds):
            if self.heat[i] > 0:  # Only include lit LEDs
                leds.append({
                    "index": i,
                    "color": self._heat_to_color(self.heat[i])
                })
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete fire animation."""
        self.frames = []
        
        # Generate frames
        for frame_index in range(self.simulation_steps):
            self._update_fire(frame_index)
            self.frames.append(self._get_frame())
        
        # Create the animation JSON
        animation = {
            "name": "Horizontal Fire Effect",
            "type": "custom",
            "speed": 50,  # 50ms per frame
            "direction": "forward",
            "frames": self.frames
        }
        
        return animation


# Example presets for horizontal LED strips
def generate_headlight_fire_presets():
    # 1. Classic Amber Headlight Fire - like running lights or turn signals
    amber_fire = HorizontalFireSimulation(
        num_leds=60,
        simulation_steps=60,
        flame_width=0.3,
        flame_speed=0.6,
        flame_density=0.6,
        color_temp=1700,           # Amber color
        intensity=1.2,
        red_bias=1.0,
        yellow_bias=1.5,           # More yellow for amber look
        blue_accent=0.0,
        white_edge=False,
        ember_glow=0.2,
        outer_fade=0.1,
        flicker_frequency=0.4,
        flicker_intensity=0.7,
        flow_direction="right",    # Flow from left to right
        symmetrical=False
    )
    amber_animation = amber_fire.generate_animation()
    with open("amber_headlight_fire.json", "w") as f:
        json.dump(amber_animation, f, indent=2)

    # 2. Racing Blue Fire - aggressive sports car look
    blue_fire = HorizontalFireSimulation(
        num_leds=60,
        simulation_steps=60,
        flame_width=0.2,           # Narrower flames
        flame_speed=1.5,           # Faster movement
        flame_density=0.8,         # More flames
        color_temp=2200,           # Hotter blue tint
        intensity=1.4,             # Brighter
        red_bias=0.8,
        yellow_bias=1.0,
        blue_accent=0.25,          # Strong blue accent
        white_edge=True,           # White edges
        ember_glow=0.3,
        outer_fade=0.05,           # Less fade at edges
        flicker_frequency=0.6,     # More flickering
        flicker_intensity=1.2,     # Stronger flickers
        flow_direction="right",    # Flow from left to right
        symmetrical=False          
    )
    blue_animation = blue_fire.generate_animation()
    with open("blue_racing_fire.json", "w") as f:
        json.dump(blue_animation, f, indent=2)

    # 3. Dual-Direction Pulse - fire flows from both sides
    dual_fire = HorizontalFireSimulation(
        num_leds=60,
        simulation_steps=60,
        flame_width=0.25,
        flame_speed=0.7,
        flame_density=0.8,
        color_temp=1900,
        intensity=1.1,
        red_bias=1.2,
        yellow_bias=0.9,
        blue_accent=0.05,
        white_edge=True,
        ember_glow=0.4,
        outer_fade=0.05,
        inner_fade=0.0,           # No center fade
        flicker_frequency=0.5,
        flicker_intensity=0.8,
        flow_direction="both",    # Flows in both directions
        symmetrical=False
    )
    dual_animation = dual_fire.generate_animation()
    with open("dual_direction_fire.json", "w") as f:
        json.dump(dual_animation, f, indent=2)

    # 4. Center-Out Symmetrical - fire flows outward from center (like knight rider)
    center_fire = HorizontalFireSimulation(
        num_leds=60,
        simulation_steps=60,
        flame_width=0.3,
        flame_speed=0.8,
        flame_density=0.7,
        color_temp=2000,
        intensity=1.3,
        red_bias=1.1,
        yellow_bias=1.0,
        blue_accent=0.1,
        white_edge=True,
        ember_glow=0.3,
        outer_fade=0.1,
        inner_fade=0.5,           # Strong center fade
        flicker_frequency=0.5,
        flicker_intensity=0.9,
        flow_direction="center_out", # Flow from center outward
        symmetrical=True          # Perfect symmetry
    )
    center_animation = center_fire.generate_animation()
    with open("center_out_fire.json", "w") as f:
        json.dump(center_animation, f, indent=2)

    print("Generated 4 different horizontal fire animations for headlight applications:")
    print("1. Amber Headlight Fire")
    print("2. Blue Racing Fire")
    print("3. Dual-Direction Fire")
    print("4. Center-Out Symmetrical Fire")

# Run the generation
generate_headlight_fire_presets()