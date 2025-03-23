import json
import math
import colorsys
import random

class ColorSplashGenerator:
    def __init__(self, 
                 num_leds=60,
                 simulation_steps=120,
                 num_colors=5,          # Number of different colors to use
                 num_splashes=12,       # Number of color splashes in the animation
                 splash_height=0.8,     # Maximum height of splashes (0-1)
                 splash_width=0.15,     # Width of each splash
                 splash_decay=0.7,      # How quickly splashes decay
                 bounce_amount=0.3,     # How much color "bounces" after hitting
                 startup_ratio=0.4,     # Portion of animation for build-up
                 middle_ratio=0.2,      # Portion of animation for full display
                 chaos_factor=0.4):     # Randomness in splash timing and position
        """
        Initialize the 2D Color Splash animation generator.
        """
        self.num_leds = num_leds
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Store parameters
        self.num_colors = max(2, min(10, num_colors))
        self.num_splashes = max(5, min(30, num_splashes))
        self.splash_height = max(0.1, min(1.0, splash_height))
        self.splash_width = max(0.05, min(0.5, splash_width))
        self.splash_decay = max(0.1, min(0.9, splash_decay))
        self.bounce_amount = max(0.0, min(0.5, bounce_amount))
        self.chaos_factor = max(0.0, min(0.8, chaos_factor))
        
        # Calculate phase frames
        self.startup_frames = int(simulation_steps * startup_ratio)
        self.middle_frames = int(simulation_steps * middle_ratio)
        self.shutdown_frames = simulation_steps - self.startup_frames - self.middle_frames
        
        # Generate color palette
        self.color_palette = self._generate_color_palette()
        
        # Plan the splash sequence
        self.splashes = self._plan_splash_sequence()
        
        # Internal state tracking
        self.active_splashes = []
        self.color_coverage = [0] * num_leds  # Track color coverage at each position
    
    def _generate_color_palette(self):
        """Generate a vibrant color palette."""
        palette = []
        
        # Netflix red
        palette.append((0.95, 0.0, 0.0))
        
        # Generate a range of bold colors with good spacing
        hues = [i / self.num_colors for i in range(self.num_colors)]
        random.shuffle(hues)  # Randomize order
        
        for hue in hues:
            # Make saturated, bright colors
            saturation = 0.8 + random.random() * 0.2  # 0.8-1.0
            brightness = 0.8 + random.random() * 0.2  # 0.8-1.0
            rgb = colorsys.hsv_to_rgb(hue, saturation, brightness)
            palette.append(rgb)
        
        return palette
    
    def _plan_splash_sequence(self):
        """Plan when and where each color splash will occur."""
        splashes = []
        
        # Distribute splashes primarily during startup phase
        startup_splashes = int(self.num_splashes * 0.9)
        late_splashes = self.num_splashes - startup_splashes
        
        # Create startup splashes
        for i in range(startup_splashes):
            # Calculate base timing - distribute throughout startup phase
            base_frame = int((i / startup_splashes) * self.startup_frames * 0.9)
            
            # Add randomness to timing
            frame = base_frame + int(random.random() * self.startup_frames * 0.1 * self.chaos_factor)
            frame = min(frame, self.startup_frames - 1)
            
            # Select position along strip
            position = random.random()
            
            # Select color
            color_idx = random.randint(0, len(self.color_palette) - 1)
            
            # Determine splash intensity and size
            intensity = 0.7 + random.random() * 0.3
            width_factor = 0.8 + random.random() * 0.4
            
            splashes.append({
                'frame': frame,
                'position': position,
                'color_idx': color_idx,
                'intensity': intensity,
                'width_factor': width_factor
            })
        
        # Create a few late splashes during middle phase
        for i in range(late_splashes):
            frame = self.startup_frames + int(random.random() * self.middle_frames * 0.8)
            position = random.random()
            color_idx = random.randint(0, len(self.color_palette) - 1)
            intensity = 0.6 + random.random() * 0.4
            width_factor = 0.8 + random.random() * 0.4
            
            splashes.append({
                'frame': frame,
                'position': position,
                'color_idx': color_idx,
                'intensity': intensity,
                'width_factor': width_factor
            })
        
        # Sort by frame
        splashes.sort(key=lambda x: x['frame'])
        return splashes
    
    def _color_to_hex(self, color):
        """Convert RGB tuple to hex color string."""
        r = int(max(0, min(1, color[0])) * 255)
        g = int(max(0, min(1, color[1])) * 255)
        b = int(max(0, min(1, color[2])) * 255)
        return f"#{r:02x}{g:02x}{b:02x}"
    
    def _add_splash(self, splash):
        """Add a new active splash."""
        self.active_splashes.append({
            'position': splash['position'],
            'color_idx': splash['color_idx'],
            'intensity': splash['intensity'],
            'width': self.splash_width * splash['width_factor'],
            'height': 0,  # Start at height 0
            'rising': True,  # Initially rising
            'age': 0,  # Track splash age in frames
            'speed': 0.1 + random.random() * 0.1,  # Random initial speed
            'bounces': []  # Track secondary bounces
        })
    
    def _update_splashes(self, is_shutdown_phase):
        """Update all active splashes for the next frame."""
        i = 0
        while i < len(self.active_splashes):
            splash = self.active_splashes[i]
            splash['age'] += 1
            
            # Update height based on physics
            if splash['rising']:
                # Decelerate as it rises (gravity)
                splash['speed'] -= 0.008
                if splash['speed'] <= 0:
                    splash['rising'] = False
                    splash['speed'] = 0.02  # Initial falling speed
            else:
                # Accelerate as it falls (gravity)
                splash['speed'] += 0.012
            
            # Update height
            if splash['rising']:
                splash['height'] += splash['speed']
                # Cap maximum height
                splash['height'] = min(splash['height'], self.splash_height)
            else:
                splash['height'] -= splash['speed']
                
                # Check for bounce when hitting bottom
                if splash['height'] <= 0 and not is_shutdown_phase:
                    # Bounce and create secondary splashes
                    bounce_height = self.bounce_amount * (0.7 + random.random() * 0.3)
                    splash['height'] = 0
                    
                    # Create bounce effect
                    if bounce_height > 0.05:  # Only bounce if significant
                        # Add bounce effects to sides
                        bounce_left = splash['position'] - random.random() * 0.15
                        bounce_right = splash['position'] + random.random() * 0.15
                        
                        # Add to bounce list if within strip
                        if 0 <= bounce_left <= 1:
                            self.active_splashes.append({
                                'position': bounce_left,
                                'color_idx': splash['color_idx'],
                                'intensity': splash['intensity'] * 0.7,
                                'width': splash['width'] * 0.7,
                                'height': 0,
                                'rising': True,
                                'age': 0,
                                'speed': 0.05 + random.random() * 0.05,
                                'bounces': []
                            })
                        
                        if 0 <= bounce_right <= 1:
                            self.active_splashes.append({
                                'position': bounce_right,
                                'color_idx': splash['color_idx'],
                                'intensity': splash['intensity'] * 0.7,
                                'width': splash['width'] * 0.7,
                                'height': 0,
                                'rising': True,
                                'age': 0,
                                'speed': 0.05 + random.random() * 0.05,
                                'bounces': []
                            })
                        
                        # Main splash also rises again
                        splash['rising'] = True
                        splash['speed'] = 0.07 * bounce_height
                    else:
                        # Remove the splash if it's done
                        self.active_splashes.pop(i)
                        continue
            
            # Remove old splashes in shutdown phase or if they're too old
            if is_shutdown_phase and splash['height'] <= 0:
                self.active_splashes.pop(i)
                continue
            elif splash['age'] > 60:  # Maximum age limit
                self.active_splashes.pop(i)
                continue
                
            i += 1
    
    def _calculate_color_at_position(self, position, active_splashes, shutdown_factor=1.0):
        """Calculate the final color at a given position based on all active splashes."""
        # Base color is black
        final_color = [0, 0, 0]
        total_contribution = 0
        
        for splash in active_splashes:
            # Calculate horizontal distance
            distance = abs(position - splash['position'])
            
            # Check if within splash width
            if distance <= splash['width']:
                # Calculate contribution based on distance from center and height
                center_factor = 1.0 - (distance / splash['width'])
                
                # Height factor - color is stronger near the "water line"
                height_factor = 1.0
                if splash['height'] > 0:
                    height_factor = 1.0 - 0.7 * splash['height'] / self.splash_height
                
                # Combine factors
                contribution = center_factor * height_factor * splash['intensity'] * shutdown_factor
                
                # Decay based on age
                age_decay = math.pow(self.splash_decay, splash['age'] / 30.0)
                contribution *= age_decay
                
                # Get color
                color = self.color_palette[splash['color_idx']]
                
                # Add weighted contribution
                final_color[0] += color[0] * contribution
                final_color[1] += color[1] * contribution
                final_color[2] += color[2] * contribution
                total_contribution += contribution
        
        # Normalize if needed
        if total_contribution > 1.0:
            final_color[0] /= total_contribution
            final_color[1] /= total_contribution
            final_color[2] /= total_contribution
        
        return tuple(final_color)
    
    def _render_frame(self, frame_index):
        """Render a single frame of the animation."""
        # Determine animation phase
        if frame_index < self.startup_frames:
            phase = "startup"
            shutdown_factor = 1.0
        elif frame_index < self.startup_frames + self.middle_frames:
            phase = "middle"
            shutdown_factor = 1.0
        else:
            phase = "shutdown"
            # Calculate fade out factor
            progress = (frame_index - self.startup_frames - self.middle_frames) / self.shutdown_frames
            shutdown_factor = 1.0 - progress
        
        # Check for new splashes to add
        for splash in self.splashes:
            if splash['frame'] == frame_index:
                self._add_splash(splash)
        
        # Update all active splashes
        self._update_splashes(phase == "shutdown")
        
        # Render colors for each LED
        leds = []
        for i in range(self.num_leds):
            position = i / (self.num_leds - 1)  # Normalize to 0-1
            color = self._calculate_color_at_position(position, self.active_splashes, shutdown_factor)
            
            # Only include lit LEDs
            if max(color) > 0.02:  # Threshold to avoid very dim LEDs
                leds.append({
                    "index": i,
                    "color": self._color_to_hex(color)
                })
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete Color Splash animation."""
        # Clear any existing frames
        self.frames = []
        self.active_splashes = []
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            frame = self._render_frame(frame_index)
            self.frames.append(frame)
        
        # Create the animation JSON
        animation = {
            "name": "Color Splash Animation",
            "type": "custom",
            "speed": 30,  # 30ms per frame for smooth animation
            "direction": "forward",
            "frames": self.frames
        }
        
        return animation


# Generate different color splash animations
def generate_color_splash_animations():
    # Standard Color Splash
    standard = ColorSplashGenerator(
        num_leds=60,
        simulation_steps=120,
        num_colors=5,
        num_splashes=12,
        splash_height=0.8,
        splash_width=0.15,
        splash_decay=0.7,
        bounce_amount=0.3,
        startup_ratio=0.4,
        middle_ratio=0.2,
        chaos_factor=0.4
    )
    standard_animation = standard.generate_animation()
    with open("standard_color_splash.json", "w") as f:
        json.dump(standard_animation, f, indent=2)
    
    # Chaotic Color Splash
    chaotic = ColorSplashGenerator(
        num_leds=60,
        simulation_steps=120,
        num_colors=7,
        num_splashes=20,
        splash_height=0.9,
        splash_width=0.12,
        splash_decay=0.8,
        bounce_amount=0.4,
        startup_ratio=0.5,
        middle_ratio=0.2,
        chaos_factor=0.7
    )
    chaotic_animation = chaotic.generate_animation()
    with open("chaotic_color_splash.json", "w") as f:
        json.dump(chaotic_animation, f, indent=2)
    
    # Gentle Color Wash
    gentle = ColorSplashGenerator(
        num_leds=60,
        simulation_steps=150,
        num_colors=4,
        num_splashes=8,
        splash_height=0.6,
        splash_width=0.25,
        splash_decay=0.6,
        bounce_amount=0.2,
        startup_ratio=0.6,
        middle_ratio=0.2,
        chaos_factor=0.2
    )
    gentle_animation = gentle.generate_animation()
    with open("gentle_color_wash.json", "w") as f:
        json.dump(gentle_animation, f, indent=2)
    
    print("Generated 3 color splash animations:")
    print("1. Standard Color Splash")
    print("2. Chaotic Color Splash")
    print("3. Gentle Color Wash")

# Run the generation
generate_color_splash_animations()