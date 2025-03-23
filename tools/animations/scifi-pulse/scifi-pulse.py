import json
import math
import colorsys
import random

class SciFiPulseGenerator:
    def __init__(self, 
                 num_leds=60,
                 simulation_steps=150,
                 pulses=3,            # Number of major pulse events
                 pulse_color="#00eeff", # Base color for pulses
                 accent_color="#ff00ff", # Accent color
                 energy_build_rate=0.8, # How fast energy builds (0-1)
                 crash_speed=1.5,     # How fast crashes occur
                 brightness=1.2,      # Overall brightness multiplier
                 flicker_amount=0.3,  # Amount of random flickering
                 warp_speed=1.5):     # Speed of moving light effects
        """
        Initialize the Sci-Fi Pulse animation generator with dramatic buildups and crashes.
        """
        self.num_leds = num_leds
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Store parameters
        self.pulses = max(1, min(5, pulses))
        self.pulse_color = self._parse_color(pulse_color)
        self.accent_color = self._parse_color(accent_color)
        self.energy_build_rate = max(0.1, min(1.0, energy_build_rate))
        self.crash_speed = max(0.5, min(3.0, crash_speed))
        self.brightness = max(0.5, min(2.0, brightness))
        self.flicker_amount = max(0.0, min(0.7, flicker_amount))
        self.warp_speed = max(0.5, min(3.0, warp_speed))
        
        # Calculate key sequence points
        self.key_points = self._calculate_key_points()
        
        # Generate secondary colors
        self.colors = self._generate_colors()
    
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
        r = int(max(0, min(1, color[0] * self.brightness)) * 255)
        g = int(max(0, min(1, color[1] * self.brightness)) * 255)
        b = int(max(0, min(1, color[2] * self.brightness)) * 255)
        return f"#{r:02x}{g:02x}{b:02x}"
    
    def _calculate_key_points(self):
        """Calculate the timing of pulse buildups and crashes."""
        points = []
        segment_length = self.simulation_steps / self.pulses
        
        for i in range(self.pulses):
            segment_start = i * segment_length
            
            # Buildup takes 70% of the segment, crash takes 30%
            buildup_length = int(segment_length * 0.7)
            crash_length = int(segment_length * 0.3)
            
            # Ensure crash_length is at least 10 frames
            if crash_length < 10:
                crash_length = 10
                buildup_length = segment_length - crash_length
            
            buildup_start = segment_start
            buildup_end = segment_start + buildup_length
            crash_start = buildup_end
            crash_end = crash_start + crash_length
            
            points.append({
                'buildup_start': int(buildup_start),
                'buildup_end': int(buildup_end),
                'crash_start': int(crash_start),
                'crash_end': int(crash_end),
                'peak_energy': 0.7 + random.random() * 0.3  # Random peak energy for variety
            })
        
        return points
    
    def _generate_colors(self):
        """Generate color palette for the animation."""
        colors = [self.pulse_color, self.accent_color]
        
        # Add secondary colors
        h, s, v = colorsys.rgb_to_hsv(*self.pulse_color)
        h2, s2, v2 = colorsys.rgb_to_hsv(*self.accent_color)
        
        # Add shifted color variants
        for shift in [0.05, 0.1, 0.15]:
            # Shifted from primary
            h_shifted = (h + shift) % 1.0
            colors.append(colorsys.hsv_to_rgb(h_shifted, s, v))
            
            # Shifted from accent
            h2_shifted = (h2 + shift) % 1.0
            colors.append(colorsys.hsv_to_rgb(h2_shifted, s2, v2))
            
            # High energy colors (whiter)
            colors.append(colorsys.hsv_to_rgb(h_shifted, 0.5 * s, min(1.0, v * 1.2)))
            colors.append(colorsys.hsv_to_rgb(h2_shifted, 0.5 * s2, min(1.0, v2 * 1.2)))
        
        # Add "warning" colors for buildup
        colors.append((1.0, 0.5, 0.0))  # Orange
        colors.append((1.0, 0.3, 0.0))  # Dark orange/red
        
        return colors
    
    def _get_energy_level(self, frame_index):
        """Calculate the system's energy level at a given frame."""
        energy = 0
        active_segment = None
        
        # Find which segment we're in
        for point in self.key_points:
            if point['buildup_start'] <= frame_index < point['crash_end']:
                active_segment = point
                break
        
        if not active_segment:
            return 0.0  # Default to zero energy
        
        # Calculate energy based on segment phase
        if frame_index < active_segment['buildup_end']:
            # Buildup phase - energy increases
            progress = (frame_index - active_segment['buildup_start']) / (active_segment['buildup_end'] - active_segment['buildup_start'])
            
            # Apply easing function for non-linear buildup (accelerating)
            progress_eased = math.pow(progress, 1.0 / self.energy_build_rate)
            energy = progress_eased * active_segment['peak_energy']
        else:
            # Crash phase - energy decreases rapidly
            progress = (frame_index - active_segment['crash_start']) / (active_segment['crash_end'] - active_segment['crash_start'])
            
            # Apply easing function for rapid crash
            progress_eased = math.pow(progress, self.crash_speed)
            energy = active_segment['peak_energy'] * (1.0 - progress_eased)
        
        return energy
    
    def _calculate_warp_effect(self, position, frame_index, energy):
        """Calculate fast-moving warp effect."""
        # Base speed is affected by energy and warp_speed parameter
        speed = self.warp_speed * (0.5 + energy * 2.0)
        
        # Multiple warp streams with different speeds and phases
        warp1 = 0.5 + 0.5 * math.sin((position * 10 + frame_index * speed * 0.2) % (2 * math.pi))
        warp2 = 0.5 + 0.5 * math.sin((position * 20 + frame_index * speed * 0.1 + 1.0) % (2 * math.pi))
        warp3 = 0.5 + 0.5 * math.sin((position * 5 - frame_index * speed * 0.15 + 2.0) % (2 * math.pi))
        
        # Combine with different weights
        warp_intensity = warp1 * 0.5 + warp2 * 0.3 + warp3 * 0.2
        
        # Scale by energy
        return warp_intensity * energy
    
    def _calculate_pulse_effect(self, position, frame_index, energy):
        """Calculate pulsing effect that intensifies with energy."""
        # Pulse frequency increases with energy
        frequency = 0.05 + energy * 0.15
        
        # Multiple pulse waves
        pulse1 = 0.5 + 0.5 * math.sin(frame_index * frequency * 2.0)
        pulse2 = 0.5 + 0.5 * math.sin(frame_index * frequency * 3.0 + 1.0)
        
        # Combine pulses
        pulse_intensity = pulse1 * 0.7 + pulse2 * 0.3
        
        # Apply position-based variation - stronger in the middle
        position_factor = 1.0 - abs(position - 0.5) * 0.5
        
        return pulse_intensity * energy * position_factor
    
    def _calculate_buildup_warning(self, position, frame_index, energy, segment):
        """Calculate warning flashes that precede the crash."""
        warning_intensity = 0
        
        if segment and frame_index > segment['buildup_end'] - 15 and frame_index < segment['buildup_end']:
            # Calculate time to crash
            time_to_crash = segment['buildup_end'] - frame_index
            
            # Warning flashes get more frequent as crash approaches
            if time_to_crash < 15:
                flash_frequency = 1.0 + (15 - time_to_crash) * 0.3
                warning_flash = 0.5 + 0.5 * math.sin(time_to_crash * flash_frequency)
                
                # Warning is more intense closer to the crash
                warning_intensity = warning_flash * (1.0 - time_to_crash / 15.0) * 1.5
        
        return warning_intensity
    
    def _apply_flicker(self, intensity):
        """Apply random flickering to intensity."""
        if self.flicker_amount <= 0:
            return intensity
        
        flicker = 1.0 - (random.random() * self.flicker_amount)
        return intensity * flicker
    
    def _apply_crash_effect(self, position, frame_index, energy, segment):
        """Apply dramatic crash effect."""
        if not segment or frame_index < segment['crash_start']:
            return 0.0
        
        # Time since crash started
        crash_time = frame_index - segment['crash_start']
        crash_duration = segment['crash_end'] - segment['crash_start']
        
        # Position of crash shockwave (moves out from center)
        crash_wave_position = crash_time / crash_duration * 0.6
        
        # Distance from crash wave
        distance = abs(position - 0.5) - crash_wave_position
        
        # Intensity based on distance from wave
        if distance < 0:
            # Inside the crash shockwave
            wave_intensity = 0.5 * (1.0 - crash_time / crash_duration)
            return wave_intensity
        elif distance < 0.05:
            # At the leading edge of the crash wave
            edge_intensity = (0.05 - distance) / 0.05
            return edge_intensity * 1.5
        
        return 0.0
    
    def _get_active_segment(self, frame_index):
        """Get the active segment for the current frame."""
        for point in self.key_points:
            if point['buildup_start'] <= frame_index < point['crash_end']:
                return point
        return None
    
    def _render_frame(self, frame_index):
        """Render a single frame of the animation."""
        # Get current energy level
        energy = self._get_energy_level(frame_index)
        
        # Get active segment
        active_segment = self._get_active_segment(frame_index)
        
        # Is this a crash frame?
        is_crash = active_segment and frame_index >= active_segment['crash_start']
        
        # Prepare LEDs for this frame
        leds = []
        
        # Process each LED
        for i in range(self.num_leds):
            position = i / (self.num_leds - 1)  # Normalize to 0-1
            
            # Get base intensity from various effects
            warp = self._calculate_warp_effect(position, frame_index, energy)
            pulse = self._calculate_pulse_effect(position, frame_index, energy)
            warning = self._calculate_buildup_warning(position, frame_index, energy, active_segment)
            crash = self._apply_crash_effect(position, frame_index, energy, active_segment)
            
            # Combine effects
            intensity = 0
            
            if is_crash:
                # During crash, the crash effect dominates
                intensity = crash * 2.0 + warp * 0.3
                color_choice = random.choice(self.colors[2:4])  # Use energy colors
            elif warning > 0:
                # Warning flashes
                intensity = warning * 1.5 + pulse * 0.3 + warp * 0.2
                color_choice = random.choice(self.colors[-2:])  # Use warning colors
            else:
                # Normal operation
                intensity = warp * 0.7 + pulse * 0.8
                
                # Color choice based on position and effects
                if pulse > warp:
                    color_choice = self.pulse_color if random.random() < 0.7 else random.choice(self.colors[2:6])
                else:
                    color_choice = self.accent_color if random.random() < 0.7 else random.choice(self.colors[4:8])
            
            # Apply overall energy scaling
            if not is_crash:
                intensity *= energy
            
            # Apply random flicker
            intensity = self._apply_flicker(intensity)
            
            # Skip if too dim
            if intensity < 0.05:
                continue
            
            # Apply final intensity to color
            color = (
                color_choice[0] * intensity,
                color_choice[1] * intensity,
                color_choice[2] * intensity
            )
            
            # Add to frame
            leds.append({
                "index": i,
                "color": self._color_to_hex(color)
            })
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete Sci-Fi Pulse animation."""
        # Clear any existing frames
        self.frames = []
        
        # Generate each frame
        for frame_index in range(self.simulation_steps):
            frame = self._render_frame(frame_index)
            self.frames.append(frame)
        
        # Create the animation JSON
        animation = {
            "name": "Sci-Fi Pulse Animation",
            "type": "custom",
            "speed": 30,  # 30ms per frame for smooth animation
            "direction": "forward",
            "frames": self.frames
        }
        
        return animation


# Generate different sci-fi pulse animations
def generate_scifi_animations():
    # Standard Sci-Fi Pulse
    standard = SciFiPulseGenerator(
        num_leds=60,
        simulation_steps=150,
        pulses=3,
        pulse_color="#00eeff",   # Cyan
        accent_color="#ff00ff",  # Magenta
        energy_build_rate=0.8,
        crash_speed=1.5,
        brightness=1.2,
        flicker_amount=0.3,
        warp_speed=1.5
    )
    standard_animation = standard.generate_animation()
    with open("scifi_pulse.json", "w") as f:
        json.dump(standard_animation, f, indent=2)
    
    # Hyperspace Jump
    hyperspace = SciFiPulseGenerator(
        num_leds=60,
        simulation_steps=120,
        pulses=1,                 # Single massive pulse
        pulse_color="#0077ff",    # Blue
        accent_color="#ffffff",   # White
        energy_build_rate=0.6,    # Slower buildup
        crash_speed=2.0,          # Faster crash
        brightness=1.5,           # Brighter
        flicker_amount=0.4,
        warp_speed=2.5            # Faster warp effect
    )
    hyperspace_animation = hyperspace.generate_animation()
    with open("hyperspace_jump.json", "w") as f:
        json.dump(hyperspace_animation, f, indent=2)
    
    # Reactor Overload
    reactor = SciFiPulseGenerator(
        num_leds=60,
        simulation_steps=180,
        pulses=5,                 # Multiple pulses
        pulse_color="#ff3300",    # Orange-red
        accent_color="#ffee00",   # Yellow
        energy_build_rate=0.9,    # Fast buildup
        crash_speed=1.2,
        brightness=1.3,
        flicker_amount=0.5,       # More flickering
        warp_speed=1.2
    )
    reactor_animation = reactor.generate_animation()
    with open("reactor_overload.json", "w") as f:
        json.dump(reactor_animation, f, indent=2)
    
    print("Generated 3 sci-fi animations:")
    print("1. Standard Sci-Fi Pulse")
    print("2. Hyperspace Jump")
    print("3. Reactor Overload")

# Run the generation
generate_scifi_animations()