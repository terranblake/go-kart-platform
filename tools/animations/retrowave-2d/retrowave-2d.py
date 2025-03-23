import random
import json
import math
import colorsys

class RetroWaveGenerator2D:
    def __init__(self, 
                 width=60,               # Grid width (x dimension)
                 height=2,               # Grid height (y dimension)
                 simulation_steps=60,
                 # Color parameters
                 primary_color="#ff00ff",    # Hot pink
                 secondary_color="#00ffff",  # Cyan
                 accent_color="#ffff00",     # Yellow
                 background_color="#000000", # Black
                 color_mode="fixed",         # fixed, gradient, pulse
                 
                 # Pattern parameters
                 pattern_type="stripes",     # stripes, grid, scan, blocks
                 pattern_speed=1.0,          # 0.1-5.0, Speed multiplier
                 pattern_density=0.5,        # 0.1-1.0, How dense the pattern is
                 pattern_width=0.3,          # 0.05-1.0, Width of pattern elements
                 direction="forward",        # forward, backward, both, bounce
                 orientation="horizontal",   # horizontal, vertical, both
                 
                 # Effect parameters
                 glow_amount=0.3,            # 0.0-1.0, How much "glow" effect
                 strobe_frequency=0.0,       # 0.0-1.0, 0 = no strobe
                 scanline_enabled=False,     # Enable scanning line effect
                 symmetrical=False,          # Make pattern symmetrical
                 
                 # Sun grid effect (optional)
                 sun_enabled=False,          # Enable sun grid in background
                 sun_size=0.3,               # Size of sun
                 sun_color="#ff5500"         # Color of sun
                ):
        """
        Initialize the 2D RetroWave animation generator.
        """
        self.width = width
        self.height = height
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Store all parameters as attributes
        self.primary_color = self._parse_color(primary_color)
        self.secondary_color = self._parse_color(secondary_color)
        self.accent_color = self._parse_color(accent_color)
        self.background_color = self._parse_color(background_color)
        self.color_mode = color_mode
        
        self.pattern_type = pattern_type
        self.pattern_speed = max(0.1, min(5.0, pattern_speed))
        self.pattern_density = max(0.1, min(1.0, pattern_density))
        self.pattern_width = max(0.05, min(1.0, pattern_width))
        self.direction = direction
        self.orientation = orientation
        
        self.glow_amount = max(0.0, min(1.0, glow_amount))
        self.strobe_frequency = max(0.0, min(1.0, strobe_frequency))
        self.scanline_enabled = scanline_enabled
        self.symmetrical = symmetrical
        
        self.sun_enabled = sun_enabled
        self.sun_size = max(0.1, min(1.0, sun_size))
        self.sun_color = self._parse_color(sun_color)
        
        # Generate color palette
        self.color_palette = [self.primary_color, self.secondary_color, self.accent_color]
        
        # Internal state tracking
        self.current_time = 0
        self.strobe_state = False
    
    def _parse_color(self, color_str):
        """Convert hex color string to RGB tuple."""
        if color_str.startswith('#'):
            color_str = color_str[1:]
        return (
            int(color_str[0:2], 16) / 255.0,
            int(color_str[2:4], 16) / 255.0,
            int(color_str[4:6], 16) / 255.0
        )
    
    def _color_to_hex(self, color):
        """Convert RGB tuple to hex color string."""
        r = int(max(0, min(1, color[0])) * 255)
        g = int(max(0, min(1, color[1])) * 255)
        b = int(max(0, min(1, color[2])) * 255)
        return f"#{r:02x}{g:02x}{b:02x}"
    
    def _apply_glow(self, color, amount):
        """Apply a glow effect by increasing brightness and reducing saturation."""
        h, s, v = colorsys.rgb_to_hsv(*color)
        glow_s = max(0, s - (amount * 0.5))
        glow_v = min(1.0, v + (amount * 0.5))
        return colorsys.hsv_to_rgb(h, glow_s, glow_v)
    
    def _get_color_for_position(self, position, time):
        """Get the color for a specific position based on current time and effects."""
        # Determine color index based on position and mode
        if self.color_mode == "fixed":
            color_index = int(position * len(self.color_palette)) % len(self.color_palette)
        else:
            # For gradient and pulse, calculate based on time
            time_factor = time * self.pattern_speed * 0.2
            color_index = int((position + time_factor) * len(self.color_palette)) % len(self.color_palette)
        
        # Get base color
        color = self.color_palette[color_index]
        
        # Apply color modes
        if self.color_mode == "gradient":
            # Blend between this color and the next
            next_color_index = (color_index + 1) % len(self.color_palette)
            next_color = self.color_palette[next_color_index]
            
            # Calculate blend factor
            blend_pos = (position * len(self.color_palette)) % 1.0
            
            # Linear interpolation between colors
            color = (
                color[0] * (1 - blend_pos) + next_color[0] * blend_pos,
                color[1] * (1 - blend_pos) + next_color[1] * blend_pos,
                color[2] * (1 - blend_pos) + next_color[2] * blend_pos
            )
        elif self.color_mode == "pulse":
            # Pulsing effect between colors
            pulse_factor = (math.sin(time * self.pattern_speed * math.pi) * 0.5 + 0.5)
            pulse_factor = 0.3 + pulse_factor * 0.7  # Keep minimum brightness
            
            # Apply pulse factor
            color = (
                color[0] * pulse_factor,
                color[1] * pulse_factor,
                color[2] * pulse_factor
            )
        
        # Apply glow if enabled
        if self.glow_amount > 0:
            color = self._apply_glow(color, self.glow_amount)
        
        return color
    
    def _calculate_grid_pattern(self, x_pos, y_pos, time):
        """Calculate grid pattern intensity for position and time."""
        # Use appropriate position based on orientation
        if self.orientation == "horizontal":
            position = x_pos
        elif self.orientation == "vertical":
            position = y_pos
        else:  # both - use both coordinates
            position = (x_pos + y_pos) / 2
        
        # Scale position based on density
        grid_size = 3 + int(self.pattern_density * 7)  # 3-10 cells based on density
        grid_position = position * grid_size
        
        # Add time-based movement
        if self.direction == "forward":
            grid_position += time * self.pattern_speed
        elif self.direction == "backward":
            grid_position -= time * self.pattern_speed
        elif self.direction == "bounce":
            grid_position += math.sin(time * self.pattern_speed) * 2
        
        # Calculate grid cell
        cell = grid_position % 1.0
        
        # Determine if we're in a gap or not
        in_gap = cell > self.pattern_width
        
        # Calculate intensity (0 in gap, 1 in grid)
        intensity = 0.0 if in_gap else 1.0
        
        return intensity
    
    def _calculate_stripe_pattern(self, x_pos, y_pos, time):
        """Calculate stripe pattern intensity for position and time."""
        # Use appropriate position based on orientation
        if self.orientation == "horizontal":
            position = x_pos
            num_elements = self.width
        elif self.orientation == "vertical":
            position = y_pos
            num_elements = self.height
        else:  # both - create diagonal stripes
            position = (x_pos + y_pos) / 2
            num_elements = max(self.width, self.height)
        
        # Calculate number of stripes based on density
        num_stripes = max(2, int(num_elements * self.pattern_density / 5))
        
        # Scale position based on number of stripes
        stripe_position = position * num_stripes
        
        # Add time-based movement
        if self.direction == "forward":
            stripe_position += time * self.pattern_speed
        elif self.direction == "backward":
            stripe_position -= time * self.pattern_speed
        elif self.direction == "bounce":
            stripe_position += math.sin(time * self.pattern_speed) * 2
        
        # Calculate stripe position within cycle
        pos_in_cycle = stripe_position % 1.0
        
        # Determine if we're in a stripe or not
        in_stripe = pos_in_cycle < self.pattern_width
        
        # Calculate intensity (0 off, 1 in stripe)
        intensity = 1.0 if in_stripe else 0.0
        
        return intensity
    
    def _calculate_scan_pattern(self, x_pos, y_pos, time):
        """Calculate scanning line pattern intensity for position and time."""
        # Use appropriate position based on orientation
        if self.orientation == "horizontal":
            position = x_pos
        elif self.orientation == "vertical":
            position = y_pos
        else:  # both - use both coordinates with phase difference
            position = (x_pos + y_pos) / 2
        
        # Calculate scan position (0-1 across the grid)
        if self.direction == "forward":
            scan_position = (time * self.pattern_speed * 0.2) % 1.0
        elif self.direction == "backward":
            scan_position = 1.0 - ((time * self.pattern_speed * 0.2) % 1.0)
        elif self.direction == "bounce":
            scan_position = (math.sin(time * self.pattern_speed * 0.2 * math.pi) + 1) * 0.5
        else:  # both
            scan_position1 = (time * self.pattern_speed * 0.2) % 1.0
            scan_position2 = 1.0 - scan_position1
            
            # Calculate distance to both scan positions
            dist1 = abs(position - scan_position1)
            dist2 = abs(position - scan_position2)
            distance = min(dist1, dist2)
            
            # Calculate intensity based on distance to scan line
            if distance < self.pattern_width:
                intensity = 1.0 - (distance / self.pattern_width)
                return intensity
            return 0.0
        
        # Calculate distance to scan position
        distance = abs(position - scan_position)
        
        # Calculate intensity based on distance to scan line
        if distance < self.pattern_width:
            intensity = 1.0 - (distance / self.pattern_width)
            return intensity
        
        return 0.0
    
    def _calculate_blocks_pattern(self, x_pos, y_pos, time):
        """Calculate blocks pattern intensity for position and time."""
        # Use appropriate position and dimensions based on orientation
        if self.orientation == "horizontal":
            position = x_pos
            dimension = self.width
        elif self.orientation == "vertical":
            position = y_pos
            dimension = self.height
        else:  # both - create checkerboard
            x_scaled = x_pos * (self.width / max(self.width, self.height))
            y_scaled = y_pos * (self.height / max(self.width, self.height))
            
            # For checkerboard, we use both x and y
            x_block = math.floor((x_scaled * 10 * self.pattern_density + time * self.pattern_speed) % 2)
            y_block = math.floor((y_scaled * 10 * self.pattern_density + time * self.pattern_speed) % 2)
            
            # XOR pattern for checkerboard
            return 1.0 if (x_block ^ y_block) else 0.0
        
        # Number of blocks based on density
        num_blocks = max(2, int(dimension * self.pattern_density / 3))
        
        # Width of each block
        block_width = 0.7 / num_blocks
        
        # Movement speed
        if self.direction == "forward":
            offset = time * self.pattern_speed * 0.2
        elif self.direction == "backward":
            offset = -time * self.pattern_speed * 0.2
        elif self.direction == "bounce":
            offset = math.sin(time * self.pattern_speed * 0.2) * 0.5
        else:  # both
            block_phase = ((position * num_blocks) % 1.0) * math.pi * 2
            offset = math.sin(time * self.pattern_speed * 0.2 + block_phase) * 0.5
        
        # Block pattern
        pos_in_pattern = ((position + offset) * num_blocks) % 1.0
        
        # Determine if in a block or gap
        in_block = pos_in_pattern < block_width * self.pattern_width
        
        intensity = 1.0 if in_block else 0.0
        
        return intensity
    
    def _apply_pattern(self, x_pos, y_pos, time):
        """Apply the selected pattern to determine intensity at position and time."""
        if self.pattern_type == "grid":
            return self._calculate_grid_pattern(x_pos, y_pos, time)
        elif self.pattern_type == "stripes":
            return self._calculate_stripe_pattern(x_pos, y_pos, time)
        elif self.pattern_type == "scan":
            return self._calculate_scan_pattern(x_pos, y_pos, time)
        elif self.pattern_type == "blocks":
            return self._calculate_blocks_pattern(x_pos, y_pos, time)
        else:
            # Default to stripes if unknown pattern
            return self._calculate_stripe_pattern(x_pos, y_pos, time)
    
    def _apply_sun(self, x_pos, y_pos, time):
        """Apply sun grid effect to determine color contribution."""
        if not self.sun_enabled:
            return None
        
        # Use appropriate position based on orientation
        if self.orientation == "horizontal":
            position = x_pos
        elif self.orientation == "vertical":
            position = y_pos
        else:  # both - use x position for sun
            position = x_pos
        
        # Sun positioned in the center by default
        sun_position = 0.5
        
        # Calculate distance from sun position
        distance = abs(position - sun_position)
        
        # Check if in sun circle
        if distance < self.sun_size / 2:
            # Inside sun circle
            dist_factor = distance / (self.sun_size / 2)
            intensity = 1.0 - dist_factor * dist_factor
            return self.sun_color
        
        # Check if on a sun ray
        if distance < self.sun_size:
            # Calculate angle in radians
            angle = abs(position - sun_position) / self.sun_size * math.pi * 2
            
            # Check if near a ray line (8 rays)
            for i in range(8):
                ray_angle = i * (math.pi * 2 / 8)
                angle_diff = abs((angle - ray_angle) % (math.pi * 2))
                if angle_diff > math.pi:
                    angle_diff = math.pi * 2 - angle_diff
                
                if angle_diff < 0.1:  # Threshold for ray width
                    # Fade based on distance from sun
                    fade_factor = (self.sun_size - distance) / self.sun_size
                    intensity = fade_factor * 0.7
                    return self.sun_color
        
        return None
    
    def _apply_scanline(self, x_pos, y_pos, time):
        """Apply scanline effect to brighten a moving line."""
        if not self.scanline_enabled:
            return 1.0  # No effect
        
        # Use appropriate position based on orientation
        if self.orientation == "horizontal":
            position = x_pos
        elif self.orientation == "vertical":
            position = y_pos
        else:  # both - use x position for scanline
            position = x_pos
        
        # Calculate scanline position (0-1 across the strip)
        scanline_speed = self.pattern_speed * 0.5  # Separate speed for scanline
        scanline_position = (time * scanline_speed) % 1.0
        
        # Calculate distance to the scanline
        distance = min(abs(position - scanline_position), 
                       abs(position - (scanline_position - 1.0)),
                       abs(position - (scanline_position + 1.0)))
        
        # Determine scanline effect
        scanline_width = 0.05  # Width of the scanline
        if distance < scanline_width:
            effect = 1.0 + (1.0 - distance / scanline_width) * 0.5
            return effect
        
        return 1.0  # No effect
    
    def _apply_strobe(self, frame_index):
        """Determine if strobe effect should be active for this frame."""
        if self.strobe_frequency <= 0:
            return False
        
        # Simple strobe implementation - alternating frames
        strobe_period = int(1.0 / self.strobe_frequency * 10)
        return frame_index % strobe_period < 2  # 2 frames on, rest off
    
    def _render_frame(self, frame_index):
        """Render a single frame of the animation."""
        time = frame_index / 10.0  # Time in arbitrary units
        leds = []
        
        # Check for strobe effect
        strobe_active = self._apply_strobe(frame_index)
        if strobe_active:
            # Full brightness all LEDs during strobe
            for y in range(self.height):
                for x in range(self.width):
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": "#ffffff"  # White strobe
                    })
            return {"leds": leds}
        
        # Normal rendering - iterate through the 2D grid
        for y in range(self.height):
            for x in range(self.width):
                # Normalize positions to 0-1 range
                x_pos = x / (self.width - 1) if self.width > 1 else 0.5
                y_pos = y / (self.height - 1) if self.height > 1 else 0.5
                
                # If symmetrical, mirror around center
                if self.symmetrical:
                    if x_pos > 0.5:
                        x_pos = 1.0 - x_pos
                    if y_pos > 0.5:
                        y_pos = 1.0 - y_pos
                    # Scale to keep full range
                    x_pos = x_pos * 2
                    y_pos = y_pos * 2
                
                # Get base pattern intensity
                intensity = self._apply_pattern(x_pos, y_pos, time)
                
                # Skip if no intensity
                if intensity <= 0:
                    continue
                
                # Use the appropriate position for color based on orientation
                if self.orientation == "horizontal":
                    color_position = x_pos
                elif self.orientation == "vertical":
                    color_position = y_pos
                else:  # both - average positions
                    color_position = (x_pos + y_pos) / 2
                
                # Get color for this position
                color = self._get_color_for_position(color_position, time)
                
                # Apply sun effect if enabled
                sun_color = self._apply_sun(x_pos, y_pos, time)
                if sun_color is not None:
                    # Blend with sun color
                    color = (
                        color[0] * 0.3 + sun_color[0] * 0.7,
                        color[1] * 0.3 + sun_color[1] * 0.7,
                        color[2] * 0.3 + sun_color[2] * 0.7
                    )
                
                # Apply scanline effect if enabled
                intensity_mult = self._apply_scanline(x_pos, y_pos, time)
                
                # Apply intensity to color
                final_color = (
                    min(1.0, color[0] * intensity * intensity_mult),
                    min(1.0, color[1] * intensity * intensity_mult),
                    min(1.0, color[2] * intensity * intensity_mult)
                )
                
                # Only add LEDs that are on
                if max(final_color) > 0:
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": self._color_to_hex(final_color)
                    })
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete retro wave animation."""
        # Clear any existing frames
        self.frames = []
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            frame = self._render_frame(frame_index)
            self.frames.append(frame)
        
        # Create the animation JSON with dimensions
        animation = {
            "name": "RetroWave Animation",
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


# Example presets for different grid sizes
def generate_retrowave_presets():
    # 1. 60x1 Horizontal - Classic Pink/Blue Neon Stripes
    horizontal_strip = RetroWaveGenerator2D(
        width=60,
        height=1,
        simulation_steps=60,
        primary_color="#ff00ff",    # Hot pink
        secondary_color="#00ffff",  # Cyan
        accent_color="#ffff00",     # Yellow
        color_mode="fixed",
        pattern_type="stripes",
        pattern_speed=0.7,
        pattern_density=0.6,
        pattern_width=0.7,
        direction="forward",
        orientation="horizontal",
        glow_amount=0.5,
        scanline_enabled=True
    )
    h_animation = horizontal_strip.generate_animation()
    with open("retrowave_60x1.json", "w") as f:
        json.dump(h_animation, f, indent=2)

    # 2. 30x2 Horizontal Grid - Two-row stripes
    two_row = RetroWaveGenerator2D(
        width=30,
        height=2,
        simulation_steps=60,
        primary_color="#ff00ff",    # Hot pink
        secondary_color="#00ffff",  # Cyan
        accent_color="#ffff00",     # Yellow
        color_mode="gradient",
        pattern_type="stripes",
        pattern_speed=1.0,
        pattern_density=0.7,
        pattern_width=0.5,
        direction="forward",
        orientation="vertical",     # Vertical orientation for horizontal rows
        glow_amount=0.4,
        scanline_enabled=False
    )
    two_row_animation = two_row.generate_animation()
    with open("retrowave_30x2.json", "w") as f:
        json.dump(two_row_animation, f, indent=2)

    # 3. 15x4 Checkerboard - Four-row with checkerboard pattern
    four_row = RetroWaveGenerator2D(
        width=15,
        height=4,
        simulation_steps=60,
        primary_color="#ff3366",    # Hot pink
        secondary_color="#33ccff",  # Light blue
        accent_color="#ffcc00",     # Gold
        color_mode="pulse",
        pattern_type="blocks",
        pattern_speed=0.5,
        pattern_density=0.8,
        pattern_width=0.5,
        direction="both",
        orientation="both",         # Use both dimensions for checkerboard
        glow_amount=0.3,
        strobe_frequency=0.0
    )
    four_row_animation = four_row.generate_animation()
    with open("retrowave_15x4.json", "w") as f:
        json.dump(four_row_animation, f, indent=2)
        
    # 4. 5x6 Box - Sun grid with matrix-like effects
    box = RetroWaveGenerator2D(
        width=5,
        height=6,
        simulation_steps=60,
        primary_color="#ff6633",    # Orange
        secondary_color="#00ff99",  # Mint
        accent_color="#cc33ff",     # Purple
        color_mode="gradient",
        pattern_type="scan",
        pattern_speed=0.8,
        pattern_density=0.5,
        pattern_width=0.3,
        direction="bounce",
        orientation="both",
        glow_amount=0.5,
        sun_enabled=True,
        sun_size=0.7,
        sun_color="#ff3300"
    )
    box_animation = box.generate_animation()
    with open("retrowave_5x6.json", "w") as f:
        json.dump(box_animation, f, indent=2)

    # 4. 5x6 Box - Sun grid with matrix-like effects
    box = RetroWaveGenerator2D(
        width=60,
        height=10,
        simulation_steps=60,
        primary_color="#ff6633",    # Orange
        secondary_color="#00ff99",  # Mint
        accent_color="#cc33ff",     # Purple
        color_mode="gradient",
        pattern_type="scan",
        pattern_speed=0.8,
        pattern_density=0.5,
        pattern_width=0.3,
        direction="bounce",
        orientation="both",
        glow_amount=0.5,
        sun_enabled=True,
        sun_size=0.7,
        sun_color="#ff3300"
    )
    box_animation = box.generate_animation()
    with open("retrowave_60x10.json", "w") as f:
        json.dump(box_animation, f, indent=2)

    print("Generated RetroWave animations for different grid layouts:")
    print("1. 60x1 Horizontal Strip")
    print("2. 30x2 Two-Row Grid")
    print("3. 15x4 Four-Row Checkerboard")
    print("4. 5x6 Box with Sun")
    print("4. 60x10 Big box")

# Run the generation
generate_retrowave_presets()