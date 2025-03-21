#include <stdint.h>  // For uint8_t type
#include <kart_protocol.h>

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

// Serial command configuration
#define SERIAL_COMMAND_BUFFER_SIZE 32

// Structure for light state
typedef struct
{
    uint8_t mode = 0;          // Light mode: 0=Off, 1=Low, 2=High, 3=Hazard
    uint8_t turnLeft = 0;      // 0=Off, 1=On
    uint8_t turnRight = 0;     // 0=Off, 1=On
    uint8_t braking = 0;       // 0=Off, 1=On
    uint8_t animation = 0;     // 0=Off, 1=On (startup), 2=On->Off (shutdown)
    uint8_t sweepPosition = 0; // For animations
} LightState;

// Define a command handler function type
typedef void (*CommandHandler)(uint8_t componentId, uint8_t valueId, uint8_t *data);

// Command handler registry
struct CommandRegistry
{
    uint8_t componentType;
    uint8_t commandId;
    CommandHandler handler;
};

// Function prototypes for command handlers
void handleLightMode(uint8_t componentId, uint8_t valueId, uint8_t *data);
void handleLightSignal(uint8_t componentId, uint8_t valueId, uint8_t *data);
void handleLightBrake(uint8_t componentId, uint8_t valueId, uint8_t *data);
void handleLightTest(uint8_t componentId, uint8_t valueId, uint8_t *data);
void handleLightLocation(uint8_t componentId, uint8_t valueId, uint8_t *data);

// Register command handlers in a lookup table
CommandRegistry commandHandlers[] = {
    {KART_TYPE_LIGHTS, KART_CMD_LIGHT_MODE, handleLightMode},
    {KART_TYPE_LIGHTS, KART_CMD_LIGHT_SIGNAL, handleLightSignal},
    {KART_TYPE_LIGHTS, KART_CMD_LIGHT_BRAKE, handleLightBrake},
    {KART_TYPE_LIGHTS, KART_CMD_LIGHT_TEST, handleLightTest},
    {KART_TYPE_LIGHTS, 0x05, handleLightLocation} // Light location command
};

const int NUM_HANDLERS = sizeof(commandHandlers) / sizeof(CommandRegistry);

// Forward declarations for functions
void clearLights(CRGB *leds, int numLeds);
void updateTurnSignals(CRGB *leds, int numLeds, LightState &state);
void updateLights(CRGB *leds, int numLeds, LightState &state);
void updateStartupShutdownAnimation(CRGB *leds, int numLeds, LightState &state);
void processCAN();
void runTestSequence();
void processSerialCommands();
void executeSerialCommand(const char *command);
void processCANMessage(uint32_t id, uint8_t *data, uint8_t length);