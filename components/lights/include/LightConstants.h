#ifndef LIGHT_CONSTANTS_H
#define LIGHT_CONSTANTS_H

// Additional constants for light control
#define TURN_SIGNAL_COUNT TURN_SIGNAL_WIDTH
#define BRIGHT_BRIGHTNESS MAX_BRIGHTNESS
#define BRAKE_BRIGHTNESS MAX_BRIGHTNESS
#define TURN_SIGNAL_BLINK_RATE TURN_SIGNAL_INTERVAL
#define TURN_SIGNAL_SWEEP_STEP SWEEP_INTERVAL

// Constants for Serial communication
#define SERIAL_COMMAND_BUFFER_SIZE 64

// Other utility functions
void clearLights(CRGB* leds, int numLeds);
void updateStartupShutdownAnimation(CRGB* leds, int numLeds, LightState& lightState);
void executeSerialCommand();

#endif // LIGHT_CONSTANTS_H 