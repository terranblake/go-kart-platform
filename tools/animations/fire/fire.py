import random
import json
import math
import time
from collections import deque

class FireSimulation:
    def __init__(self, num_leds=60, fire_cooling=55, fire_sparking=120, simulation_steps=40,
                 color_temp=1800, intensity=1.0, color_palette_size=100, 
                 red_bias=1.0, yellow_bias=1.0, blue_accent=0.0, white_tip=True,
                 ember_glow=0.3, flare_frequency=0.1, flare_intensity=1.0):
        """
        Initialize the fire simulation with enhanced controls.
        
        Parameters:
        - num_leds: Number of LEDs in the strip
        - fire_cooling: How much the fire cools down between frames (higher = cooler)
        - fire_sparking: Chance of new sparks at the base (higher = more sparks)
        - simulation_steps: Number of frames to generate
        - color_temp: Color temperature in Kelvin (lower = redder, higher = yellower)
                      Typical fire: 1500-2500K
        - intensity: Overall brightness multiplier (0.0-2.0)
        - color_palette_size: Number of colors in the palette (higher = smoother gradient)
        - red_bias: Multiplier for red component (0.5-2.0)
        - yellow_bias: Multiplier for yellow component (0.5-2.0)
        - blue_accent: Amount of blue to add to the flames (0.0-0.3)
        - white_tip: Whether to have white tips on the flames (boolean)
        - ember_glow: Intensity of the ember glow at the base (0.0-1.0)
        - flare_frequency: How often flares occur (0.0-1.0)
        - flare_intensity: How bright the flares are (0.5-2.0)
        """
        self.num_leds = num_leds
        self.fire_cooling = fire_cooling
        self.fire_sparking = fire_sparking
        self.heat = [0] * num_leds  # Array of temperature values
        self.frames = []
        self.simulation_steps = simulation_steps
        
        # Additional color and intensity parameters
        self.color_temp = max(1000, min(10000, color_temp))  # Clamp to reasonable range
        self.intensity = max(0.1, min(2.0, intensity))
        self.red_bias = max(0.5, min(2.0, red_bias))
        self.yellow_bias = max(0.5, min(2.0, yellow_bias))
        self.blue_accent = max(0.0, min(0.3, blue_accent))
        self.white_tip = white_tip
        self.ember_glow = max(0.0, min(1.0, ember_glow))
        self.flare_frequency = max(0.0, min(1.0, flare_frequency))
        self.flare_intensity = max(0.5, min(2.0, flare_intensity))
        
        # Color palette from deep red to bright yellow
        self.palette = self._create_fire_palette(color_palette_size)
        
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
        
        # Ember glow at the bottom (first 10% of palette)
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
        if self.white_tip:
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
    
    def _cool_down(self):
        """Cool down the fire by random amounts."""
        for i in range(self.num_leds):
            # The closer to the top, the more it cools
            # This creates the natural tapering of the flames
            position_ratio = i / self.num_leds
            reversed_position = 1.0 - position_ratio
            
            # Base cooling increases with height
            base_cooling = reversed_position * 1.5
            
            # Add some noise to the cooling rate
            # More noise = more dynamic flickering
            noise_factor = 1.0 + self._random_between(-0.2, 0.2)
            cooling_factor = base_cooling * noise_factor
            
            # Apply fire_cooling parameter
            random_cooling = self._random_between(0, self.fire_cooling * cooling_factor / 10)
            
            # Cool down this pixel - the amount of cooling determines flame height
            self.heat[i] = max(0, self.heat[i] - random_cooling)
            
            # Occasionally add some cooling "pockets" to break up uniform patterns
            if random.random() < 0.03:  # 3% chance per pixel per frame
                extra_cooling = self._random_between(0, self.fire_cooling / 5)
                self.heat[i] = max(0, self.heat[i] - extra_cooling)
    
    def _spread_fire(self):
        """Spread the fire up from the bottom."""
        # Heat rises - copy values to neighboring cells with some blending
        for i in range(self.num_leds - 1, 1, -1):
            # Simple spreading algorithm - heat spreads up
            # The more sophisticated versions might include side-spread and more complex diffusion
            self.heat[i] = (self.heat[i - 1] + self.heat[i - 2] + self.heat[i - 3]) / 3.0
        
        # Add drift and noise to make it more dynamic
        for i in range(3, self.num_leds):
            if random.random() < 0.75:  # 75% chance of adding noise
                drift = self._random_between(-5, 5)
                self.heat[i] = max(0, min(255, self.heat[i] + drift))
    
    def _add_sparks(self):
        """Add new sparks at the bottom of the fire."""
        # Chance of a new spark at the first few LEDs
        for i in range(3):  # The first 3 LEDs (base of fire)
            if random.random() * 255 < self.fire_sparking:
                # New spark with random intensity
                spark_heat = self._random_between(180, 255)
                self.heat[i] = min(self.heat[i] + spark_heat, 255)
    
    def _add_random_flares(self):
        """Add random flares throughout the fire to make it more dynamic."""
        # Sometimes add bright flares in the middle of the fire
        if random.random() < self.flare_frequency:
            # Position range depends on fire height
            min_pos = 3
            max_pos = max(min_pos + 1, int(self.num_leds * 0.6))  # Can go higher with more intensity
            flare_pos = random.randint(min_pos, max_pos)
            
            # Intensity varies with the flare_intensity parameter
            base_intensity = self._random_between(80, 180)
            flare_intensity = base_intensity * self.flare_intensity
            
            # Flare radius depends on intensity
            flare_radius = int(2 + self.flare_intensity * 2)
            
            # Add a flare that affects nearby LEDs
            for i in range(max(0, flare_pos - flare_radius), min(self.num_leds, flare_pos + flare_radius + 1)):
                # Intensity falls off with distance from flare center
                distance = abs(i - flare_pos)
                # Gaussian-like falloff for more natural flare shape
                falloff = math.exp(-(distance * distance) / (2 * (flare_radius/2) * (flare_radius/2)))
                intensity_addition = flare_intensity * falloff
                
                self.heat[i] = min(255, self.heat[i] + intensity_addition)
    
    def _update_fire(self):
        """Update the fire simulation for one frame."""
        # Step 1: Cool down the fire
        self._cool_down()
        
        # Step 2: Spread the fire upwards
        self._spread_fire()
        
        # Step 3: Add new sparks at the bottom
        self._add_sparks()
        
        # Step 4: Add random flares for more dynamics
        self._add_random_flares()
    
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
        
        # Start with a small fire at the bottom
        for i in range(5):
            self.heat[i] = random.randint(100, 200)
        
        # Generate frames
        for _ in range(self.simulation_steps):
            self._update_fire()
            self.frames.append(self._get_frame())
        
        # Create the animation JSON
        animation = {
            "name": "Realistic Fire Effect",
            "type": "custom",
            "speed": 50,  # 50ms per frame
            "direction": "forward",
            "frames": self.frames
        }
        
        return animation

# Example: Create and run different fire simulations with various settings

# 1. Classic fire (default settings)
classic_fire = FireSimulation(
    num_leds=60, 
    fire_cooling=55, 
    fire_sparking=120, 
    simulation_steps=60
)
classic_animation = classic_fire.generate_animation()
with open("classic_fire.json", "w") as f:
    json.dump(classic_animation, f, indent=2)

# 2. Hot blue-tipped fire
blue_fire = FireSimulation(
    num_leds=60,
    fire_cooling=40,            # Less cooling = taller flames
    fire_sparking=150,          # More sparks
    simulation_steps=60,
    color_temp=2200,            # Hotter color temperature
    intensity=1.2,              # Brighter
    blue_accent=0.15,           # Add blue accent
    white_tip=True,             # White/blue tips
    flare_frequency=0.15,       # More frequent flares
    flare_intensity=1.5         # More intense flares
)
blue_animation = blue_fire.generate_animation()
with open("blue_fire.json", "w") as f:
    json.dump(blue_animation, f, indent=2)

# 3. Deep red ember fire
ember_fire = FireSimulation(
    num_leds=60,
    fire_cooling=70,            # More cooling = shorter flames
    fire_sparking=90,           # Fewer sparks
    simulation_steps=60,
    color_temp=1500,            # Cooler color temperature (redder)
    intensity=0.9,              # Slightly dimmer
    red_bias=1.5,               # More red
    yellow_bias=0.7,            # Less yellow
    blue_accent=0.0,            # No blue
    white_tip=False,            # No white tips
    ember_glow=0.8,             # Strong ember glow
    flare_frequency=0.05,       # Few flares
    flare_intensity=0.8         # Less intense flares
)
ember_animation = ember_fire.generate_animation()
with open("ember_fire.json", "w") as f:
    json.dump(ember_animation, f, indent=2)

# 4. Dynamic tall yellow fire
tall_fire = FireSimulation(
    num_leds=60,
    fire_cooling=30,            # Much less cooling = very tall flames
    fire_sparking=180,          # Many sparks
    simulation_steps=60,
    color_temp=2000,            # Medium-hot color temperature
    intensity=1.3,              # Quite bright
    red_bias=0.9,               # Less red bias
    yellow_bias=1.3,            # More yellow bias
    blue_accent=0.05,           # Slight blue accent
    white_tip=True,             # White tips
    ember_glow=0.4,             # Medium ember glow
    flare_frequency=0.2,        # Frequent flares
    flare_intensity=1.7         # Very intense flares
)
tall_animation = tall_fire.generate_animation()
with open("tall_fire.json", "w") as f:
    json.dump(tall_animation, f, indent=2)

print("Generated 4 different fire animations:")
print("1. Classic Fire")
print("2. Hot Blue-tipped Fire")
print("3. Deep Red Ember Fire")
print("4. Dynamic Tall Yellow Fire")