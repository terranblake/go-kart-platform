import json
import random
import math

class PongAnimation:
    def __init__(self, 
                 width=60,              # Width of the grid (x dimension)
                 height=2,              # Height of the grid (y dimension)
                 frames=150,            # Number of frames to generate
                 ball_speed=1.0,        # Base ball speed
                 paddle_size=3,         # Size of paddles
                 ai_difficulty=0.8,     # AI difficulty (0-1)
                 ball_color="#ffffff",  # Ball color
                 paddle1_color="#ff0000", # Left/top paddle color
                 paddle2_color="#0000ff", # Right/bottom paddle color
                 orientation="horizontal", # "horizontal" or "vertical"
                 speed=50):             # Frame speed in ms
        self.width = width
        self.height = height
        self.frames = frames
        self.ball_speed = ball_speed
        self.paddle_size = min(paddle_size, height if orientation == "horizontal" else width)
        self.ai_difficulty = max(0.1, min(1.0, ai_difficulty))
        self.ball_color = ball_color
        self.paddle1_color = paddle1_color
        self.paddle2_color = paddle2_color
        self.frame_speed = speed
        self.orientation = orientation
        
        # Initialize game state
        self.reset_game()
    
    def reset_game(self):
        """Reset the game state to starting position"""
        # Ball position (center of field)
        self.ball_x = self.width / 2
        self.ball_y = self.height / 2
        
        # Ball velocity (random direction)
        angle = random.uniform(math.pi/4, 3*math.pi/4)
        if random.random() > 0.5:
            angle = math.pi - angle  # Sometimes go left
        
        self.ball_vx = math.cos(angle) * self.ball_speed
        self.ball_vy = math.sin(angle) * self.ball_speed
        
        # Paddle positions (centered on their sides)
        if self.orientation == "horizontal":
            # Paddles on left and right sides
            self.paddle1_pos = self.height / 2  # Y position of paddle1
            self.paddle2_pos = self.height / 2  # Y position of paddle2
        else:
            # Paddles on top and bottom
            self.paddle1_pos = self.width / 2   # X position of paddle1
            self.paddle2_pos = self.width / 2   # X position of paddle2
        
        # Score
        self.score1 = 0
        self.score2 = 0
    
    def update_game(self):
        """Update game state for one frame"""
        # Update ball position
        self.ball_x += self.ball_vx
        self.ball_y += self.ball_vy
        
        # Handle ball collision with top/bottom or left/right walls
        if self.orientation == "horizontal":
            # Top/bottom walls
            if self.ball_y < 0:
                self.ball_y = -self.ball_y
                self.ball_vy = -self.ball_vy
            elif self.ball_y >= self.height:
                self.ball_y = 2 * self.height - self.ball_y
                self.ball_vy = -self.ball_vy
        else:
            # Left/right walls for vertical orientation
            if self.ball_x < 0:
                self.ball_x = -self.ball_x
                self.ball_vx = -self.ball_vx
            elif self.ball_x >= self.width:
                self.ball_x = 2 * self.width - self.ball_x
                self.ball_vx = -self.ball_vx
        
        # Handle AI paddle movement
        self.update_ai_paddle()
        
        # Handle ball collision with paddles
        self.handle_paddle_collisions()
        
        # Check for scoring
        if self.check_scoring():
            # If someone scored, we'll reset the ball
            self.reset_ball()
    
    def update_ai_paddle(self):
        """Update AI paddle positions based on ball position"""
        # Both paddles are AI-controlled in this simulation
        if self.orientation == "horizontal":
            # For horizontal layout - paddles move up/down
            # Paddle 1 (left)
            if self.ball_vx < 0:  # Ball coming towards paddle 1
                # Calculate ideal position with some error based on difficulty
                target = self.ball_y
                error = (1.0 - self.ai_difficulty) * self.height * 0.5 * random.uniform(-1, 1)
                target += error
                
                # Move towards target
                if abs(self.paddle1_pos - target) > 0.2:
                    if self.paddle1_pos < target:
                        self.paddle1_pos += min(0.3, target - self.paddle1_pos)
                    else:
                        self.paddle1_pos -= min(0.3, self.paddle1_pos - target)
            
            # Paddle 2 (right)
            if self.ball_vx > 0:  # Ball coming towards paddle 2
                # Calculate ideal position with some error based on difficulty
                target = self.ball_y
                error = (1.0 - self.ai_difficulty) * self.height * 0.5 * random.uniform(-1, 1)
                target += error
                
                # Move towards target
                if abs(self.paddle2_pos - target) > 0.2:
                    if self.paddle2_pos < target:
                        self.paddle2_pos += min(0.3, target - self.paddle2_pos)
                    else:
                        self.paddle2_pos -= min(0.3, self.paddle2_pos - target)
        else:
            # For vertical layout - paddles move left/right
            # Paddle 1 (top)
            if self.ball_vy < 0:  # Ball coming towards paddle 1
                # Calculate ideal position with some error based on difficulty
                target = self.ball_x
                error = (1.0 - self.ai_difficulty) * self.width * 0.5 * random.uniform(-1, 1)
                target += error
                
                # Move towards target
                if abs(self.paddle1_pos - target) > 0.2:
                    if self.paddle1_pos < target:
                        self.paddle1_pos += min(0.3, target - self.paddle1_pos)
                    else:
                        self.paddle1_pos -= min(0.3, self.paddle1_pos - target)
            
            # Paddle 2 (bottom)
            if self.ball_vy > 0:  # Ball coming towards paddle 2
                # Calculate ideal position with some error based on difficulty
                target = self.ball_x
                error = (1.0 - self.ai_difficulty) * self.width * 0.5 * random.uniform(-1, 1)
                target += error
                
                # Move towards target
                if abs(self.paddle2_pos - target) > 0.2:
                    if self.paddle2_pos < target:
                        self.paddle2_pos += min(0.3, target - self.paddle2_pos)
                    else:
                        self.paddle2_pos -= min(0.3, self.paddle2_pos - target)
    
    def handle_paddle_collisions(self):
        """Handle ball collisions with paddles"""
        if self.orientation == "horizontal":
            # Horizontal layout: paddles on left and right
            half_paddle = self.paddle_size / 2
            
            # Left paddle
            if (self.ball_x <= 1 and self.ball_vx < 0 and 
                self.ball_y >= self.paddle1_pos - half_paddle and 
                self.ball_y <= self.paddle1_pos + half_paddle):
                # Bounce off left paddle
                self.ball_x = 2 - self.ball_x  # Adjust position
                self.ball_vx = -self.ball_vx  # Reverse x direction
                
                # Adjust angle based on where the ball hit the paddle
                hit_pos = (self.ball_y - (self.paddle1_pos - half_paddle)) / self.paddle_size
                self.ball_vy += (hit_pos - 0.5) * 2 * self.ball_speed
            
            # Right paddle
            if (self.ball_x >= self.width - 1 and self.ball_vx > 0 and 
                self.ball_y >= self.paddle2_pos - half_paddle and 
                self.ball_y <= self.paddle2_pos + half_paddle):
                # Bounce off right paddle
                self.ball_x = 2 * (self.width - 1) - self.ball_x  # Adjust position
                self.ball_vx = -self.ball_vx  # Reverse x direction
                
                # Adjust angle based on where the ball hit the paddle
                hit_pos = (self.ball_y - (self.paddle2_pos - half_paddle)) / self.paddle_size
                self.ball_vy += (hit_pos - 0.5) * 2 * self.ball_speed
        else:
            # Vertical layout: paddles on top and bottom
            half_paddle = self.paddle_size / 2
            
            # Top paddle
            if (self.ball_y <= 1 and self.ball_vy < 0 and 
                self.ball_x >= self.paddle1_pos - half_paddle and 
                self.ball_x <= self.paddle1_pos + half_paddle):
                # Bounce off top paddle
                self.ball_y = 2 - self.ball_y  # Adjust position
                self.ball_vy = -self.ball_vy  # Reverse y direction
                
                # Adjust angle based on where the ball hit the paddle
                hit_pos = (self.ball_x - (self.paddle1_pos - half_paddle)) / self.paddle_size
                self.ball_vx += (hit_pos - 0.5) * 2 * self.ball_speed
            
            # Bottom paddle
            if (self.ball_y >= self.height - 1 and self.ball_vy > 0 and 
                self.ball_x >= self.paddle2_pos - half_paddle and 
                self.ball_x <= self.paddle2_pos + half_paddle):
                # Bounce off bottom paddle
                self.ball_y = 2 * (self.height - 1) - self.ball_y  # Adjust position
                self.ball_vy = -self.ball_vy  # Reverse y direction
                
                # Adjust angle based on where the ball hit the paddle
                hit_pos = (self.ball_x - (self.paddle2_pos - half_paddle)) / self.paddle_size
                self.ball_vx += (hit_pos - 0.5) * 2 * self.ball_speed
    
    def check_scoring(self):
        """Check if someone scored a point"""
        if self.orientation == "horizontal":
            # Ball went past the left paddle
            if self.ball_x < 0:
                self.score2 += 1
                return True
            
            # Ball went past the right paddle
            if self.ball_x >= self.width:
                self.score1 += 1
                return True
        else:
            # Ball went past the top paddle
            if self.ball_y < 0:
                self.score2 += 1
                return True
            
            # Ball went past the bottom paddle
            if self.ball_y >= self.height:
                self.score1 += 1
                return True
        
        return False
    
    def reset_ball(self):
        """Reset ball to center with a new direction"""
        self.ball_x = self.width / 2
        self.ball_y = self.height / 2
        
        # Random direction
        angle = random.uniform(math.pi/4, 3*math.pi/4)
        if random.random() > 0.5:
            angle = math.pi - angle
        
        self.ball_vx = math.cos(angle) * self.ball_speed
        self.ball_vy = math.sin(angle) * self.ball_speed
    
    def generate_frame(self):
        """Generate a single frame for the animation"""
        leds = []
        
        # Add ball (rounded to nearest integer position)
        ball_x_int = round(self.ball_x)
        ball_y_int = round(self.ball_y)
        
        # Only add if ball is within bounds
        if 0 <= ball_x_int < self.width and 0 <= ball_y_int < self.height:
            leds.append({
                "x": ball_x_int,
                "y": ball_y_int,
                "color": self.ball_color
            })
        
        # Add paddles
        if self.orientation == "horizontal":
            # Left paddle
            half_paddle = self.paddle_size // 2
            for i in range(-half_paddle, half_paddle + 1):
                paddle_y = round(self.paddle1_pos + i)
                if 0 <= paddle_y < self.height:
                    leds.append({
                        "x": 0,
                        "y": paddle_y,
                        "color": self.paddle1_color
                    })
            
            # Right paddle
            for i in range(-half_paddle, half_paddle + 1):
                paddle_y = round(self.paddle2_pos + i)
                if 0 <= paddle_y < self.height:
                    leds.append({
                        "x": self.width - 1,
                        "y": paddle_y,
                        "color": self.paddle2_color
                    })
        else:
            # Top paddle
            half_paddle = self.paddle_size // 2
            for i in range(-half_paddle, half_paddle + 1):
                paddle_x = round(self.paddle1_pos + i)
                if 0 <= paddle_x < self.width:
                    leds.append({
                        "x": paddle_x,
                        "y": 0,
                        "color": self.paddle1_color
                    })
            
            # Bottom paddle
            for i in range(-half_paddle, half_paddle + 1):
                paddle_x = round(self.paddle2_pos + i)
                if 0 <= paddle_x < self.width:
                    leds.append({
                        "x": paddle_x,
                        "y": self.height - 1,
                        "color": self.paddle2_color
                    })
        
        return {"leds": leds}
    
    def generate_animation(self):
        """Generate the complete Pong animation"""
        all_frames = []
        
        # Generate each frame
        for _ in range(self.frames):
            self.update_game()
            all_frames.append(self.generate_frame())
        
        # Create the animation JSON
        animation = {
            "name": f"Pong Game ({self.orientation})",
            "type": "custom",
            "speed": self.frame_speed,
            "direction": "forward",
            "dimensions": {
                "length": self.width,
                "height": self.height
            },
            "frames": all_frames
        }
        
        return animation

# Generate different Pong animations
def generate_pong_animations():
    # Horizontal pong for 60x1 layout
    horizontal_1row = PongAnimation(
        width=60,
        height=1,
        frames=200,
        ball_speed=4.0,
        paddle_size=1,
        ai_difficulty=0.7,
        orientation="horizontal",
        speed=40
    )
    h1_animation = horizontal_1row.generate_animation()
    with open("pong_60x1_horizontal.json", "w") as f:
        json.dump(h1_animation, f, indent=2)
    
    # Horizontal pong for 30x2 layout
    horizontal_2row = PongAnimation(
        width=30,
        height=2,
        frames=200,
        ball_speed=0.7,
        paddle_size=2,
        ai_difficulty=0.8,
        orientation="horizontal",
        speed=40
    )
    h2_animation = horizontal_2row.generate_animation()
    with open("pong_30x2_horizontal.json", "w") as f:
        json.dump(h2_animation, f, indent=2)
    
    # Vertical pong for 60x1 layout
    vertical_1row = PongAnimation(
        width=60,
        height=1,
        frames=200,
        ball_speed=4.0,
        paddle_size=5,
        ai_difficulty=0.7,
        orientation="vertical",
        speed=40
    )
    v1_animation = vertical_1row.generate_animation()
    with open("pong_60x1_vertical.json", "w") as f:
        json.dump(v1_animation, f, indent=2)
    
    # Vertical pong for 15x4 layout
    vertical_4row = PongAnimation(
        width=15,
        height=4,
        frames=200,
        ball_speed=0.6,
        paddle_size=3,
        ai_difficulty=0.8,
        orientation="vertical",
        speed=40
    )
    v4_animation = vertical_4row.generate_animation()
    with open("pong_15x4_vertical.json", "w") as f:
        json.dump(v4_animation, f, indent=2)
    
    # Return an example for display
    example = PongAnimation(width=30, height=2, frames=1)
    example.ball_x = 15
    example.ball_y = 1
    example.paddle1_pos = 1
    example.paddle2_pos = 0
    return example.generate_animation()

# Generate animations and return example
example_animation = generate_pong_animations()
print(json.dumps(example_animation, indent=2))