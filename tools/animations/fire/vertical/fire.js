// Fire Animation Generator for 60-LED strip
// Uses Perlin noise for natural fire movement simulation

const { writeFileSync } = require('fs');

// Perlin noise implementation (simplified)
class PerlinNoise {
  constructor() {
    this.permutation = [];
    for (let i = 0; i < 256; i++) {
      this.permutation.push(i);
    }
    // Shuffle the array
    for (let i = 0; i < 256; i++) {
      const j = Math.floor(Math.random() * 256);
      [this.permutation[i], this.permutation[j]] = [this.permutation[j], this.permutation[i]];
    }
    // Duplicate the permutation
    this.permutation = [...this.permutation, ...this.permutation];
  }

  fade(t) {
    return t * t * t * (t * (t * 6 - 15) + 10);
  }

  lerp(t, a, b) {
    return a + t * (b - a);
  }

  grad(hash, x, y, z) {
    const h = hash & 15;
    const u = h < 8 ? x : y;
    const v = h < 4 ? y : h === 12 || h === 14 ? x : z;
    return ((h & 1) === 0 ? u : -u) + ((h & 2) === 0 ? v : -v);
  }

  noise(x, y = 0, z = 0) {
    const X = Math.floor(x) & 255;
    const Y = Math.floor(y) & 255;
    const Z = Math.floor(z) & 255;

    x -= Math.floor(x);
    y -= Math.floor(y);
    z -= Math.floor(z);

    const u = this.fade(x);
    const v = this.fade(y);
    const w = this.fade(z);

    const A = this.permutation[X] + Y;
    const AA = this.permutation[A] + Z;
    const AB = this.permutation[A + 1] + Z;
    const B = this.permutation[X + 1] + Y;
    const BA = this.permutation[B] + Z;
    const BB = this.permutation[B + 1] + Z;

    return this.lerp(
      w,
      this.lerp(
        v,
        this.lerp(
          u,
          this.grad(this.permutation[AA], x, y, z),
          this.grad(this.permutation[BA], x - 1, y, z)
        ),
        this.lerp(
          u,
          this.grad(this.permutation[AB], x, y - 1, z),
          this.grad(this.permutation[BB], x - 1, y - 1, z)
        )
      ),
      this.lerp(
        v,
        this.lerp(
          u,
          this.grad(this.permutation[AA + 1], x, y, z - 1),
          this.grad(this.permutation[BA + 1], x - 1, y, z - 1)
        ),
        this.lerp(
          u,
          this.grad(this.permutation[AB + 1], x, y - 1, z - 1),
          this.grad(this.permutation[BB + 1], x - 1, y - 1, z - 1)
        )
      )
    );
  }
}

// Function to convert HSV to RGB color
function hsvToRgb(h, s, v) {
  let r, g, b;
  const i = Math.floor(h * 6);
  const f = h * 6 - i;
  const p = v * (1 - s);
  const q = v * (1 - f * s);
  const t = v * (1 - (1 - f) * s);

  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }

  return {
    r: Math.round(r * 255),
    g: Math.round(g * 255),
    b: Math.round(b * 255)
  };
}

// Convert RGB to hex color
function rgbToHex(r, g, b) {
  return '#' + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
}

// Generate fire color based on height and noise
function getFireColor(height, noiseValue) {
  // Enhanced fire color palette with wider range
  
  // Higher heat range for more vibrant flames
  // Base hue can go from deep red (0.0) to orange-yellow (0.12)
  const h = Math.max(0, Math.min(0.12, 0.11 - height * 0.11)); 
  
  // More yellow/orange in the middle, deeper reds at the edges
  // Add noise influence to saturation for color variation
  const s = Math.min(1.0, 0.7 + (1.0 - height) * 0.3 - noiseValue * 0.1);
  
  // Brightness based on both height and noise
  // More variance in brightness creates better flickering effect
  const v = Math.min(1.0, 0.5 + height * 1.1 + noiseValue * 0.7);
  
  // Apply a slight random variation to each parameter for more natural look
  const hVar = h + (Math.random() * 0.01 - 0.005);
  const sVar = Math.min(1.0, Math.max(0.5, s + (Math.random() * 0.05 - 0.025)));
  const vVar = Math.min(1.0, Math.max(0.3, v + (Math.random() * 0.1 - 0.05)));
  
  const rgb = hsvToRgb(hVar, sVar, vVar);
  return rgbToHex(rgb.r, rgb.g, rgb.b);
}

// Generate a fire animation frame
function generateFireFrame(perlin, time, numLeds) {
  const leds = [];
  const coolingFactor = 0.9; // How quickly the fire cools at the top (reduced for more even effect)
  
  // Create mirrored effect for both sides of the strip
  for (let i = 0; i < numLeds; i++) {
    // Map LEDs to center-outward position (middle is hottest)
    // This creates two fire sources, one at each end of the strip
    const centerDistance = Math.abs((i - (numLeds / 2)) / (numLeds / 2));
    
    // Convert to a height value (1.0 at ends, 0.0 at center)
    // This makes the "height" calculation work for both sides
    const position = centerDistance;
    
    // Invert position for fire effect (bright at center, dim at edges)
    const height = 1.0 - position;
    
    // Noise parameters
    const xScale = 0.12;
    const timeScale = 0.15;
    
    // Add time offset based on position to create wave-like movement
    const timeOffset = i * 0.05;
    
    // Get noise value for this LED at this time
    // Use both LED position and time for more complex patterns
    let noiseValue = perlin.noise(i * xScale, (time + timeOffset) * timeScale) * 0.6 + 0.4;
    
    // Add secondary noise layer for more complexity
    noiseValue += perlin.noise(i * xScale * 2.5, (time * 1.5 + timeOffset) * timeScale) * 0.3;
    noiseValue = Math.min(1.0, noiseValue); // Cap at 1.0
    
    // Apply cooling factor more evenly
    const cooling = Math.pow(position, 1.5) * coolingFactor;
    noiseValue = noiseValue * (1.0 - cooling);
    
    // Skip LEDs that are totally dark (edges of the strip)
    if (height < 0.1 && noiseValue < 0.05) continue;
    
    // Get color based on height and noise
    const color = getFireColor(height, noiseValue);
    
    // Add LED to frame
    leds.push({
      index: i,
      color: color
    });
  }
  
  return leds;
}

// Generate the complete animation JSON
function generateFireAnimation(numFrames = 30, numLeds = 60) {
  const perlin = new PerlinNoise();
  const frames = [];
  
  // Generate each frame
  for (let frame = 0; frame < numFrames; frame++) {
    const time = frame * 0.2; // Time step for animation
    const leds = generateFireFrame(perlin, time, numLeds);
    frames.push({ leds });
  }
  
  // Create the complete animation object
  const fireAnimation = {
    name: "Full Strip Fire Effect",
    type: "custom",
    speed: 60, // 60ms per frame for slightly faster animation
    direction: "forward",
    frames: frames
  };
  
  return fireAnimation;
}

// Generate animation and output JSON
const fireAnimation = generateFireAnimation(40, 60);
writeFileSync('./animation.json', JSON.stringify(fireAnimation, null, 2));