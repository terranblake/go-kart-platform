# Animation Pipeline Design Document

<!-- LLM_CONTEXT: animation_pipeline_overview -->
## Overview

The Animation Pipeline enables real-time control of LED strips by:
1. Extending the CAN protocol to support animation data transmission
2. Creating server-side components to manage animations
3. Implementing Arduino-side animation reception and display
4. Building a simple web frontend for animation creation and control

The system is designed to operate within hardware constraints:
- 1 Mbps CAN bus
- 60 RGB LEDs per strip
- 2KB flash memory limit on Arduino
- Target performance of 30 FPS
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: binary_protocol_design -->
## Binary Protocol Design

The binary protocol extends the existing CAN message format while maintaining compatibility:

```
| Byte 0      | Byte 1       | Byte 2       | Byte 3      | Byte 4       | Bytes 5-7      |
|-------------|--------------|--------------|-------------|--------------|----------------|
| Header+Flags| LED Index    | Component ID | Command ID  | Value Type   | RGB Data       |
```

Key design decisions:
1. Repurpose 3 reserved bits in Byte 0 as animation flags
2. Use Byte 1 to track LED indices being updated
3. Pack multiple LEDs per message sequence for efficiency
4. Implement immediate LED updates without buffering on Arduino
5. Use animation flags to signal frame start/continuation/end

Animation flags:
- ANIMATION_START (1): First message in animation frame
- ANIMATION_FRAME (2): Continuation message for a frame
- ANIMATION_END (3): Final message in animation frame
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: server_components -->
## Server-side Components

### Animation Manager

```python
class AnimationManager:
    def __init__(self):
        self.animations = {}  # In-memory animation storage
        self.active_animations = {}  # Track currently playing animations
        self.can_interface = None
        
    def validate_animation(self, animation_data):
        # Validate against schema.json structure
        pass
        
    def load_animation(self, animation_id, animation_data):
        # Store validated animation
        pass
    
    def play_animation(self, animation_id, component_id):
        # Start streaming animation frames to component
        pass
```

### Frame Transmission

```python
def send_animation_frame(self, component_id, frame_index, frame_data, led_count):
    # Pack multiple LEDs per message for efficiency
    # Each standard message can contain RGB data for ~2 LEDs
    for i in range(0, led_count, 2):
        # Set appropriate animation flags (START/FRAME/END)
        # Pack LED data efficiently
        # Send messages with proper headers
```

### REST API

The server exposes REST endpoints for:
- Loading animations: `POST /api/animations`
- Listing animations: `GET /api/animations`
- Playing animations: `POST /api/animations/{id}/play`
- Stopping animations: `POST /api/animations/{id}/stop`
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: arduino_implementation -->
## Arduino Implementation

### Animation Handler Structure

```cpp
// Minimal animation state tracking
struct AnimationState {
    bool isPlaying = false;
    uint8_t currentFrame = 0;
    uint8_t totalFrames = 0;
    uint8_t fps = 30;
    bool loopAnimation = true;
    unsigned long lastFrameTime = 0;
};

// Handler for animation frame data
void handleAnimationFrame(
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id,
    kart_common_ValueType value_type,
    int32_t value) {
    
    // Extract animation flags from header
    // Extract LED index and RGB data
    // Update LEDs immediately without buffering
    // Show frame when END flag is received
}
```

Key design decisions:
1. No buffering - update LEDs immediately as data arrives
2. Minimal state tracking to conserve memory
3. Use FastLED for efficient LED control
4. Request next frame only after current frame is displayed
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: frontend_implementation -->
## Frontend Implementation

The frontend will be based on the existing animator.html tool with these enhancements:

```javascript
// Function to send animation to server
function saveAnimationToServer(animation) {
    fetch('/api/animations', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(animation)
    })
    .then(response => response.json())
    .then(data => {
        // Store animation ID for playback
        animationId = data.id;
    });
}

// Function to play animation on physical LEDs
function playOnHardware(animationId, componentId) {
    fetch(`/api/animations/${animationId}/play?component=${componentId}`, {
        method: 'POST'
    });
}
```

The UI will include:
1. Animation editor based on animator.html
2. Preview panel showing current animation
3. Controls for uploading to the server
4. Buttons to play animations on physical hardware
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: performance_considerations -->
## Performance Considerations

### CAN Bus Utilization

With packing 2 LEDs per message sequence:
- 60 LEDs ÷ 2 LEDs per sequence = 30 message sequences
- 30 sequences × 2 messages × 128 bits = 7,680 bits per frame
- 7,680 bits × 30 FPS = 230,400 bps (23% of 1 Mbps bandwidth)

### Memory Usage

Arduino memory optimization:
- No animation frame buffers (process LEDs immediately)
- Minimal state tracking (< 20 bytes)
- Reuse existing LED array from FastLED

### Timing

- Server sends frames based on target FPS
- Arduino requests next frame after showing current frame
- Adaptive timing based on actual transmission/processing times
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: next_steps -->
## Next Steps

### Implementation Phases:
1. Protocol extensions (common.proto and lights.proto)
2. Arduino animation handler implementation
3. Server-side animation manager
4. Frontend integration

### Testing Priorities:
1. RAM and flash memory usage on Arduino
2. Achievable FPS over CAN with varying LED counts
3. End-to-end animation playback quality
4. Stability under load
<!-- LLM_CONTEXT_END -->

**⚠️ DESIGN REVIEW CHECKPOINT**: This design enables 30 FPS animation streaming to 60 LEDs while respecting hardware constraints. It maintains compatibility with the existing protocol and minimizes memory usage on Arduino.