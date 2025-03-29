#ifndef ANIMATION_PROTOCOL_H
#define ANIMATION_PROTOCOL_H

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"

// Animation protocol constants
#define ANIM_BASE_ID      0x700  // Base CAN ID for animation messages
#define ANIM_DATA_ID      0x701  // CAN ID for animation data frames
#define ANIM_CTRL_ID      0x700  // CAN ID for animation control messages

// Control command bytes (first byte in message payload)
#define CMD_STREAM_START  0x01
#define CMD_STREAM_END    0x02
#define CMD_FRAME_START   0x03
#define CMD_FRAME_END     0x04
#define CMD_CONFIG        0x05

// Configuration parameter IDs (second byte in CONFIG messages)
#define CONFIG_FPS        0x01
#define CONFIG_NUM_LEDS   0x02
#define CONFIG_BRIGHTNESS 0x03
#define CONFIG_MODE       0x04

// Mode values
#define MODE_STATIC       0x01
#define MODE_ANIMATION    0x02
#define MODE_OFF          0x00

// Stream state
#define STREAM_STATE_IDLE     0
#define STREAM_STATE_ACTIVE   1
#define STREAM_STATE_FRAME    2

// Memory management
#define MAX_FRAME_SIZE    (NUM_LEDS * 3)  // Maximum bytes in a frame (RGB values)
#define MAX_FRAME_BUFFER  512             // Maximum buffer size (adjust based on available RAM)

class AnimationProtocol {
public:
    AnimationProtocol();
    
    // Initialize with LED array reference
    void begin(CRGB* leds, uint16_t numLeds);
    
    // Process received CAN message
    bool processMessage(uint32_t canId, uint8_t* data, uint8_t length);
    
    // Check if animation is active
    bool isAnimationActive() const;
    
    // Update animation frame if needed (call in main loop)
    bool update();
    
    // Reset animation state
    void reset();
    
    // Get diagnostics
    uint16_t getMemoryUsage() const;
    uint8_t getFPS() const;
    uint16_t getReceivedFrames() const;
    uint16_t getDroppedFrames() const;
    
private:
    // Message handlers
    void handleControlMessage(uint8_t* data, uint8_t length);
    void handleDataMessage(uint8_t* data, uint8_t length);
    void handleConfigMessage(uint8_t* data, uint8_t length);
    
    // Stream handling
    void startStream(uint8_t numFrames, uint8_t fps, uint16_t numLeds);
    void endStream();
    void startFrame(uint8_t frameNum, uint16_t frameSize);
    void endFrame(uint8_t frameNum);
    void processFrameData();
    
    // State variables
    uint8_t streamState;
    uint8_t currentFrame;
    uint8_t numFrames;
    uint8_t fps;
    uint16_t targetNumLeds;
    uint8_t brightness;
    uint8_t mode;
    
    // Frame buffer
    uint8_t frameBuffer[MAX_FRAME_BUFFER];
    uint16_t frameBufferPos;
    uint16_t expectedFrameSize;
    
    // LED reference
    CRGB* leds;
    uint16_t numLeds;
    
    // Timing
    unsigned long lastFrameTime;
    unsigned long frameInterval;
    
    // Statistics
    uint16_t receivedFrames;
    uint16_t droppedFrames;
    uint16_t memoryUsage;
    
    // Flag for frame ready to display
    bool frameReady;
};

#endif // ANIMATION_PROTOCOL_H
