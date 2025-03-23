import json
import random
import math
import colorsys

class MusicVisualizerAnimation:
    def __init__(self, 
                 width=60,               # Grid width (x dimension)
                 height=1,               # Grid height (y dimension)
                 simulation_steps=120,   # Number of frames to generate
                 # Animation style
                 viz_style="equalizer",  # equalizer, pulse, strobe, wave, spectrum
                 bpm=128,                # Beats per minute
                 beat_intensity=1.2,     # Beat intensity multiplier
                 beat_sharpness=0.7,     # How sharp the beat transition is (0-1)
                 sub_beats=4,            # Number of subdivisions per beat
                 intensity_falloff=0.6,  # How quickly intensity drops after beats
                 
                 # Color settings
                 color_mode="spectrum",  # spectrum, fixed, reactive, strobe, gradient
                 primary_color="#ff0099",  # Main color
                 secondary_color="#00ffff", # Secondary color
                 color_speed=1.0,        # How fast colors change
                 saturation=1.0,         # Color saturation
                 brightness=1.0,         # Base brightness
                 
                 # Special effects
                 flash_beats=True,      # Flash on main beats
                 flash_intensity=1.5,   # How intense flashes are
                 strobe_chance=0.3,     # Chance of a strobe effect on beats
                 xray_effect=False,     # Invert colors on certain beats
                 
                 # Physics
                 gravity=0.0,           # Gravity effect for falling bars (0-1)
                 bounce_amount=0.0,     # How much elements bounce (0-1)
                 
                 # Frame rate
                 frame_speed=25):       # ms per frame
        
        self.width = width
        self.height = height
        self.simulation_steps = simulation_steps
        
        # Animation parameters
        self.viz_style = viz_style
        self.bpm = max(60, min(200, bpm))
        self.beat_intensity = max(0.8, min(2.0, beat_intensity))
        self.beat_sharpness = max(0.1, min(0.95, beat_sharpness))
        self.sub_beats = max(1, min(8, sub_beats))
        self.intensity_falloff = max(0.1, min(0.9, intensity_falloff))
        
        # Color parameters
        self.color_mode = color_mode
        self.primary_color = self._parse_color(primary_color)
        self.secondary_color = self._parse_color(secondary_color)
        self.color_speed = max(0.1, min(3.0, color_speed))
        self.saturation = max(0.0, min(1.0, saturation))
        self.brightness = max(0.3, min(1.5, brightness))
        
        # Effects parameters
        self.flash_beats = flash_beats
        self.flash_intensity = max(1.0, min(2.0, flash_intensity))
        self.strobe_chance = max(0.0, min(0.8, strobe_chance))
        self.xray_effect = xray_effect
        
        # Physics parameters
        self.gravity = max(0.0, min(1.0, gravity))
        self.bounce_amount = max(0.0, min(1.0, bounce_amount))
        
        # Frame rate
        self.frame_speed = max(15, min(50, frame_speed))
        
        # State tracking
        self.frames = []
        self.beat_positions = self._calculate_beat_positions()
        self.last_bar_heights = [0] * width  # For equalizer persistence
        self.color_phase = 0
    
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
    
    def _calculate_beat_positions(self):
        """Calculate frame positions of beats based on BPM."""
        # Frames per beat
        frames_per_beat = (60 / self.bpm) * (1000 / self.frame_speed)
        
        # Calculate positions of all beats and sub-beats
        beat_positions = []
        
        # How many total beats we need (main + sub)
        total_beats = math.ceil(self.simulation_steps / frames_per_beat) * self.sub_beats
        
        for i in range(total_beats):
            # Frame position of this beat
            frame_pos = int((i / self.sub_beats) * frames_per_beat)
            
            # Is this a main beat or sub-beat?
            is_main_beat = (i % self.sub_beats == 0)
            
            # Only add if within our simulation range
            if frame_pos < self.simulation_steps:
                beat_positions.append({
                    'frame': frame_pos,
                    'intensity': 1.0 if is_main_beat else 0.5,
                    'is_main': is_main_beat
                })
        
        return beat_positions
    
    def _get_beat_intensity(self, frame_index):
        """Calculate beat intensity for the current frame."""
        # Find the closest beats before and after this frame
        prev_beat = None
        next_beat = None
        
        for beat in self.beat_positions:
            if beat['frame'] <= frame_index and (prev_beat is None or beat['frame'] > prev_beat['frame']):
                prev_beat = beat
            if beat['frame'] > frame_index and (next_beat is None or beat['frame'] < next_beat['frame']):
                next_beat = beat
        
        # If we're on a beat, return its intensity
        if prev_beat and prev_beat['frame'] == frame_index:
            return prev_beat['intensity'] * self.beat_intensity
        
        # If no previous beat, no intensity yet
        if prev_beat is None:
            return 0.0
        
        # If no next beat, decay from previous
        if next_beat is None:
            # Calculate distance since last beat
            distance = frame_index - prev_beat['frame']
            # Decay based on distance and falloff
            return prev_beat['intensity'] * math.pow(self.intensity_falloff, distance * 0.1)
        
        # We're between beats, calculate intensity based on distance
        total_distance = next_beat['frame'] - prev_beat['frame']
        position = (frame_index - prev_beat['frame']) / total_distance
        
        # Apply easing function for smoother transitions
        # Sharp attack and slow decay
        if position < self.beat_sharpness:
            # Sharp attack (front)
            normalized_pos = position / self.beat_sharpness
            intensity = prev_beat['intensity'] * (1.0 - normalized_pos)
        else:
            # Slow decay (back)
            normalized_pos = (position - self.beat_sharpness) / (1.0 - self.beat_sharpness)
            intensity = prev_beat['intensity'] * math.pow(self.intensity_falloff, normalized_pos * 5)
        
        return intensity * self.beat_intensity
    
    def _is_on_beat(self, frame_index, main_only=False):
        """Check if the current frame is on a beat."""
        for beat in self.beat_positions:
            if beat['frame'] == frame_index:
                if main_only and not beat['is_main']:
                    continue
                return True
        return False
    
    def _get_color_for_position(self, x_pos, y_pos, frame_index, intensity):
        """Get color for a specific position based on current frame and intensity."""
        # Calculate color phase
        self.color_phase = (frame_index * 0.02 * self.color_speed) % 1.0
        
        # Base color selection
        if self.color_mode == "spectrum":
            # Full rainbow spectrum based on position
            hue = (x_pos / self.width + self.color_phase) % 1.0
            sat = self.saturation
            val = self.brightness * (0.5 + intensity * 0.5)
            color = colorsys.hsv_to_rgb(hue, sat, val)
        
        elif self.color_mode == "fixed":
            # Use primary color with intensity variations
            color = (
                self.primary_color[0] * intensity,
                self.primary_color[1] * intensity,
                self.primary_color[2] * intensity
            )
        
        elif self.color_mode == "reactive":
            # Interpolate between primary and secondary based on intensity
            color = (
                self.primary_color[0] * (1 - intensity) + self.secondary_color[0] * intensity,
                self.primary_color[1] * (1 - intensity) + self.secondary_color[1] * intensity,
                self.primary_color[2] * (1 - intensity) + self.secondary_color[2] * intensity
            )
        
        elif self.color_mode == "strobe":
            # Alternate between colors on beats
            if frame_index % 2 == 0:
                color = self.primary_color
            else:
                color = self.secondary_color
            
            # Apply intensity
            color = (
                color[0] * intensity,
                color[1] * intensity,
                color[2] * intensity
            )
        
        elif self.color_mode == "gradient":
            # Gradient across the strip that shifts with time
            gradient_pos = (x_pos / self.width + self.color_phase) % 1.0
            # Interpolate between primary and secondary
            color = (
                self.primary_color[0] * (1 - gradient_pos) + self.secondary_color[0] * gradient_pos,
                self.primary_color[1] * (1 - gradient_pos) + self.secondary_color[1] * gradient_pos,
                self.primary_color[2] * (1 - gradient_pos) + self.secondary_color[2] * gradient_pos
            )
            # Apply intensity
            color = (
                color[0] * intensity,
                color[1] * intensity,
                color[2] * intensity
            )
        
        else:
            # Default to spectrum
            hue = (x_pos / self.width + self.color_phase) % 1.0
            sat = self.saturation
            val = self.brightness * (0.5 + intensity * 0.5)
            color = colorsys.hsv_to_rgb(hue, sat, val)
        
        # Apply X-ray effect (color inversion) if enabled and on certain beats
        if self.xray_effect and frame_index % 8 == 0:
            color = (
                1.0 - color[0],
                1.0 - color[1],
                1.0 - color[2]
            )
        
        return color
    
    def _render_equalizer(self, frame_index, beat_intensity):
        """Render equalizer-style visualization."""
        leds = []
        
        # Calculate bar heights based on frame
        for x in range(self.width):
            # Base height calculation - varies across the strip
            # We use a combination of sine waves for a more organic feel
            position_factor = x / self.width
            wave1 = 0.5 + 0.5 * math.sin(position_factor * math.pi * 4 + frame_index * 0.1)
            wave2 = 0.5 + 0.5 * math.sin(position_factor * math.pi * 7 + frame_index * 0.05)
            wave3 = 0.5 + 0.5 * math.sin(position_factor * math.pi * 2 + frame_index * 0.15)
            
            # Combine waves with weights
            base_height = (wave1 * 0.5 + wave2 * 0.3 + wave3 * 0.2)
            
            # Apply beat intensity
            height_factor = base_height * (0.3 + beat_intensity * 0.7)
            
            # Calculate final height (number of lit LEDs in this column)
            max_height = self.height
            bar_height = int(height_factor * max_height)
            
            # Apply physics if enabled
            if self.gravity > 0:
                # Current bar wants to go to bar_height
                # But it's limited by how fast it can change from last position
                gravity_factor = self.gravity * 0.2
                
                if bar_height > self.last_bar_heights[x]:
                    # Rising - can go up instantly
                    final_height = bar_height
                else:
                    # Falling - limited by gravity
                    fall_speed = gravity_factor * (self.last_bar_heights[x] - bar_height + 1)
                    final_height = max(0, self.last_bar_heights[x] - fall_speed)
                
                # Apply bounce if enabled and hitting bottom
                if self.bounce_amount > 0 and self.last_bar_heights[x] <= 0 and final_height <= 0:
                    # Calculate bounce height based on how fast we were falling
                    bounce_height = fall_speed * self.bounce_amount
                    final_height = bounce_height
                
                # Store for next frame
                self.last_bar_heights[x] = final_height
                bar_height = int(final_height)
            
            # Ensure at least one LED is lit if there's any intensity
            if beat_intensity > 0.1 and bar_height < 1:
                bar_height = 1
            
            # Add LEDs for this column
            for y in range(bar_height):
                # Calculate intensity based on position in the bar
                # Higher positions are brighter
                position_in_bar = y / max_height
                local_intensity = beat_intensity * (0.7 + position_in_bar * 0.3)
                
                # Get color for this LED
                color = self._get_color_for_position(x / self.width, y / max_height, frame_index, local_intensity)
                
                # Add to frame
                leds.append({
                    "x": x,
                    "y": y,
                    "color": self._color_to_hex(color)
                })
        
        return leds
    
    def _render_pulse(self, frame_index, beat_intensity):
        """Render pulse-style visualization."""
        leds = []
        
        # In pulse mode, all LEDs have the same intensity but may have different colors
        for y in range(self.height):
            for x in range(self.width):
                # Skip if below beat threshold
                if beat_intensity < 0.1:
                    continue
                
                # Get color for this LED
                color = self._get_color_for_position(x / self.width, y / self.height, frame_index, beat_intensity)
                
                # Add to frame
                leds.append({
                    "x": x,
                    "y": y,
                    "color": self._color_to_hex(color)
                })
        
        return leds
    
    def _render_strobe(self, frame_index, beat_intensity):
        """Render strobe-style visualization."""
        leds = []
        
        # Check if this is a strobe frame
        is_strobe = False
        
        # Strobe on beats if enabled
        if self._is_on_beat(frame_index):
            if random.random() < self.strobe_chance:
                is_strobe = True
        
        # If strobing, all LEDs are white at max brightness
        if is_strobe:
            for y in range(self.height):
                for x in range(self.width):
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": "#ffffff"
                    })
        else:
            # Otherwise, dim color based on beat intensity
            for y in range(self.height):
                for x in range(self.width):
                    # Skip if below beat threshold
                    if beat_intensity < 0.1:
                        continue
                    
                    # Get color for this LED
                    color = self._get_color_for_position(x / self.width, y / self.height, frame_index, beat_intensity * 0.5)
                    
                    # Add to frame
                    leds.append({
                        "x": x,
                        "y": y,
                        "color": self._color_to_hex(color)
                    })
        
        return leds
    
    def _render_wave(self, frame_index, beat_intensity):
        """Render wave-style visualization."""
        leds = []
        
        # Wave parameters
        wave_speed = 0.1 * self.color_speed
        wave_frequency = 1.0 + beat_intensity
        wave_amplitude = self.height / 2 * beat_intensity
        
        # Calculate wave height for each position
        for x in range(self.width):
            position = x / self.width
            
            # Calculate wave height using sin function
            wave1 = math.sin(position * math.pi * wave_frequency + frame_index * wave_speed)
            wave2 = math.sin(position * math.pi * wave_frequency * 2 + frame_index * wave_speed * 1.5)
            wave_height = wave1 * 0.7 + wave2 * 0.3
            
            # Scale wave height based on beat intensity
            scaled_height = wave_height * wave_amplitude
            
            # Calculate LED y position
            if self.height == 1:
                # Special case for 1D strip - all LEDs are at y=0
                y_pos = 0
            else:
                # Map to center of the strip
                center_y = (self.height - 1) / 2
                y_pos = round(center_y + scaled_height)
                
                # Clamp to valid range
                y_pos = max(0, min(self.height - 1, y_pos))
            
            # Get color for this LED
            intensity = beat_intensity * (0.5 + 0.5 * abs(wave_height))
            color = self._get_color_for_position(position, y_pos / max(1, self.height - 1), frame_index, intensity)
            
            # Add LED to frame
            leds.append({
                "x": x,
                "y": y_pos,
                "color": self._color_to_hex(color)
            })
        
        return leds
    
    def _render_spectrum(self, frame_index, beat_intensity):
        """Render spectrum-style visualization."""
        leds = []
        
        # For spectrum, we create a moving color gradient across the strip
        for y in range(self.height):
            for x in range(self.width):
                # Skip if below beat threshold
                if beat_intensity < 0.1:
                    continue
                
                # Position in strip
                position = x / self.width
                
                # Calculate intensity based on a spectrum analyzer look
                # Higher in the middle, lower at the edges
                center_distance = abs(position - 0.5) * 2  # 0 at center, 1 at edges
                intensity_factor = 1.0 - center_distance * 0.7
                local_intensity = beat_intensity * intensity_factor
                
                # Skip if too dim
                if local_intensity < 0.1:
                    continue
                
                # Get color
                color = self._get_color_for_position(position, y / max(1, self.height - 1), frame_index, local_intensity)
                
                # Add LED to frame
                leds.append({
                    "x": x,
                    "y": y,
                    "color": self._color_to_hex(color)
                })
        
        return leds
    
    def _apply_flash_effect(self, leds, frame_index, beat_intensity):
        """Apply flash effect on beats if enabled."""
        if not self.flash_beats:
            return leds
        
        # Check if we're on a main beat
        if not self._is_on_beat(frame_index, True):
            return leds
        
        # Create a flash by brightening all colors
        brightened_leds = []
        
        for led in leds:
            # Parse existing color
            color_hex = led["color"]
            r = int(color_hex[1:3], 16) / 255.0
            g = int(color_hex[3:5], 16) / 255.0
            b = int(color_hex[5:7], 16) / 255.0
            
            # Brighten the color
            r = min(1.0, r * self.flash_intensity)
            g = min(1.0, g * self.flash_intensity)
            b = min(1.0, b * self.flash_intensity)
            
            # Convert back to hex
            new_color = f"#{int(r*255):02x}{int(g*255):02x}{int(b*255):02x}"
            
            # Update LED
            brightened_leds.append({
                "x": led["x"],
                "y": led["y"],
                "color": new_color
            })
        
        return brightened_leds
    
    def _render_frame(self, frame_index):
        """Render a single frame of the animation."""
        # Calculate beat intensity for this frame
        beat_intensity = self._get_beat_intensity(frame_index)
        
        # Render based on selected style
        if self.viz_style == "equalizer":
            leds = self._render_equalizer(frame_index, beat_intensity)
        elif self.viz_style == "pulse":
            leds = self._render_pulse(frame_index, beat_intensity)
        elif self.viz_style == "strobe":
            leds = self._render_strobe(frame_index, beat_intensity)
        elif self.viz_style == "wave":
            leds = self._render_wave(frame_index, beat_intensity)
        elif self.viz_style == "spectrum":
            leds = self._render_spectrum(frame_index, beat_intensity)
        else:
            # Default to equalizer
            leds = self._render_equalizer(frame_index, beat_intensity)
        
        # Apply flash effect if enabled
        leds = self._apply_flash_effect(leds, frame_index, beat_intensity)
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete music visualization animation."""
        # Clear any existing frames
        self.frames = []
        
        # Reset state
        self.last_bar_heights = [0] * self.width
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            frame = self._render_frame(frame_index)
            self.frames.append(frame)
        
        # Create the animation JSON
        animation = {
            "name": f"Music Visualizer ({self.viz_style})",
            "type": "custom",
            "speed": self.frame_speed,
            "direction": "forward",
            "dimensions": {
                "length": self.width,
                "height": self.height
            },
            "frames": self.frames
        }
        
        return animation


# Generate visualizations for different configurations
def generate_music_visualizations():
    # 1. Fast Strobe - High energy club visualization
    strobe_viz = MusicVisualizerAnimation(
        width=60,
        height=1,
        simulation_steps=120,
        viz_style="strobe",
        bpm=140,
        beat_intensity=1.5,
        beat_sharpness=0.8,
        sub_beats=4,
        intensity_falloff=0.7,
        color_mode="reactive",
        primary_color="#ff00ff",
        secondary_color="#00ffff",
        color_speed=2.0,
        saturation=1.0,
        brightness=1.2,
        flash_beats=True,
        flash_intensity=1.8,
        strobe_chance=0.6,
        xray_effect=True,
        frame_speed=20
    )
    strobe_animation = strobe_viz.generate_animation()
    with open("club_strobe_60x1.json", "w") as f:
        json.dump(strobe_animation, f, indent=2)

    # 2. Equalizer - Classic audio visualizer
    eq_viz = MusicVisualizerAnimation(
        width=60,
        height=1,
        simulation_steps=120,
        viz_style="equalizer",
        bpm=128,
        beat_intensity=1.2,
        beat_sharpness=0.6,
        sub_beats=4,
        intensity_falloff=0.6,
        color_mode="spectrum",
        color_speed=1.0,
        saturation=1.0,
        brightness=1.0,
        flash_beats=True,
        flash_intensity=1.4,
        strobe_chance=0.0,
        gravity=0.7,
        bounce_amount=0.3,
        frame_speed=25
    )
    eq_animation = eq_viz.generate_animation()
    with open("equalizer_60x1.json", "w") as f:
        json.dump(eq_animation, f, indent=2)

    # 3. Wave - Smooth sine wave visualization
    wave_viz = MusicVisualizerAnimation(
        width=30,
        height=2,
        simulation_steps=120,
        viz_style="wave",
        bpm=120,
        beat_intensity=1.1,
        beat_sharpness=0.5,
        sub_beats=4,
        intensity_falloff=0.5,
        color_mode="gradient",
        primary_color="#0077ff",
        secondary_color="#00ffaa",
        color_speed=0.8,
        saturation=0.9,
        brightness=1.1,
        flash_beats=False,
        frame_speed=30
    )
    wave_animation = wave_viz.generate_animation()
    with open("wave_30x2.json", "w") as f:
        json.dump(wave_animation, f, indent=2)

    # 4. Pulse - Simple pulsing visualization
    pulse_viz = MusicVisualizerAnimation(
        width=15,
        height=4,
        simulation_steps=120,
        viz_style="pulse",
        bpm=100,
        beat_intensity=1.3,
        beat_sharpness=0.6,
        sub_beats=2,
        intensity_falloff=0.5,
        color_mode="reactive",
        primary_color="#000033",
        secondary_color="#00ffff",
        color_speed=1.2,
        saturation=1.0,
        brightness=1.2,
        flash_beats=True,
        flash_intensity=1.5,
        frame_speed=25
    )
    pulse_animation = pulse_viz.generate_animation()
    with open("pulse_15x4.json", "w") as f:
        json.dump(pulse_animation, f, indent=2)

    print("Generated 4 music visualization animations:")
    print("1. Club Strobe (60x1)")
    print("2. Equalizer (60x1)")
    print("3. Wave (30x2)")
    print("4. Pulse (15x4)")

# Run the generation
generate_music_visualizations()