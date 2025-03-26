#include <Arduino.h>
#include <FastLED.h>

#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "common.pb.h"
#include "lights.pb.h"
#include "controls.pb.h"

// Check if we should use memory optimizations (set in platformio.ini)
#ifdef OPTIMIZE_MEMORY
// Use build flags from platformio.ini
#ifndef MAX_ANIMATION_FRAMES
#define MAX_ANIMATION_FRAMES 3  // Memory-optimized default
#endif
#ifndef MAX_LEDS_PER_FRAME
#define MAX_LEDS_PER_FRAME 5    // Memory-optimized default
#endif

// We must define SERIAL_COMMAND_BUFFER_SIZE to something, even if small
#ifndef SERIAL_COMMAND_BUFFER_SIZE
#define SERIAL_COMMAND_BUFFER_SIZE 16
#endif

// Define the buffer if it's needed
char serialCommandBuffer[SERIAL_COMMAND_BUFFER_SIZE];

// Optimized LED data structure - omits y coordinate to save RAM
struct LedData {
  uint8_t x;
  CRGB color;
};

#else
// Original unoptimized values
#ifndef MAX_ANIMATION_FRAMES
#define MAX_ANIMATION_FRAMES 30
#endif
#ifndef MAX_LEDS_PER_FRAME
#define MAX_LEDS_PER_FRAME 60
#endif

#ifndef SERIAL_COMMAND_BUFFER_SIZE
#define SERIAL_COMMAND_BUFFER_SIZE 64
#endif

// Define the buffer with original size
char serialCommandBuffer[SERIAL_COMMAND_BUFFER_SIZE];

// Original LED data structure with x,y coordinates
struct LedData {
  uint8_t x;
  uint8_t y;
  CRGB color;
};
#endif

// Animation frame structure
struct AnimationFrame {
  uint8_t numLeds;
  LedData leds[MAX_LEDS_PER_FRAME];
};

// Animation structure
struct Animation {
  bool active;
  uint8_t numFrames;
  uint8_t currentFrame;
  uint16_t frameDelay;
  unsigned long lastFrameTime;
  AnimationFrame frames[MAX_ANIMATION_FRAMES];
};

// Global state variables
LightState lightState{};
bool testModeActive = false;
CRGB leds[NUM_LEDS];
bool updateFrontLights = true; // Default to updating front lights
bool locationSelected = false; // whether the primary controller has told this secondary which lights it controls

// Animation state
Animation currentAnimation;
bool animationMode = false;

// Animation and turn signal timing variables
unsigned long lastTurnUpdate = 0;
unsigned long lastAnimationUpdate = 0;
bool turnSignalState = true;
unsigned long lastTurnToggle;
unsigned long lastSweepUpdate;

void setupLightsForTesting();
void updateAnimation();
void clearAnimation();
void handleAnimationStart(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value);
void handleAnimationFrame(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, const void* data, size_t data_size);
void handleAnimationEnd(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value);
void handleAnimationStop(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value);

// todo: this should be pulled from EEPROM as a unique id
ProtobufCANInterface canInterface(0x01);

// Optimized print macros to disable serial prints when not needed
#ifdef DISABLE_SERIAL_PRINTS
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT_VAL(x, y)
#else
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINT_VAL(x, y) do { Serial.print(x); Serial.println(y); } while(0)
#endif

void setup()
{
#if DEBUG_MODE
  Serial.begin(115200);
#endif

  if (!canInterface.begin(500E3))
  {
    while (1)
      ;
  }

  // Initialize LED strips
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

  // Initialize all light states to default
  lightState.mode = 0; // Lights disabled by default
  clearLights(leds, NUM_LEDS);
  
  // Initialize animation state
  clearAnimation();

  // Register standard light handlers
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_DIAGNOSTIC, kart_controls_ControlCommandId_MODE, handleLightTest);

  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_MODE, handleLightMode);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_SIGNAL, handleLightSignal);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_BRAKE, handleLightBrake);

  // Register animation handlers
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_START, handleAnimationStart);
  canInterface.registerBinaryHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_FRAME, handleAnimationFrame);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_END, handleAnimationEnd);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_STOP, handleAnimationStop);

  // todo: remove these in favor of EEPROM-supported node identification
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_LOCATION, handleLightLocation);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_LOCATION, handleLightLocation);

  Serial.println(F("Go-Kart Lights"));

#if DEBUG_MODE
  setupLightsForTesting();
#endif
}

void loop()
{
  if (!testModeActive)
  {
    if (animationMode && currentAnimation.active) {
      // Update animation if active
      updateAnimation();
    } else {
      // Update light states based on current settings
      updateLights(leds, NUM_LEDS, lightState);
    }

    // Show the updated lights
    FastLED.show();
  }

  // Small delay to prevent flickering
  delay(10);
  canInterface.process();

#if DEBUG_MODE
  if (testModeActive)
  {
    // Run the test sequence if in test mode
    runTestSequence();
  }
#endif
}

void updateLights(CRGB *leds, int numLeds, LightState &lightState)
{
  // Store previous mode to detect transitions
  // todo: topo tired to check this but looks bad that we're initing previous state to off every time
  static uint8_t previousStateMode = kart_lights_LightModeValue_OFF;

  // Check for state transitions that should trigger animations
  if (previousStateMode == kart_lights_LightModeValue_OFF && lightState.mode != kart_lights_LightModeValue_OFF)
  {
    // Transition from OFF to any ON mode - start forward animation
    lightState.animation = 1;
    lightState.sweepPosition = 0;
  }
  else if (previousStateMode != kart_lights_LightModeValue_OFF && lightState.mode == kart_lights_LightModeValue_OFF)
  {
    // Transition from any ON mode to OFF - start reverse animation
    lightState.animation = 2;               // 2 indicates reverse animation
    lightState.sweepPosition = numLeds / 2; // Start from fully extended
  }

  // Update the previous mode for next time
  previousStateMode = lightState.mode;

  // Handle animation if it's active (takes precedence over normal display)
  if (lightState.animation > 0)
  {
    // Forward or reverse startup animation
    updateStartupShutdownAnimation(leds, numLeds, lightState);
    return;
  }

  // Only proceed with normal lighting if mode is not OFF
  if (lightState.mode == kart_lights_LightModeValue_OFF)
  {
    clearLights(leds, numLeds);
    return;
  }

  // Clear LEDs first to ensure clean state
  clearLights(leds, numLeds);

  // Calculate brightness based on mode and braking
  uint8_t brightness = DEFAULT_BRIGHTNESS;
  if (lightState.mode == kart_lights_LightModeValue_BRIGHT)
  {
    brightness = BRIGHT_BRIGHTNESS;
  }
  if (!updateFrontLights && lightState.braking)
  {
    brightness = BRAKE_BRIGHTNESS;
  }
  FastLED.setBrightness(brightness);

  // Standard lighting (for non-hazard modes) - main lights
  if (lightState.mode != kart_lights_LightModeValue_HAZARD)
  {
    CRGB baseColor = updateFrontLights ? CRGB::White : CRGB::Red;
    int mainStart = TURN_SIGNAL_COUNT;
    int mainEnd = numLeds - TURN_SIGNAL_COUNT;

    for (int i = mainStart; i < mainEnd; i++)
    {
      leds[i] = baseColor;
    }
  }

  // Handle turn signals
  updateTurnSignals(leds, numLeds, lightState);
}

void updateTurnSignals(CRGB *leds, int numLeds, LightState &lightState)
{
  unsigned long currentMillis = millis();

  // Handle hazard mode by setting both turn signals
  if (lightState.mode == kart_lights_LightModeValue_HAZARD)
  {
    // First set the middle section lights (same as regular mode)
    CRGB baseColor = updateFrontLights ? CRGB::White : CRGB::Red;
    int mainStart = TURN_SIGNAL_COUNT;
    int mainEnd = numLeds - TURN_SIGNAL_COUNT;

    for (int i = mainStart; i < mainEnd; i++)
    {
      leds[i] = baseColor;
    }

    // In hazard mode, both turn signals blink together
    if (currentMillis - lastTurnToggle > TURN_SIGNAL_BLINK_RATE)
    {
      turnSignalState = !turnSignalState;
      lastTurnToggle = currentMillis;
    }

    if (turnSignalState)
    {
      // Left side (first TURN_SIGNAL_COUNT LEDs)
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++)
      {
        leds[i] = CRGB::Orange;
      }

      // Right side (last TURN_SIGNAL_COUNT LEDs)
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++)
      {
        leds[numLeds - 1 - i] = CRGB::Orange;
      }
    }
    return;
  }

  // Only process regular turn signals if there's a signal active
  if (!lightState.turnLeft && !lightState.turnRight)
    return;

  // Update blink state based on timing
  if (currentMillis - lastTurnToggle > TURN_SIGNAL_BLINK_RATE)
  {
    turnSignalState = !turnSignalState;
    lastTurnToggle = currentMillis;

    // Reset sweep position when starting a new blink cycle
    if (turnSignalState)
    {
      lightState.sweepPosition = 0;
      lastSweepUpdate = currentMillis;
    }
  }

  // Process turn signals only if they're in the ON phase of blinking
  if (turnSignalState)
  {
    // Update sweep position based on timing
    if (currentMillis - lastSweepUpdate > TURN_SIGNAL_SWEEP_STEP && lightState.sweepPosition < TURN_SIGNAL_COUNT)
    {
      lightState.sweepPosition++;
      lastSweepUpdate = currentMillis;
    }

    // Left turn signal - sweeping right to left
    if (lightState.turnLeft)
    {
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++)
      {
        if (i <= lightState.sweepPosition)
        {
          leds[TURN_SIGNAL_COUNT - 1 - i] = CRGB::Orange;
        }
      }
    }

    // Right turn signal - sweeping left to right
    if (lightState.turnRight)
    {
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++)
      {
        if (i <= lightState.sweepPosition)
        {
          leds[numLeds - TURN_SIGNAL_COUNT + i] = CRGB::Orange;
        }
      }
    }
  }
}

void updateStartupShutdownAnimation(CRGB *leds, int numLeds, LightState &lightState)
{
  // Determine the color based on front/rear
  CRGB color = updateFrontLights ? CRGB::White : CRGB::Red;

  // Get middle position for animation
  int middle = numLeds / 2;

  // Forward animation speed control
  unsigned long currentMillis = millis();
  static unsigned long lastStepTime = 0;

  if (currentMillis - lastStepTime < ANIMATION_STEP_DELAY)
  {
    return;
  }
  lastStepTime = currentMillis;

  // Clear all LEDs first to ensure clean animation
  clearLights(leds, numLeds);

  if (lightState.animation == 1)
  {
    // STARTUP ANIMATION
    // Fill from middle to current position in both directions
    for (int i = middle - lightState.sweepPosition; i <= middle + lightState.sweepPosition; i++)
    {
      if (i >= 0 && i < numLeds)
      {
        leds[i] = color;
      }
    }

    // Increment position with accelerating speed
    int speedMultiplier = 1 + (lightState.sweepPosition / 10);
    lightState.sweepPosition += speedMultiplier;

    // End animation when it reaches edges
    if (lightState.sweepPosition >= middle)
    {
      lightState.animation = 0;
      lightState.sweepPosition = 0;

      // Fill in the regular lighting pattern
      if (lightState.mode != kart_lights_LightModeValue_OFF)
      {
        if (updateFrontLights)
        {
          for (int i = TURN_SIGNAL_COUNT; i < numLeds - TURN_SIGNAL_COUNT; i++)
          {
            leds[i] = CRGB::White;
          }
        }
        else
        {
          for (int i = TURN_SIGNAL_COUNT; i < numLeds - TURN_SIGNAL_COUNT; i++)
          {
            leds[i] = CRGB::Red;
          }
        }
      }
    }
  }
  else if (lightState.animation == 2)
  {
    // SHUTDOWN ANIMATION
    // Ensure lightState.sweepPosition stays within valid range (signed int)
    if (lightState.sweepPosition > 200)
    { // This catches overflow to large values
      lightState.sweepPosition = middle;
    }

    // Animation starts with full strip lit, then contracts to middle
    int currentWidth = lightState.sweepPosition;

    // Fill all LEDs from middle-width to middle+width
    for (int i = middle - currentWidth; i <= middle + currentWidth; i++)
    {
      if (i >= 0 && i < numLeds)
      {
        leds[i] = color;
      }
    }

    // Decrement position with accelerating speed
    int speedMultiplier = 1 + ((middle - lightState.sweepPosition) / 10);
    if (speedMultiplier <= 0)
      speedMultiplier = 1; // Ensure positive multiplier

    // Fix for overflow - check bounds before subtraction
    if (lightState.sweepPosition <= speedMultiplier)
    {
      lightState.sweepPosition = 0; // About to reach 0
    }
    else
    {
      lightState.sweepPosition -= speedMultiplier;
    }

    // End animation when it reaches the middle
    if (lightState.sweepPosition <= 0)
    {
      lightState.animation = 0;
      lightState.sweepPosition = 0;
      clearLights(leds, numLeds);
    }
  }

  // Force update the display
  FastLED.show();
}

void clearLights(CRGB *leds, int numLeds)
{
  // Turn off all LEDs
  fill_solid(leds, numLeds, CRGB::Black);
}

void handleLightMode(kart_common_MessageType msg_type,
                     kart_common_ComponentType comp_type,
                     uint8_t component_id,
                     uint8_t command_id,
                     kart_common_ValueType value_type,
                     int32_t value)
{
  lightState.mode = value;
  DEBUG_PRINT(F("Light mode: "));
  DEBUG_PRINTLN(lightState.mode);
}

void handleLightSignal(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value)
{
  switch (value)
  {
  case kart_lights_LightSignalValue_NONE:
    lightState.turnLeft = 0;
    lightState.turnRight = 0;
    break;
  case kart_lights_LightSignalValue_LEFT:
    lightState.turnLeft = 1;
    lightState.turnRight = 0;
    break;
  case kart_lights_LightSignalValue_RIGHT:
    lightState.turnLeft = 0;
    lightState.turnRight = 1;
    break;
  }
  DEBUG_PRINT(F("Turn signals: L="));
  DEBUG_PRINT(lightState.turnLeft);
  DEBUG_PRINT(F(" R="));
  DEBUG_PRINTLN(lightState.turnRight);
}

void handleLightBrake(kart_common_MessageType msg_type,
                      kart_common_ComponentType comp_type,
                      uint8_t component_id,
                      uint8_t command_id,
                      kart_common_ValueType value_type,
                      int32_t value)
{
  lightState.braking = (value == 1) ? 1 : 0;
  DEBUG_PRINT(F("Brake: "));
  DEBUG_PRINTLN(lightState.braking);
}

void handleLightTest(kart_common_MessageType msg_type,
                     kart_common_ComponentType comp_type,
                     uint8_t component_id,
                     uint8_t command_id,
                     kart_common_ValueType value_type,
                     int32_t value)
{
  testModeActive = (value == kart_controls_ControlModeValue_TEST);
  DEBUG_PRINT(F("Test mode: "));
  DEBUG_PRINT(value);
  DEBUG_PRINT(" ");
  DEBUG_PRINT(kart_controls_ControlModeValue_TEST);
  DEBUG_PRINT(" ");
  DEBUG_PRINTLN(testModeActive);
}

void handleLightLocation(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value)
{
  updateFrontLights = (value == 1);
  locationSelected = true;
  DEBUG_PRINT(F("Light location: "));
  DEBUG_PRINTLN(updateFrontLights ? F("Front") : F("Rear"));
}

#if DEBUG_MODE

void setupLightsForTesting()
{
  Serial.println("Sending message to set location to front");
  canInterface.sendMessage(
      kart_common_MessageType_COMMAND,
      kart_common_ComponentType_LIGHTS,
      kart_lights_LightComponentId_ALL,
      kart_lights_LightCommandId_LOCATION,
      kart_common_ValueType_INT8,
      0);

  Serial.println("Sending message to set testing to true");
  canInterface.sendMessage(
      kart_common_MessageType_COMMAND,
      kart_common_ComponentType_CONTROLS,
      kart_controls_ControlComponentId_DIAGNOSTIC,
      kart_controls_ControlCommandId_MODE,
      kart_common_ValueType_INT8,
      kart_controls_ControlModeValue_TEST);

  Serial.println("Lights set up for testing");
}

void runTestSequence()
{
  static unsigned long lastStateChange = 0;
  static uint8_t testState = 0;
  const uint8_t numTestStates = 12;
  const unsigned long testInterval = 3000; // 3 seconds per state

  unsigned long currentMillis = millis();

  // Check if it's time to change state
  if (currentMillis - lastStateChange > testInterval)
  {
    lastStateChange = currentMillis;
    testState = (testState + 1) % numTestStates;

    // Reset all states before each test
    updateFrontLights = true;
    lightState.turnLeft = 0;
    lightState.turnRight = 0;
    lightState.braking = 0;
    lightState.sweepPosition = 0;
    lightState.animation = 0;

    switch (testState)
    {
    case 0:
      // Startup front - lights on
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_ON);
      break;

    case 1:
      // Shutdown front
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_OFF);
      break;

    case 2:
      // Low mode with left turn signal
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_ON);
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_ALL,
          kart_lights_LightCommandId_SIGNAL,
          kart_common_ValueType_UINT8,
          kart_lights_LightSignalValue_LEFT); // Left signal value
      break;

    case 3:
      // Low mode with right turn signal
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_ON);
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_ALL,
          kart_lights_LightCommandId_SIGNAL,
          kart_common_ValueType_UINT8,
          kart_lights_LightSignalValue_RIGHT); // Right signal value
      break;

    case 4:
      // High mode with left turn signal
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_BRIGHT);
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_ALL,
          kart_lights_LightCommandId_SIGNAL,
          kart_common_ValueType_UINT8,
          kart_lights_LightSignalValue_LEFT); // Left signal value
      break;

    case 5:
      // High mode with right turn signal
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_BRIGHT);
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_ALL,
          kart_lights_LightCommandId_SIGNAL,
          kart_common_ValueType_UINT8,
          kart_lights_LightSignalValue_RIGHT); // Right signal value
      break;

    case 6:
      // Hazard lights (both turn signals)
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_ALL,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_HAZARD);
      break;

    case 7:
      // Set to rear lights and hazard
      updateFrontLights = false;
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_HAZARD); // Using FLASH as equivalent to HAZARD
      break;

    case 8:
      // Brake lights test
      updateFrontLights = false;
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_ON);
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_BRAKE,
          kart_common_ValueType_BOOLEAN,
          true); // Brake ON
      break;

    case 9:
      // Brake lights with left turn signals
      updateFrontLights = false;
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_ON);
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_BRAKE,
          kart_common_ValueType_BOOLEAN,
          1); // Brake ON
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_ALL,
          kart_lights_LightCommandId_SIGNAL,
          kart_common_ValueType_UINT8,
          kart_lights_LightSignalValue_LEFT); // Left signal
      break;

    case 10:
      // Brake lights with right turn signals
      updateFrontLights = false;
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_ON);
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_BRAKE,
          kart_common_ValueType_BOOLEAN,
          1); // Brake ON
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_ALL,
          kart_lights_LightCommandId_SIGNAL,
          kart_common_ValueType_UINT8,
          kart_lights_LightSignalValue_RIGHT); // Right signal
      break;

    case 11:
      // Shutdown rear animation
      updateFrontLights = false;
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_REAR,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_OFF);
      break;
    }

    Serial.print(F("TS:"));
    Serial.println(testState);
  }

  updateLights(leds, NUM_LEDS, lightState);
  FastLED.show();
}

// Process commands received over Serial
void processSerialCommands()
{
  static int bufferIndex = 0;

  // Check if data is available
  while (Serial.available() > 0)
  {
    char c = Serial.read();

    // Handle newline as command terminator
    if (c == '\n' || c == '\r')
    {
      if (bufferIndex > 0)
      {
        // Null-terminate the buffer
        serialCommandBuffer[bufferIndex] = '\0';

        // Process the command
        executeSerialCommand(serialCommandBuffer);

        // Reset buffer index
        bufferIndex = 0;
      }
    }
    // Add character to buffer if there's room
    else if (bufferIndex < SERIAL_COMMAND_BUFFER_SIZE - 1)
    {
      serialCommandBuffer[bufferIndex++] = c;
    }
  }
}

void executeSerialCommand(const char *command)
{
  // Process CAN message simulation with Protocol Buffer format
  if (strncmp(command, "CAN:", 4) == 0)
  {
    char *ptr = (char *)command + 4;

    // Parse component type
    uint8_t componentType = strtoul(ptr, &ptr, 10);
    if (*ptr == ':')
    {
      ptr++;

      // Parse component ID
      uint8_t componentId = strtoul(ptr, &ptr, 10);
      if (*ptr == ':')
      {
        ptr++;

        // Parse command ID
        uint8_t commandId = strtoul(ptr, &ptr, 10);

        // Parse optional value (default to 0)
        int16_t value = 0;
        if (*ptr == ':')
        {
          ptr++;
          value = strtol(ptr, NULL, 10);
        }

        // Send the message using our new overload
        bool success = canInterface.sendMessage(
            kart_common_MessageType_COMMAND,
            (kart_common_ComponentType)componentType,
            componentId,
            commandId,
            kart_common_ValueType_INT8,
            value);

        if (success)
        {
          Serial.print(F("Serial->CAN: "));
          Serial.print(componentType);
          Serial.print(F(":"));
          Serial.print(componentId);
          Serial.print(F(":"));
          Serial.print(commandId);
          Serial.print(F("="));
          Serial.println(value);
        }
        else
        {
          Serial.println(F("Failed to send message"));
        }
      }
    }
  }
  // Toggle test sequence
  else if (strcmp(command, "TEST:START") == 0)
  {
    testModeActive = true;
    Serial.println(F("Test sequence started"));
  }
  else if (strcmp(command, "TEST:STOP") == 0)
  {
    testModeActive = false;
    Serial.println(F("Test sequence stopped"));
  }
  // Simple ping command to test serial connectivity
  else if (strcmp(command, "PING") == 0)
  {
    Serial.println(F("PONG"));
  }
  else
  {
    Serial.println(F("Unsupported command"));
  }
}

// Function to clear animation data
void clearAnimation() {
  currentAnimation.active = false;
  currentAnimation.numFrames = 0;
  currentAnimation.currentFrame = 0;
  currentAnimation.frameDelay = 100;  // Default to 100ms
  currentAnimation.lastFrameTime = 0;
  animationMode = false;
  
  // Clear all frames
  for (int i = 0; i < MAX_ANIMATION_FRAMES; i++) {
    currentAnimation.frames[i].numLeds = 0;
  }
}

// Function to update animation with memory optimizations
void updateAnimation() {
  unsigned long currentTime = millis();
  
  if (currentTime - currentAnimation.lastFrameTime >= currentAnimation.frameDelay) {
    currentAnimation.currentFrame = (currentAnimation.currentFrame + 1) % currentAnimation.numFrames;
    currentAnimation.lastFrameTime = currentTime;
    
    clearLights(leds, NUM_LEDS);
    
    AnimationFrame& frame = currentAnimation.frames[currentAnimation.currentFrame];
    for (int i = 0; i < frame.numLeds; i++) {
      LedData& led = frame.leds[i];
      
#ifdef OPTIMIZE_MEMORY
      // Optimized: Use x directly as index
      int index = led.x;
#else
      // Original: Calculate index from x, y (specific to application)
      int index = led.x;
#endif
      
      if (index >= 0 && index < NUM_LEDS) {
        leds[index] = led.color;
      }
    }
  }
}

// Handler for ANIMATION_START command
void handleAnimationStart(kart_common_MessageType msg_type,
                        kart_common_ComponentType comp_type,
                        uint8_t component_id,
                        uint8_t command_id,
                        kart_common_ValueType value_type,
                        int32_t value) {
  // Initialize a new animation
  clearAnimation();
  
  // Set animation mode to true (will override normal lighting)
  animationMode = true;
  
  DEBUG_PRINTLN(F("Animation Start"));
}

// Handler for ANIMATION_FRAME command (binary data)
void handleAnimationFrame(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       const void* data,
                       size_t data_size) {
  // Ensure we don't exceed maximum frames
  if (currentAnimation.numFrames >= MAX_ANIMATION_FRAMES) {
    DEBUG_PRINTLN(F("Too many animation frames, ignoring"));
    return;
  }
  
  const uint8_t* bytes = (const uint8_t*)data;
  
  if (data_size < 2) {
    DEBUG_PRINTLN(F("Invalid binary data size"));
    return;
  }
  
  uint8_t frameIndex = bytes[0];
  uint8_t numLeds = bytes[1];
  
#ifdef OPTIMIZE_MEMORY
  // Optimized: Each LED takes 4 bytes (x, r, g, b)
  size_t requiredSize = 2 + (numLeds * 4);
#else
  // Original: Each LED takes 5 bytes (x, y, r, g, b)
  size_t requiredSize = 2 + (numLeds * 5);
#endif

  if (data_size < requiredSize) {
    DEBUG_PRINT(F("Invalid LED data size: "));
    DEBUG_PRINT(data_size);
    DEBUG_PRINT(F(" < "));
    DEBUG_PRINTLN(requiredSize);
    return;
  }
  
  AnimationFrame& frame = currentAnimation.frames[frameIndex];
  frame.numLeds = numLeds;
  
  // Ensure we don't exceed maximum LEDs per frame
  if (numLeds > MAX_LEDS_PER_FRAME) {
    numLeds = MAX_LEDS_PER_FRAME;
    frame.numLeds = numLeds;
  }
  
  // Parse LED data with memory optimizations if enabled
  for (int i = 0; i < numLeds; i++) {
#ifdef OPTIMIZE_MEMORY
    // Optimized: Each LED takes 4 bytes (x, r, g, b)
    const uint8_t* ledData = bytes + 2 + (i * 4);
    frame.leds[i].x = ledData[0];
    frame.leds[i].color = CRGB(ledData[1], ledData[2], ledData[3]);
#else
    // Original: Each LED takes 5 bytes (x, y, r, g, b)
    const uint8_t* ledData = bytes + 2 + (i * 5);
    frame.leds[i].x = ledData[0];
    frame.leds[i].y = ledData[1];
    frame.leds[i].color = CRGB(ledData[2], ledData[3], ledData[4]);
#endif
  }
  
  // Update number of frames if needed
  if (frameIndex >= currentAnimation.numFrames) {
    currentAnimation.numFrames = frameIndex + 1;
  }
  
  DEBUG_PRINT(F("Received frame "));
  DEBUG_PRINT(frameIndex);
  DEBUG_PRINT(F(" with "));
  DEBUG_PRINT(numLeds);
  DEBUG_PRINTLN(F(" LEDs"));
}

// Handler for ANIMATION_END command
void handleAnimationEnd(kart_common_MessageType msg_type,
                      kart_common_ComponentType comp_type,
                      uint8_t component_id,
                      uint8_t command_id,
                      kart_common_ValueType value_type,
                      int32_t value) {
  // value contains the number of frames
  if (value > 0 && value <= MAX_ANIMATION_FRAMES) {
    currentAnimation.numFrames = value;
  }
  
  // Set animation as active
  currentAnimation.active = true;
  currentAnimation.currentFrame = 0;
  currentAnimation.lastFrameTime = millis();
  
  DEBUG_PRINT(F("Animation End - Playing with "));
  DEBUG_PRINT(currentAnimation.numFrames);
  DEBUG_PRINTLN(F(" frames"));
}

// Handler for ANIMATION_STOP command
void handleAnimationStop(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value) {
  // Disable animation mode and clear animation data
  clearAnimation();
  
  DEBUG_PRINTLN(F("Animation Stopped"));
}

#endif