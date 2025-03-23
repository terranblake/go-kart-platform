import random
import json
import math
import time
from collections import deque

class Fire2DSimulation:
    def __init__(self, width=60, height=2, simulation_steps=60,
                 flame_width=0.4, flame_height=0.6, flame_speed=0.5, flame_density=0.7, 
                 color_temp=1800, intensity=1.0, color_palette_size=100,
                 red_bias=1.0, yellow_bias=1.0, blue_accent=0.0, white_edge=True,
                 ember_glow=0.3, outer_fade=0.2, inner_fade=0.6,
                 flicker_frequency=0.7, flicker_intensity=1.0,
                 flow_direction="right", symmetrical=False, vertical_movement=True):
        """
        Initialize the 2D fire simulation for LED matrices and multi-row strips.
        
        Parameters:
        - width: Width of the LED matrix/strip
        - height: Height of the LED matrix/strip
        - simulation_steps: Number of frames to generate
        - flame_width: Width of flame "tongues" (0.1-1.0)
        - flame_height: Height of flames (0.1-1.0)
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
        - vertical_movement: Whether flames can move vertically (boolean)
        """
        self.width = width
        self.height = height
        self.heat = [[0 for _ in range(width)] for _ in range(height)]
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Flame pattern parameters
        self.flame_width = max(0.1, min(1.0, flame_width))
        self.flame_height = max(0.1, min(1.0, flame_height))
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
        
        # 2D-specific parameters
        self.outer_fade = max(0.0, min(1.0, outer_fade))
        self.inner_fade = max(0.0, min(1.0, inner_fade))
        self.flicker_frequency = max(0.0, min(1.0, flicker_frequency))
        self.flicker_intensity = max(0.5, min(2.0, flicker_intensity))
        self.flow_direction = flow_direction
        self.symmetrical = symmetrical
        self.vertical_movement = vertical_movement
        
        # Initialize noise parameters for flame movement
        self.noise_offsets = [random.random() * 100 for _ in range(4)]
        
        # Create color palette from deep red to bright yellow
        self.palette = self._create_fire_palette(color_palette_size)
        
        # Generate flame wave patterns based on the density
        num_flames = int(max(2, self.flame_density * 10))
        
        # For 2D, we store both x and y coordinates
        self.flame_positions = []  # Each position is a tuple (x, y)
        self.flame_speeds = []     # Each speed is a tuple (x_speed, y_speed)
        self.flame_intensities = []  # How bright each flame is
        self.flame_sizes = []      # Each size is a tuple (width, height)
        
        for _ in range(num_flames):
            # Initial X position based on flow direction
            if self.flow_direction == "right":
                x_pos = random.random() * 0.3  # Start at left side
            elif self.flow_direction == "left":
                x_pos = 0.7 + random.random() * 0.3  # Start at right side
            elif self.flow_direction == "both":
                x_pos = random.random()  # Start anywhere
            elif self.flow_direction == "center_out":
                x_pos = 0.5  # Start in center
            else:
                x_pos = random.random()  # Default: start anywhere
            
            # Initial Y position - start at bottom or random for complex patterns
            if height > 1:
                y_pos = random.random() * 0.4  # Start in bottom area
            else:
                y_pos = 0.0  # Only one row, so y is 0
            
            self.flame_positions.append((x_pos, y_pos))
            
            # Determine X speed based on flow direction
            base_x_speed = self.flame_speed * (0.7 + random.random() * 0.6)
            if self.flow_direction == "right":
                x_speed = base_x_speed
            elif self.flow_direction == "left":
                x_speed = -base_x_speed
            elif self.flow_direction == "both":
                x_speed = base_x_speed * random.choice([-1, 1])
            elif self.flow_direction == "center_out":
                x_speed = base_x_speed * (1 if x_pos >= 0.5 else -1)
            else:
                x_speed = base_x_speed * random.choice([-1, 1])  # Default: random direction
            
            # Determine Y speed if vertical movement is enabled
            if self.vertical_movement and height > 1:
                # Flames tend to move upward but with some randomness
                y_speed = self.flame_speed * (0.1 + random.random() * 0.3)
            else:
                y_speed = 0  # No vertical movement
            
            self.flame_speeds.append((x_speed, y_speed))
            
            # Random intensity for each flame
            self.flame_intensities.append(0.7 + random.random() * 0.3)
            
            # Random size for each flame
            flame_width_val = self.flame_width * (0.5 + random.random() * 1.0)
            flame_height_val = self.flame_height * (0.5 + random.random() * 1.0) if height > 1 else 1.0
            self.flame_sizes.append((flame_width_val, flame_height_val))
    
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
    
    def _update_flame_positions(self, time_factor):
        """Update the positions of all flames based on their speeds."""
        for i in range(len(self.flame_positions)):
            # Unpack current position and speed
            x_pos, y_pos = self.flame_positions[i]
            x_speed, y_speed = self.flame_speeds[i]
            
            # Move flame position based on speed
            new_x = x_pos + x_speed * 0.01 * time_factor
            new_y = y_pos + y_speed * 0.01 * time_factor if self.vertical_movement else y_pos
            
            # Handle x-direction wrapping or bouncing based on flow direction
            if self.flow_direction == "right" or self.flow_direction == "both":
                # Wrap around if going off the right edge
                if new_x > 1.2:
                    new_x = -0.2
                # Wrap around if going off the left edge
                elif new_x < -0.2:
                    new_x = 1.2
            elif self.flow_direction == "left":
                # Wrap around if going off the left edge
                if new_x < -0.2:
                    new_x = 1.2
                # Wrap around if going off the right edge
                elif new_x > 1.2:
                    new_x = -0.2
            elif self.flow_direction == "center_out":
                # Bounce if reaching the edges
                if new_x > 1.0 or new_x < 0.0:
                    self.flame_speeds[i] = (-x_speed, y_speed)
                    new_x = max(0.0, min(1.0, new_x))
            
            # Handle y-direction behavior
            if self.height > 1 and self.vertical_movement:
                # If flame reaches top, reset to bottom with new properties
                if new_y > 1.0:
                    new_y = random.random() * 0.4  # Random position near bottom
                    # Randomize flame characteristics for variation
                    self.flame_intensities[i] = 0.7 + random.random() * 0.3
                    w, h = self.flame_sizes[i]
                    self.flame_sizes[i] = (
                        self.flame_width * (0.5 + random.random() * 1.0),
                        self.flame_height * (0.5 + random.random() * 1.0)
                    )
            
            # Update position
            self.flame_positions[i] = (new_x, new_y)
    
    def _apply_flame_to_heat(self, flame_index):
        """Apply a flame to the heat array based on its properties."""
        # Get flame properties
        x_pos, y_pos = self.flame_positions[flame_index]
        width, height = self.flame_sizes[flame_index]
        intensity = self.flame_intensities[flame_index]
        
        # Convert position to grid coordinates
        center_x = int(x_pos * self.width)
        center_y = int(y_pos * self.height)
        
        # Calculate flame dimensions in grid units
        flame_width_px = max(1, int(width * self.width / 2))
        flame_height_px = max(1, int(height * self.height / 2))
        
        # Calculate region affected by this flame
        x_min = max(0, center_x - flame_width_px)
        x_max = min(self.width - 1, center_x + flame_width_px)
        y_min = max(0, center_y - flame_height_px)
        y_max = min(self.height - 1, center_y + flame_height_px)
        
        # Apply flame heat to cells within range
        for y in range(y_min, y_max + 1):
            for x in range(x_min, x_max + 1):
                # Calculate distance from center (normalized)
                x_dist = abs(x - center_x) / max(1, flame_width_px)
                y_dist = abs(y - center_y) / max(1, flame_height_px)
                
                # Calculate radial distance for a circular/elliptical flame shape
                dist = math.sqrt(x_dist**2 + y_dist**2) / math.sqrt(2)
                
                # Use a smooth falloff function
                if dist <= 1.0:
                    # Cosine falloff for a natural flame shape
                    falloff = (math.cos(dist * math.pi) + 1) * 0.5
                    
                    # Adjust height-based intensity for vertical flames
                    height_factor = 1.0
                    if self.height > 1 and self.vertical_movement:
                        # Flames are hotter in middle/bottom and cooler at top
                        rel_height = y / self.height
                        height_factor = 1.0 - rel_height * 0.5
                    
                    # Calculate heat value
                    heat_value = 255.0 * intensity * falloff * height_factor
                    
                    # Apply heat value if it's hotter than current
                    self.heat[y][x] = max(self.heat[y][x], heat_value)
    
    def _apply_edge_fading(self):
        """Apply fading effects to the edges of the 2D grid."""
        # Horizontal edge fading (left and right)
        h_fade_length = int(self.width * self.outer_fade)
        if h_fade_length > 0:
            for y in range(self.height):
                # Left edge fade
                for x in range(min(h_fade_length, self.width // 2)):
                    fade_factor = x / h_fade_length
                    self.heat[y][x] *= fade_factor
                
                # Right edge fade
                for x in range(max(0, self.width - h_fade_length), self.width):
                    fade_factor = (self.width - 1 - x) / h_fade_length
                    self.heat[y][x] *= fade_factor
        
        # Vertical edge fading (top and bottom) if height > 1
        if self.height > 1:
            v_fade_length = int(self.height * self.outer_fade)
            if v_fade_length > 0:
                # Bottom edge fade (softer for rising flames)
                for y in range(min(v_fade_length // 2, self.height // 4)):
                    fade_factor = y / (v_fade_length // 2)
                    for x in range(self.width):
                        self.heat[y][x] *= fade_factor
                
                # Top edge fade (stronger for dying flames)
                for y in range(max(0, self.height - v_fade_length), self.height):
                    fade_factor = (self.height - 1 - y) / v_fade_length
                    for x in range(self.width):
                        self.heat[y][x] *= fade_factor
        
        # Inner horizontal fade if center_out is enabled
        if self.flow_direction == "center_out" and self.inner_fade > 0:
            center_x = self.width // 2
            fade_length = int(self.width * self.inner_fade * 0.5)
            
            if fade_length > 0:
                for y in range(self.height):
                    for x in range(center_x - fade_length, center_x + fade_length):
                        if 0 <= x < self.width:
                            fade_factor = abs(x - center_x) / fade_length
                            self.heat[y][x] *= fade_factor
    
    def _apply_flicker(self):
        """Apply random flickering to the entire grid."""
        if random.random() < self.flicker_frequency:
            # Global flicker factor
            flicker_amount = 1.0 - (random.random() * 0.3 * self.flicker_intensity)
            
            # Apply flicker to all cells
            for y in range(self.height):
                for x in range(self.width):
                    self.heat[y][x] *= flicker_amount
                    
                    # Add some individual cell flicker for more realism
                    if random.random() < 0.05:  # 5% chance per cell
                        individual_flicker = 1.0 - (random.random() * 0.4 * self.flicker_intensity)
                        self.heat[y][x] *= individual_flicker
    
    def _apply_symmetry(self):
        """Make the pattern symmetrical horizontally if enabled."""
        if self.symmetrical:
            mid_point = self.width // 2
            for y in range(self.height):
                for x in range(mid_point):
                    mirror_x = self.width - 1 - x
                    # Take the maximum of the two values for an interesting pattern
                    max_value = max(self.heat[y][x], self.heat[y][mirror_x])
                    self.heat[y][x] = max_value
                    self.heat[y][mirror_x] = max_value
    
    def _update_fire(self, frame_index):
        """Update the fire simulation for one frame."""
        # Calculate time factor for animation
        time_factor = 1.0
        
        # Reset the heat array
        self.heat = [[0 for _ in range(self.width)] for _ in range(self.height)]
        
        # Update flame positions
        self._update_flame_positions(time_factor)
        
        # Apply all flames to the heat array
        for i in range(len(self.flame_positions)):
            self._apply_flame_to_heat(i)
        
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
        for y in range(self.height):
            for x in range(self.width):
                if self.heat[y][x] > 0:  # Only include lit cells
                    # Add LED using 2D x,y coordinates
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": self._heat_to_color(self.heat[y][x])
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
            "name": "2D Fire Effect",
            "type": "custom",
            "speed": 50,  # 50ms per frame
            "direction": "forward",
            "dimensions": {
                "length": self.width,
                "height": self.height
            },
            "frames": self.frames
        }
        
        return animation


# Example presets for 2D fire effects
def generate_2d_fire_presets():
    # 1. Wide Strip Fire (60x2) - horizontal flames with vertical movement
    wide_strip_fire = Fire2DSimulation(
        width=60,
        height=2,
        simulation_steps=60,
        flame_width=0.3,
        flame_height=0.8,
        flame_speed=0.6,
        flame_density=0.7,
        color_temp=1800,
        intensity=1.2,
        red_bias=1.1,
        yellow_bias=1.2,
        blue_accent=0.05,
        white_edge=True,
        ember_glow=0.3,
        outer_fade=0.1,
        flicker_frequency=0.5,
        flicker_intensity=0.8,
        flow_direction="right",
        symmetrical=False,
        vertical_movement=True
    )
    wide_strip_animation = wide_strip_fire.generate_animation()
    with open("wide_strip_fire_60x2.json", "w") as f:
        json.dump(wide_strip_animation, f, indent=2)

    # 2. Matrix Fire (7x4) - more like actual flames rising
    matrix_fire = Fire2DSimulation(
        width=7,
        height=4,
        simulation_steps=60,
        flame_width=0.35,
        flame_height=0.9,
        flame_speed=0.7,
        flame_density=0.8,
        color_temp=1700,  # Warmer/redder
        intensity=1.3,
        red_bias=1.2,
        yellow_bias=1.0,
        blue_accent=0.0,
        white_edge=False,  # No white tips for more natural fire
        ember_glow=0.4,
        outer_fade=0.15,
        flicker_frequency=0.6,
        flicker_intensity=1.0,
        flow_direction="both",  # Flames move in both directions
        symmetrical=False,
        vertical_movement=True
    )
    matrix_animation = matrix_fire.generate_animation()
    with open("matrix_fire_7x4.json", "w") as f:
        json.dump(matrix_animation, f, indent=2)

    # 3. Symmetrical Center Fire (16x8) - for larger displays with symmetrical pattern
    center_fire = Fire2DSimulation(
        width=16,
        height=8,
        simulation_steps=60,
        flame_width=0.25,
        flame_height=0.7,
        flame_speed=0.8,
        flame_density=0.9,
        color_temp=2000,
        intensity=1.4,
        red_bias=1.0,
        yellow_bias=1.1,
        blue_accent=0.1,
        white_edge=True,
        ember_glow=0.3,
        outer_fade=0.05,
        flicker_frequency=0.5,
        flicker_intensity=0.9,
        flow_direction="center_out",
        symmetrical=True,  # Perfect symmetry
        vertical_movement=True
    )
    center_animation = center_fire.generate_animation()
    with open("center_symmetrical_fire_16x8.json", "w") as f:
        json.dump(center_animation, f, indent=2)

    # 4. Blue Racing Fire Pillars (30x2) - for car underbody lighting
    racing_fire = Fire2DSimulation(
        width=30,
        height=2,
        simulation_steps=60,
        flame_width=0.2,
        flame_height=0.6,
        flame_speed=1.5,  # Fast movement
        flame_density=1.0,  # Lots of flames
        color_temp=2200,
        intensity=1.5,
        red_bias=0.7,
        yellow_bias=0.8,
        blue_accent=0.25,  # Strong blue accent
        white_edge=True,
        ember_glow=0.2,
        outer_fade=0.0,  # No edge fading
        flicker_frequency=0.7,
        flicker_intensity=1.2,
        flow_direction="right",
        symmetrical=False,
        vertical_movement=True
    )
    racing_animation = racing_fire.generate_animation()
    with open("blue_racing_fire_30x2.json", "w") as f:
        json.dump(racing_animation, f, indent=2)

    print("Generated 4 different 2D fire animations:")
    print("1. Wide Strip Fire (60x2)")
    print("2. Matrix Fire (7x4)")
    print("3. Symmetrical Center Fire (16x8)")
    print("4. Blue Racing Fire (30x2)")

# Run the generation
generate_2d_fire_presets()