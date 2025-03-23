#!/usr/bin/env python3
import json
import random
import argparse
from typing import List, Dict, Any, Tuple
import colorsys

class StarfieldGenerator:
    def __init__(
        self,
        length: int = 60,
        height: int = 1,
        num_stars: int = 15,
        num_frames: int = 20,
        speed: int = 120,
        color_mode: str = "white",
        color_range: List[str] = None,
        star_density: float = 0.25,
        twinkle_intensity: float = 0.7,
        use_2d: bool = False
    ):
        """
        Initialize the starfield animation generator.
        
        Args:
            length: Number of LEDs in length (x-direction)
            height: Number of LEDs in height (y-direction, 1 for a strip)
            num_stars: Maximum number of stars in any given frame
            num_frames: Number of frames in the animation
            speed: Animation speed in milliseconds per frame
            color_mode: "white", "blue", "multi", or "custom"
            color_range: List of custom colors if color_mode is "custom"
            star_density: Probability of a star appearing in any position (0.0-1.0)
            twinkle_intensity: How much the stars vary in brightness (0.0-1.0)
            use_2d: Whether to use 2D coordinates (x,y) or 1D (index)
        """
        self.length = length
        self.height = height
        self.num_stars = min(num_stars, length * height)
        self.num_frames = num_frames
        self.speed = speed
        self.color_mode = color_mode
        self.color_range = color_range or []
        self.star_density = min(1.0, max(0.0, star_density))
        self.twinkle_intensity = min(1.0, max(0.0, twinkle_intensity))
        self.use_2d = use_2d
        
        # Initialize star positions and maintain them throughout frames
        self.star_positions = self._generate_star_positions()
        
    def _generate_star_positions(self) -> List[Tuple[int, int]]:
        """Generate random positions for stars that will persist across frames."""
        positions = []
        total_positions = self.length * self.height
        num_positions = int(total_positions * self.star_density)
        
        # Ensure we don't exceed the number of available positions
        num_positions = min(num_positions, total_positions)
        
        # Create a list of all possible positions
        all_positions = []
        for y in range(self.height):
            for x in range(self.length):
                all_positions.append((x, y))
                
        # Randomly select positions
        return random.sample(all_positions, num_positions)
    
    def _get_color(self, brightness: float) -> str:
        """Get a color based on the color mode and brightness."""
        brightness = max(0.1, min(1.0, brightness))  # Limit to 10-100% brightness
        
        if self.color_mode == "white":
            # White stars with varying brightness
            intensity = int(255 * brightness)
            return f"#{intensity:02x}{intensity:02x}{intensity:02x}"
            
        elif self.color_mode == "blue":
            # Blue-white stars (more realistic)
            intensity = int(200 * brightness)
            blue_intensity = int(255 * brightness)
            return f"#{intensity:02x}{intensity:02x}{blue_intensity:02x}"
            
        elif self.color_mode == "multi":
            # Multicolor stars with different hues
            hue = random.random()  # Random hue between 0-1
            rgb = colorsys.hsv_to_rgb(hue, 0.3 * random.random(), brightness)
            r, g, b = [int(255 * c) for c in rgb]
            return f"#{r:02x}{g:02x}{b:02x}"
            
        elif self.color_mode == "custom" and self.color_range:
            # Pick from custom colors
            base_color = random.choice(self.color_range)
            # Adjust brightness of the selected color
            if base_color.startswith('#'):
                base_color = base_color[1:]
            r = int(base_color[0:2], 16)
            g = int(base_color[2:4], 16)
            b = int(base_color[4:6], 16)
            
            r = int(r * brightness)
            g = int(g * brightness)
            b = int(b * brightness)
            
            return f"#{r:02x}{g:02x}{b:02x}"
        
        # Default fallback
        intensity = int(255 * brightness)
        return f"#{intensity:02x}{intensity:02x}{intensity:02x}"
    
    def _convert_to_1d_index(self, x: int, y: int) -> int:
        """Convert 2D coordinates to 1D index."""
        return y * self.length + x
    
    def generate_frame(self, frame_num: int) -> Dict[str, List[Dict[str, Any]]]:
        """Generate a single frame of the starfield animation."""
        leds = []
        
        # Use the pre-defined star positions
        for x, y in self.star_positions:
            # Each star should twinkle independently
            # Use a different seed for each star's brightness pattern
            seed = hash((x, y, frame_num))
            random.seed(seed)
            
            # Calculate brightness with some randomness for twinkling
            base_brightness = 0.3 + (0.7 * random.random())
            
            # Apply twinkle intensity - higher values make brightness vary more
            twinkle_factor = random.random() * self.twinkle_intensity
            brightness = base_brightness * (1.0 - twinkle_factor + (2 * twinkle_factor * random.random()))
            
            # Some stars might be invisible in some frames for more realism
            if random.random() > 0.1:  # 90% chance of being visible
                # Get color based on brightness
                color = self._get_color(brightness)
                
                # Add the LED to the frame
                if self.use_2d:
                    leds.append({"x": x, "y": y, "color": color})
                else:
                    index = self._convert_to_1d_index(x, y)
                    leds.append({"index": index, "color": color})
        
        # Reset the global random seed
        random.seed()
        
        return {"leds": leds}
    
    def generate_animation(self) -> Dict[str, Any]:
        """Generate the complete starfield animation."""
        frames = []
        for i in range(self.num_frames):
            frames.append(self.generate_frame(i))
        
        animation = {
            "animations": [
                {
                    "name": "Twinkling Starfield",
                    "type": "custom",
                    "speed": self.speed,
                    "direction": "forward",
                    "dimensions": {
                        "length": self.length,
                        "height": self.height
                    },
                    "frames": frames
                }
            ]
        }
        
        return animation

def parse_args():
    """Parse command line arguments."""
    parser = argparse.ArgumentParser(description='Generate a twinkling starfield animation for LED displays')
    
    parser.add_argument('--length', type=int, default=60, 
                        help='Number of LEDs in length (x-direction)')
    parser.add_argument('--height', type=int, default=1, 
                        help='Number of LEDs in height (y-direction, 1 for a strip)')
    parser.add_argument('--num-stars', type=int, default=15, 
                        help='Maximum number of stars')
    parser.add_argument('--num-frames', type=int, default=20, 
                        help='Number of frames in the animation')
    parser.add_argument('--speed', type=int, default=120, 
                        help='Animation speed in milliseconds per frame')
    parser.add_argument('--color-mode', choices=['white', 'blue', 'multi', 'custom'], default='white', 
                        help='Color mode for stars')
    parser.add_argument('--colors', type=str, nargs='+', 
                        help='Custom colors for stars (when color-mode is custom)')
    parser.add_argument('--star-density', type=float, default=0.25, 
                        help='Probability of a star appearing (0.0-1.0)')
    parser.add_argument('--twinkle-intensity', type=float, default=0.7, 
                        help='How much the stars vary in brightness (0.0-1.0)')
    parser.add_argument('--use-2d', action='store_true', 
                        help='Use 2D coordinates (x,y) instead of 1D index')
    parser.add_argument('--output', type=str, default='starfield_animation.json', 
                        help='Output file path')
    
    return parser.parse_args()

def main():
    args = parse_args()
    
    # Create the generator
    generator = StarfieldGenerator(
        length=args.length,
        height=args.height,
        num_stars=args.num_stars,
        num_frames=args.num_frames,
        speed=args.speed,
        color_mode=args.color_mode,
        color_range=args.colors,
        star_density=args.star_density,
        twinkle_intensity=args.twinkle_intensity,
        use_2d=args.use_2d
    )
    
    # Generate the animation
    animation = generator.generate_animation()
    
    # Write the animation to file
    with open(args.output, 'w') as f:
        json.dump(animation, f, indent=2)
    
    print(f"Generated starfield animation with {args.num_frames} frames")
    print(f"Output written to: {args.output}")

if __name__ == "__main__":
    main()