import json
import math
import colorsys
import random

class ClubBeatGenerator:
    def __init__(self, 
                 num_leds=60,
                 simulation_steps=200,
                 bpm=128,                  # Beats per minute (typical EDM tempo)
                 colors=["#ff00ff",        # Hot pink
                         "#00ffff",        # Cyan
                         "#ffff00",        # Yellow
                         "#00ff00"],       # Green
                 bass_intensity=1.2,       # Bass hit intensity multiplier
                 strobe_chance=0.3,        # Probability of strobe effects
                 pulse_decay=0.6,          # How quickly pulses fade (lower = longer)
                 color_change_speed=0.5,   # How quickly colors shift
                 synchronize_beats=True,   # True for on-beat, False for more variation
                 brightness_boost=1.5):    # Overall brightness multiplier
        """
        Initialize the Club Beat Animation generator with beat-synchronized effects.
        """
        self.num_leds = num_leds
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Store parameters
        self.bpm = max(60, min(180, bpm))
        self.colors = [self._parse_color(c) for c in colors]
        self.bass_intensity = max(0.8, min(2.0, bass_intensity))
        self.strobe_chance = max(0.0, min(0.5, strobe_chance))
        self.pulse_decay = max(0.1, min(0.9, pulse_decay))
        self.color_change_speed = max(0.1, min(1.0, color_change_speed))
        self.synchronize_beats = synchronize_beats
        self.brightness_boost = max(0.8, min(2.0, brightness_boost))
        
        # Calculate frames per beat based on BPM and frame rate
        self.frames_per_beat = int(60 / self.bpm * (1000 / 30))  # Assuming 30ms per frame
        
        # Generate beat pattern
        self.beat_pattern = self._generate_beat_pattern()
        
        # Color cycling state
        self.current_color_index = 0
        self.next_color_index = 1
        self.color_transition = 0.0
        
        # Additional colors (derived from main colors)
        self.expanded_colors = self._expand_colors()
    
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
        # Apply brightness boost
        r = min(1.0, color[0] * self.brightness_boost)
        g = min(1.0, color[1] * self.brightness_boost)
        b = min(1.0, color[2] * self.brightness_boost)
        
        # Convert to hex
        r_hex = int(r * 255)
        g_hex = int(g * 255)
        b_hex = int(b * 255)
        return f"#{r_hex:02x}{g_hex:02x}{b_hex:02x}"
    
    def _expand_colors(self):
        """Create additional colors by blending and shifting the main colors."""
        expanded = list(self.colors)
        
        # Add white and near-white for strobe effects
        expanded.append((1.0, 1.0, 1.0))        # Pure white
        expanded.append((1.0, 0.9, 1.0))        # Slightly purple white
        expanded.append((0.9, 1.0, 1.0))        # Slightly cyan white
        
        # Add blends between adjacent colors
        for i in range(len(self.colors)):
            color1 = self.colors[i]
            color2 = self.colors[(i + 1) % len(self.colors)]
            
            # Create blend
            blend = (
                (color1[0] + color2[0]) / 2,
                (color1[1] + color2[1]) / 2,
                (color1[2] + color2[2]) / 2
            )
            expanded.append(blend)
        
        # Create some high-saturation variants by converting to HSV and back
        for color in self.colors:
            h, s, v = colorsys.rgb_to_hsv(*color)
            # Make a more saturated version
            saturated = colorsys.hsv_to_rgb(h, min(1.0, s * 1.5), min(1.0, v * 1.2))
            expanded.append(saturated)
        
        return expanded
    
    def _generate_beat_pattern(self):
        """Generate a typical EDM beat pattern with variations."""
        # Calculate total beats in animation
        total_beats = self.simulation_steps / self.frames_per_beat
        
        # A standard pattern repeats every 4 beats (one bar)
        pattern = []
        
        # Create enough patterns to fill the entire animation
        bars_needed = math.ceil(total_beats / 4)
        
        for bar in range(bars_needed):
            # Each bar has multiple patterns:
            
            # Standard 4/4 beat: kick on 1 and 3, snare on 2 and 4
            kicks = [0, 2]  # 1st and 3rd beats
            snares = [1, 3]  # 2nd and 4th beats
            
            # Every 4th bar, add a fill or variation
            if bar % 4 == 3:
                if random.random() < 0.5:
                    # Drum fill: more kicks
                    kicks = [0, 1, 2, 3]  # Kick on every beat
                else:
                    # Double-time feel
                    kicks = [0, 1, 2, 3]
                    # Add off-beat snares
                    snares = [0.5, 1.5, 2.5, 3.5]
            
            # Create the pattern for this bar
            for beat in range(4):
                # Base beat intensity
                intensity = 0.0
                
                # Add kick drum impact
                if beat in kicks:
                    intensity += 1.0
                
                # Add snare impact
                if beat in snares:
                    intensity += 0.7
                
                # Add hi-hat rhythm (on every quarter beat)
                hi_hat = 0.3
                
                # Store this beat's info
                pattern.append({
                    'intensity': intensity,
                    'hi_hat': hi_hat,
                    'is_kick': beat in kicks,
                    'is_snare': beat in snares,
                    'is_major': beat == 0  # First beat of bar is major
                })
                
                # Add off-beat hi-hats if applicable
                if beat + 0.5 in snares:
                    pattern.append({
                        'intensity': 0.8,  # off-beat snare
                        'hi_hat': 0.0,
                        'is_kick': False,
                        'is_snare': True,
                        'is_major': False
                    })
        
        return pattern
    
    def _get_beat_for_frame(self, frame_index):
        """Get the beat information for the current frame."""
        beat_position = frame_index / self.frames_per_beat
        beat_index = int(beat_position)
        beat_progress = beat_position - beat_index  # 0.0 to 1.0 within the beat
        
        # Get the pattern for this beat
        if beat_index < len(self.beat_pattern):
            current_beat = self.beat_pattern[beat_index]
        else:
            # Default if we somehow went past the pattern
            current_beat = {'intensity': 0.0, 'hi_hat': 0.0, 'is_kick': False, 'is_snare': False, 'is_major': False}
        
        return current_beat, beat_progress
    
    def _update_colors(self, frame_index):
        """Update the color cycling for this frame."""
        # Speed up color changes on major beats
        beat_position = frame_index / self.frames_per_beat
        beat_index = int(beat_position)
        
        # Check if we're on a major beat
        is_major_beat = False
        if beat_index < len(self.beat_pattern):
            is_major_beat = self.beat_pattern[beat_index].get('is_major', False)
        
        # Update color transition
        if is_major_beat and random.random() < 0.3:
            # Chance of a sudden color change on major beats
            self.current_color_index = random.randint(0, len(self.colors) - 1)
            self.next_color_index = (self.current_color_index + 1) % len(self.colors)
            self.color_transition = 0.0
        else:
            # Normal transition
            self.color_transition += 0.01 * self.color_change_speed
            
            # If transition complete, move to next color
            if self.color_transition >= 1.0:
                self.current_color_index = self.next_color_index
                self.next_color_index = (self.current_color_index + 1) % len(self.colors)
                self.color_transition = 0.0
    
    def _get_current_color(self):
        """Get the current interpolated color based on the transition."""
        current = self.colors[self.current_color_index]
        next_color = self.colors[self.next_color_index]
        
        # Linear interpolation between colors
        return (
            current[0] * (1 - self.color_transition) + next_color[0] * self.color_transition,
            current[1] * (1 - self.color_transition) + next_color[1] * self.color_transition,
            current[2] * (1 - self.color_transition) + next_color[2] * self.color_transition
        )
    
    def _calculate_kick_wave(self, position, beat_progress):
        """Calculate kick drum wave effect."""
        # Kicks create a wave that propagates from the center
        center_distance = abs(position - 0.5)
        
        # Wave speed - moves outward from center
        wave_position = beat_progress * 0.5  # Wave reaches edges when beat_progress = 1.0
        
        # Distance from wave front
        wave_distance = abs(center_distance - wave_position)
        
        # Intensity based on distance from wave front
        if wave_distance < 0.1:
            wave_intensity = (0.1 - wave_distance) / 0.1
            return wave_intensity * (1.0 - beat_progress * self.pulse_decay)
        
        return 0.0
    
    def _calculate_bass_pulse(self, position, beat_progress, is_kick):
        """Calculate bass pulse effect."""
        if not is_kick:
            return 0.0
        
        # Bass pulses are stronger in the center
        center_factor = 1.0 - abs(position - 0.5) * 0.5
        
        # Decay over time
        decay = math.pow(self.pulse_decay, beat_progress * 10)
        
        return center_factor * decay * self.bass_intensity
    
    def _calculate_strobe(self, beat_progress, is_major):
        """Determine if a strobe effect should be active."""
        if random.random() > self.strobe_chance:
            return False
        
        # Strobes more likely on strong beats
        strobe_probability = 0.1
        if is_major:
            strobe_probability = 0.4
        
        # Only strobe for a brief moment
        if beat_progress < 0.1 and random.random() < strobe_probability:
            return True
        
        return False
    
    def _render_frame(self, frame_index):
        """Render a single frame of the animation."""
        # Update color cycling
        self._update_colors(frame_index)
        
        # Get current beat info
        beat, beat_progress = self._get_beat_for_frame(frame_index)
        
        # Prepare LEDs for this frame
        leds = []
        
        # Check for strobe effect
        strobe_active = self._calculate_strobe(beat_progress, beat['is_major'])
        
        # Process each LED
        for i in range(self.num_leds):
            position = i / (self.num_leds - 1)  # Normalize to 0-1
            
            # Start with base color
            base_color = self._get_current_color()
            
            # Apply effects
            if strobe_active:
                # Strobe effect - bright white flash
                intensity = 1.0
                color = random.choice(self.expanded_colors[-3:])  # Use white/near-white colors
            else:
                # Regular beat-synced effects
                kick_intensity = self._calculate_kick_wave(position, beat_progress) if beat['is_kick'] else 0.0
                bass_intensity = self._calculate_bass_pulse(position, beat_progress, beat['is_kick'])
                hi_hat_intensity = beat['hi_hat'] * (1.0 - beat_progress * 0.7) if beat_progress < 0.5 else 0.0
                
                # Combine effects
                intensity = max(kick_intensity, bass_intensity) + hi_hat_intensity * 0.3
                
                # Base intensity ensures some always-on ambient light
                base_intensity = 0.2
                intensity = max(base_intensity, intensity)
                
                # Color selection
                if bass_intensity > 0.5:
                    # During strong bass hits, use more saturated colors
                    color_index = random.randint(len(self.colors), len(self.expanded_colors) - 4)
                    color = self.expanded_colors[color_index]
                else:
                    # Normal color cycling
                    color = base_color
            
            # Apply intensity
            final_color = (
                color[0] * intensity,
                color[1] * intensity,
                color[2] * intensity
            )
            
            # Skip if too dim
            if max(final_color) < 0.05:
                continue
            
            # Add to frame
            leds.append({
                "index": i,
                "color": self._color_to_hex(final_color)
            })
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete Club Beat animation."""
        # Clear any existing frames
        self.frames = []
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            frame = self._render_frame(frame_index)
            self.frames.append(frame)
        
        # Create the animation JSON
        animation = {
            "name": "Club Beat Animation",
            "type": "custom",
            "speed": 30,  # 30ms per frame for smooth animation
            "direction": "forward",
            "frames": self.frames
        }
        
        return animation


# Generate different club beat animations
def generate_club_animations():
    # House Music
    house = ClubBeatGenerator(
        num_leds=60,
        simulation_steps=200,
        bpm=128,
        colors=["#ff00ff", "#00ffff", "#ffff00", "#ff00aa"],  # Pink, Cyan, Yellow, Hot Pink
        bass_intensity=1.2,
        strobe_chance=0.2,
        pulse_decay=0.6,
        color_change_speed=0.5,
        synchronize_beats=True,
        brightness_boost=1.5
    )
    house_animation = house.generate_animation()
    with open("house_beat.json", "w") as f:
        json.dump(house_animation, f, indent=2)
    
    # Techno
    techno = ClubBeatGenerator(
        num_leds=60,
        simulation_steps=240,
        bpm=140,                                      # Faster BPM
        colors=["#0077ff", "#00ffaa", "#ff0066"],     # Blue, Teal, Pink
        bass_intensity=1.4,                           # Stronger bass
        strobe_chance=0.3,                            # More strobes
        pulse_decay=0.5,                              # Quicker decay
        color_change_speed=0.7,                       # Faster color changes
        synchronize_beats=True,
        brightness_boost=1.7                          # Brighter
    )
    techno_animation = techno.generate_animation()
    with open("techno_beat.json", "w") as f:
        json.dump(techno_animation, f, indent=2)
    
    # Drum and Bass
    dnb = ClubBeatGenerator(
        num_leds=60,
        simulation_steps=180,
        bpm=174,                                      # Fast BPM
        colors=["#ff3300", "#ffaa00", "#00ff77"],     # Orange, Amber, Green
        bass_intensity=1.6,                           # Very strong bass
        strobe_chance=0.4,                            # More strobes
        pulse_decay=0.4,                              # Fast decay
        color_change_speed=0.9,                       # Very fast color changes
        synchronize_beats=False,                      # Less synchronized
        brightness_boost=1.6
    )
    dnb_animation = dnb.generate_animation()
    with open("drum_and_bass.json", "w") as f:
        json.dump(dnb_animation, f, indent=2)
    
    print("Generated 3 club beat animations:")
    print("1. House Music (128 BPM)")
    print("2. Techno (140 BPM)")
    print("3. Drum and Bass (174 BPM)")

# Run the generation
generate_club_animations()