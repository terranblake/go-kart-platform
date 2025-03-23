import json
import math
import colorsys
import random

class NetflixIntroGenerator:
    def __init__(self, 
                 num_leds=60,
                 simulation_steps=100,
                 beam_width=0.15,        # Width of the light beam
                 beam_speed=1.0,         # Speed of the beam
                 startup_frames=30,      # How many frames for the startup sequence
                 middle_frames=30,       # How many frames for the middle sequence
                 shutdown_frames=40,     # How many frames for the shutdown sequence
                 color_saturation=1.0,   # 0.0-1.0
                 color_brightness=1.0,   # 0.0-1.0
                 random_seed=None):      # Set for reproducible animations
        """
        Initialize the Netflix Intro Animation generator.
        """
        self.num_leds = num_leds
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Set random seed if provided
        if random_seed is not None:
            random.seed(random_seed)
        
        # Store parameters
        self.beam_width = max(0.05, min(0.5, beam_width))
        self.beam_speed = max(0.1, min(5.0, beam_speed))
        self.startup_frames = max(10, min(simulation_steps//3, startup_frames))
        self.middle_frames = max(10, min(simulation_steps//3, middle_frames))
        self.shutdown_frames = max(10, min(simulation_steps//3, shutdown_frames))
        self.color_saturation = max(0.0, min(1.0, color_saturation))
        self.color_brightness = max(0.0, min(1.0, color_brightness))
        
        # Ensure we have enough frames
        total_frames = self.startup_frames + self.middle_frames + self.shutdown_frames
        if total_frames > simulation_steps:
            scale = simulation_steps / total_frames
            self.startup_frames = int(self.startup_frames * scale)
            self.middle_frames = int(self.middle_frames * scale)
            self.shutdown_frames = simulation_steps - self.startup_frames - self.middle_frames
        
        # Generate Netflix color palette
        self.color_palette = self._generate_color_palette()
        
        # Internal state tracking
        self.lit_segments = set()  # Track which LEDs are currently "discovered"
    
    def _generate_color_palette(self):
        """Generate a color palette of vibrant, Netflix-like colors."""
        palette = []
        
        # Netflix red
        palette.append((0.89, 0.0, 0.0))
        
        # Generate a range of bold colors
        hue_values = [0.0, 0.07, 0.15, 0.3, 0.55, 0.75, 0.85]
        for hue in hue_values:
            # Make saturated, bright colors
            rgb = colorsys.hsv_to_rgb(hue, self.color_saturation, self.color_brightness)
            palette.append(rgb)
        
        # Add some deeper versions
        for hue in hue_values[1:]:
            # Deeper, richer version
            rgb = colorsys.hsv_to_rgb(hue, self.color_saturation, self.color_brightness * 0.7)
            palette.append(rgb)
        
        return palette
    
    def _color_to_hex(self, color):
        """Convert RGB tuple to hex color string."""
        r = int(max(0, min(1, color[0])) * 255)
        g = int(max(0, min(1, color[1])) * 255)
        b = int(max(0, min(1, color[2])) * 255)
        return f"#{r:02x}{g:02x}{b:02x}"
    
    def _calculate_beam_position(self, frame_index):
        """Calculate the beam position and phase for a given frame."""
        # Determine animation phase
        if frame_index < self.startup_frames:
            # Startup phase: beam moves from left to right
            phase = "startup"
            progress = frame_index / self.startup_frames
            beam_position = progress
        elif frame_index < self.startup_frames + self.middle_frames:
            # Middle phase: beam is at the end
            phase = "middle"
            beam_position = 1.0
        else:
            # Shutdown phase: beam moves from right to left
            phase = "shutdown"
            progress = (frame_index - self.startup_frames - self.middle_frames) / self.shutdown_frames
            beam_position = 1.0 - progress
        
        return phase, beam_position
    
    def _get_color_for_position(self, position):
        """Get a color based on the LED position."""
        # Map position to a color from the palette
        # This ensures consistent colors for the same positions
        position_hash = hash(str(position)) % len(self.color_palette)
        return self.color_palette[position_hash]
    
    def _calculate_segment_illumination(self, led_index, beam_position, phase):
        """Determine if an LED segment should be illuminated and at what intensity."""
        # Normalize LED position to 0-1 range
        position = led_index / (self.num_leds - 1)
        
        # Beam edge calculation
        distance_from_beam = abs(position - beam_position)
        
        # LED is directly illuminated by the beam
        if distance_from_beam < self.beam_width:
            # Inside the beam: full intensity
            intensity = 1.0
            in_beam = True
        else:
            # Outside the beam
            intensity = 0.0
            in_beam = False
        
        # During startup, we "discover" LEDs as the beam passes over them
        if phase == "startup" and in_beam:
            self.lit_segments.add(led_index)
        
        # LEDs that have been discovered stay on but dimmer
        if led_index in self.lit_segments:
            if not in_beam:
                # Discovered but not in beam: partial intensity
                if phase == "startup" or phase == "middle":
                    intensity = 0.4  # Stay dimly lit after beam passes
                else:  # shutdown phase
                    # Fade out during shutdown if behind the beam
                    if position > beam_position:
                        intensity = 0.0  # Turn off if beam has passed in shutdown
                    else:
                        intensity = 0.4  # Stay dim if beam hasn't reached yet
        
        # During shutdown, we "undiscover" LEDs as the beam passes over them
        if phase == "shutdown" and in_beam:
            if led_index in self.lit_segments:
                self.lit_segments.remove(led_index)
        
        return intensity, in_beam
    
    def _render_frame(self, frame_index):
        """Render a single frame of the animation."""
        # Reset discovered segments if this is the first frame
        if frame_index == 0:
            self.lit_segments = set()
        
        # Calculate beam position and phase
        phase, beam_position = self._calculate_beam_position(frame_index)
        
        # Prepare LEDs for this frame
        leds = []
        
        # Process each LED
        for i in range(self.num_leds):
            intensity, in_beam = self._calculate_segment_illumination(i, beam_position, phase)
            
            # Skip if no intensity
            if intensity <= 0:
                continue
            
            # Get the base color for this LED
            base_color = self._get_color_for_position(i)
            
            # Apply intensity to color
            color = (
                base_color[0] * intensity,
                base_color[1] * intensity,
                base_color[2] * intensity
            )
            
            # If in beam, make it brighter white (beam effect)
            if in_beam:
                # Add white glow to create beam effect
                whiteness = 0.6
                color = (
                    min(1.0, color[0] + whiteness),
                    min(1.0, color[1] + whiteness),
                    min(1.0, color[2] + whiteness)
                )
            
            # Add to frame
            leds.append({
                "index": i,
                "color": self._color_to_hex(color)
            })
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete Netflix Intro animation."""
        # Clear any existing frames
        self.frames = []
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            frame = self._render_frame(frame_index)
            self.frames.append(frame)
        
        # Create the animation JSON
        animation = {
            "name": "Netflix Intro Animation",
            "type": "custom",
            "speed": 40,  # 40ms per frame for smooth animation
            "direction": "forward",
            "frames": self.frames
        }
        
        return animation


# Generate Netflix-style intro with different parameters
def generate_netflix_intros():
    # Standard Netflix Intro
    standard_intro = NetflixIntroGenerator(
        num_leds=60,
        simulation_steps=100,
        beam_width=0.15,
        beam_speed=1.0,
        startup_frames=40,
        middle_frames=20,
        shutdown_frames=40
    )
    standard_animation = standard_intro.generate_animation()
    with open("netflix_standard_intro.json", "w") as f:
        json.dump(standard_animation, f, indent=2)
    
    # Fast Netflix Intro
    fast_intro = NetflixIntroGenerator(
        num_leds=60,
        simulation_steps=60,
        beam_width=0.2,
        beam_speed=2.0,
        startup_frames=25,
        middle_frames=10,
        shutdown_frames=25
    )
    fast_animation = fast_intro.generate_animation()
    with open("netflix_fast_intro.json", "w") as f:
        json.dump(fast_animation, f, indent=2)
    
    # Slow Dramatic Intro
    dramatic_intro = NetflixIntroGenerator(
        num_leds=60,
        simulation_steps=150,
        beam_width=0.1,
        beam_speed=0.6,
        startup_frames=60,
        middle_frames=30,
        shutdown_frames=60,
        color_saturation=1.2,
        color_brightness=0.9
    )
    dramatic_animation = dramatic_intro.generate_animation()
    with open("netflix_dramatic_intro.json", "w") as f:
        json.dump(dramatic_animation, f, indent=2)
    
    print("Generated 3 Netflix-style intro animations:")
    print("1. Standard Intro")
    print("2. Fast Intro")
    print("3. Slow Dramatic Intro")

# Run the generation
generate_netflix_intros()