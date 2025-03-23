import json
import random
import math

class Snake2D:
    def __init__(self, width=30, height=2, frames=120, speed=50):
        self.width = width
        self.height = height
        self.frames = frames
        self.speed = speed
        
        # Snake properties
        self.snake = [(width//2, height//2)]  # Start in middle
        self.direction = (1, 0)  # Start moving right
        self.food = None
        self.place_food()
        
        # Colors
        self.head_color = "#00ffff"  # Cyan
        self.body_color = "#00ff00"  # Green
        self.food_color = "#ff0000"  # Red
    
    def place_food(self):
        """Place food somewhere not occupied by snake"""
        available = []
        for x in range(self.width):
            for y in range(self.height):
                if (x, y) not in self.snake:
                    available.append((x, y))
        
        if available:
            self.food = random.choice(available)
    
    def move_snake(self):
        """Move snake one step in current direction"""
        head_x, head_y = self.snake[0]
        dx, dy = self.direction
        
        # Calculate new head position
        new_x = (head_x + dx) % self.width  # Wrap around edges
        new_y = (head_y + dy) % self.height
        
        # Check if eating food
        if (new_x, new_y) == self.food:
            # Grow snake (don't remove tail)
            self.snake.insert(0, (new_x, new_y))
            self.place_food()
        else:
            # Move snake (remove tail)
            self.snake.insert(0, (new_x, new_y))
            self.snake.pop()
        
        # Check for collision with self (reset if collision)
        if self.snake[0] in self.snake[1:]:
            self.snake = [self.snake[0]]  # Keep only head
    
    def change_direction(self):
        """Randomly change direction occasionally"""
        if random.random() < 0.1:  # 10% chance to change direction
            dx, dy = self.direction
            # Don't allow 180° turns
            possible = [(1,0), (0,1), (-1,0), (0,-1)]
            possible.remove((-dx, -dy))  # Remove opposite direction
            self.direction = random.choice(possible)
    
    def generate_frame(self):
        """Generate a single frame for the animation"""
        leds = []
        
        # Add snake body
        for i, (x, y) in enumerate(self.snake):
            color = self.head_color if i == 0 else self.body_color
            leds.append({"x": x, "y": y, "color": color})
        
        # Add food
        if self.food:
            x, y = self.food
            leds.append({"x": x, "y": y, "color": self.food_color})
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete animation"""
        all_frames = []
        
        # Create each frame
        for _ in range(self.frames):
            self.change_direction()
            self.move_snake()
            all_frames.append(self.generate_frame())
        
        # Build the animation JSON
        animation = {
            "name": "2D Snake Game",
            "type": "custom",
            "speed": self.speed,
            "direction": "forward",
            "dimensions": {
                "length": self.width,
                "height": self.height
            },
            "frames": all_frames
        }
        
        return animation

# Generate animations for different grid sizes
def generate_animations():
    configs = [
        {"width": 60, "height": 1, "name": "snake_60x1.json"},
        {"width": 30, "height": 2, "name": "snake_30x2.json"},
        {"width": 15, "height": 4, "name": "snake_15x4.json"},
        {"width": 10, "height": 6, "name": "snake_10x6.json"}
    ]
    
    for config in configs:
        snake = Snake2D(
            width=config["width"], 
            height=config["height"], 
            frames=120, 
            speed=50
        )
        animation = snake.generate_animation()
        
        with open(config["name"], "w") as f:
            json.dump(animation, f, indent=2)
    
    # Return an example for display
    example = Snake2D(width=30, height=2, frames=1, speed=50)
    example.snake = [(5, 0), (4, 0), (3, 0), (2, 0)]
    example.food = (15, 1)
    return example.generate_animation()

# Generate all animations and return an example
example_animation = generate_animations()
print(json.dumps(example_animation, indent=2))