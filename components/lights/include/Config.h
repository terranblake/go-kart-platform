#include <stdint.h>  // For uint8_t type

// Pin definitions
#define DATA_PIN 6 // LED data pin for lights

// Light configuration
#define NUM_LEDS 60
#define DEFAULT_BRIGHTNESS 50
#define BRIGHT_BRIGHTNESS 127
#define BRAKE_BRIGHTNESS 255 // Much brighter for brake lights

// Turn signal configuration
#define TURN_SIGNAL_BLINK_RATE 500 // Blink rate in milliseconds
#define TURN_SIGNAL_COUNT 10       // Number of LEDs to use for turn signals on each side
#define TURN_SIGNAL_SWEEP_STEP 50  // Time between each LED in the sweep (milliseconds)

// Startup/Shutdown configuration
#define ANIMATION_STEP_DELAY 5 // Duration between the next step of the start/stop animation

// Animation configuration
#define MAX_ANIMATION_BUFFER_SIZE 180 // 60 LEDs * 3 bytes (RGB) = 180 bytes
#define DEFAULT_ANIMATION_FPS 30      // Default frames per second
#define MAX_ANIMATION_FRAMES 255      // Maximum number of frames in an animation

// Serial command configuration
#define SERIAL_COMMAND_BUFFER_SIZE 32

// Structure for animation configuration
typedef struct
{
    uint8_t fps = DEFAULT_ANIMATION_FPS;  // Frames per second
    uint8_t loopAnimation = 1;           // Whether to loop the animation (0=no, 1=yes)
    uint8_t brightness = DEFAULT_BRIGHTNESS; // Animation brightness
    uint8_t ledCount = NUM_LEDS;         // Number of LEDs to update
} AnimationConfig;

// Structure for animation state
typedef struct
{
    uint8_t isPlaying = 0;              // Whether the animation is currently playing
    uint8_t currentFrame = 0;           // Current frame being displayed
    uint8_t totalFrames = 0;            // Total number of frames in the animation
    unsigned long lastFrameTime = 0;    // Last time a frame was displayed
    uint8_t frameData[MAX_ANIMATION_BUFFER_SIZE]; // Buffer for current frame data
    uint16_t dataSize = 0;              // Size of valid data in the buffer
    uint8_t receivingFrame = 0;         // Whether we're currently receiving a frame
} AnimationState;

// Structure for light state
typedef struct
{
    uint8_t mode = 0;          // Light mode: 0=Off, 1=Low, 2=High, 3=Hazard, 9=Animation
    uint8_t turnLeft = 0;      // 0=Off, 1=On
    uint8_t turnRight = 0;     // 0=Off, 1=On
    uint8_t braking = 0;       // 0=Off, 1=On
    uint8_t animation = 0;     // 0=Off, 1=On (startup), 2=On->Off (shutdown)
    uint8_t sweepPosition = 0; // For animations
} LightState;

// Function prototypes for command handlers
void handleLightMode(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);
void handleLightSignal(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);
void handleLightBrake(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);
void handleLightTest(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);
void handleLightLocation(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);
void handleAnimationControl(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);
void handleAnimationConfig(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);
void processAnimationMessage(kart_common_MessageType message_type, 
                         kart_common_ComponentType component_type,
                         kart_common_AnimationFlag animation_flag,
                         uint8_t component_id, uint8_t command_id,
                         uint32_t value);

// Forward declarations for functions
void clearLights(CRGB *leds, int numLeds);
void updateTurnSignals(CRGB *leds, int numLeds, LightState &state);
void updateLights(CRGB *leds, int numLeds, LightState &state);
void updateStartupShutdownAnimation(CRGB *leds, int numLeds, LightState &state);
void updateAnimation(CRGB *leds, int numLeds, AnimationState &animState, AnimationConfig &config);
void displayAnimationFrame(CRGB *leds, const uint8_t *frameData, uint16_t dataSize);
void resetAnimationState(AnimationState &animState);

#if DEBUG_MODE
void runTestSequence();
void processSerialCommands();
void executeSerialCommand(const char *command);
#endif