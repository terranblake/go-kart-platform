#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>  // For uint8_t type
#include <FastLED.h>  // For CRGB type
#include "common.pb.h"
#include "lights.pb.h"

// Pin definitions
#define DATA_PIN 13 // LED data pin for lights

// Light configuration
#define NUM_LEDS 300
#define DEFAULT_BRIGHTNESS 100
#define MIN_BRIGHTNESS 30
#define MAX_BRIGHTNESS 200

// Timing configuration
#define TURN_SIGNAL_INTERVAL 500 
#define BRAKE_PULSE_INTERVAL 120
#define SWEEP_INTERVAL 50
#define ANIMATION_STEP_DELAY 15  // Delay for startup/shutdown animation (ms)

// Animation configuration
#define MAX_ANIMATION_BUFFER_SIZE 2048
#define DEFAULT_ANIMATION_FPS 30
#define MAX_ANIMATION_FRAMES 120

// Node ID
#define NODE_ID 0x01

// Debug mode
#define DEBUG_MODE 1

// Test sequence
#define RUN_TEST_SEQUENCE 0

// Section definitions
#define FRONT_START_LED 0
#define FRONT_END_LED 149
#define REAR_START_LED 150
#define REAR_END_LED 299
#define FRONT_CENTER_LED 75
#define REAR_CENTER_LED 225

// Turn signal configuration
#define TURN_SIGNAL_WIDTH 25    // Width of turn signal in LEDs
#define TURN_SIGNAL_STEPS 10    // Steps to complete turn signal animation

// Animation structures
struct AnimationConfig {
    uint8_t fps;               // Frames per second
    uint16_t frameDuration;    // Frame duration in milliseconds
    bool loopAnimation;        // Whether to loop the animation
    uint8_t brightness;        // Animation brightness
};

struct AnimationState {
    bool active;               // Whether animation is currently active
    uint32_t frameCount;       // Number of frames in the animation
    uint32_t currentFrame;     // Current frame index
    uint32_t lastFrameTime;    // Last time a frame was displayed
    uint8_t animationData[MAX_ANIMATION_BUFFER_SIZE]; // Animation data buffer
    uint32_t dataSize;         // Size of animation data
    uint16_t frameSize;        // Size of each frame in bytes
};

// Light state struct
struct LightState {
    kart_lights_LightModeValue mode;    // Current light mode
    uint8_t brightness;                  // Current brightness
    uint8_t turnLeft;                    // Left turn signal active
    uint8_t turnRight;                   // Right turn signal active
    uint8_t hazard;                      // Hazard lights active
    uint8_t braking;                     // Brake lights active
    uint8_t sweepPosition;               // Position for sweep animation
    uint8_t animation;                   // Animation flags
};

// Function prototypes
void handleLightMode(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void handleLightSignal(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void handleLightBrake(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void handleLightTest(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void handleLightLocation(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void handleAnimationControl(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void handleAnimationConfig(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void processAnimationMessage(uint8_t component_id, uint8_t command_id, const uint8_t* data, size_t length, bool isLastChunk);
void resetAnimationState();
void displayAnimationFrame(uint32_t frameIndex);
void updateAnimation();
void updateLights(CRGB* leds, int numLeds, LightState& lightState);
void updateTurnSignals(CRGB* leds, int numLeds, LightState& lightState);
void updateBrake(CRGB* leds, int numLeds, LightState& lightState);
void setSolidColor(CRGB* leds, int numLeds, CRGB color);
void runTestSequence();
void setupLightsForTesting();

#endif