#include <Arduino.h>
#include <FastLED.h>

#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "common.pb.h"
#include "lights.pb.h"
#include "controls.pb.h"

#if DEBUG_MODE
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

// Global variables
CRGB leds[NUM_LEDS];
LightState lightState;
AnimationState animationState;
AnimationConfig animationConfig;
ProtobufCANInterface canInterface(NODE_ID);
bool updateFrontLights = true; // Default to front lights
bool testModeActive = false;

// Animation and turn signal timing variables
unsigned long lastTurnUpdate = 0;
unsigned long lastAnimationUpdate = 0;
bool turnSignalState = true;
unsigned long lastTurnToggle;
unsigned long lastSweepUpdate;

void setupLightsForTesting();

#if DEBUG_MODE

char serialCommandBuffer[SERIAL_COMMAND_BUFFER_SIZE];

#endif

void setup()
{
  // Initialize serial for debugging
  Serial.begin(115200);
  Serial.println(F("Go Kart Lights Controller Starting..."));

  // Initialize FastLED
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(DEFAULT_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  // Initialize CAN interface
  canInterface.begin(500000);

  // Initialize light state
  lightState.mode = kart_lights_LightModeValue_OFF;
  lightState.brightness = DEFAULT_BRIGHTNESS;
  lightState.turnLeft = 0;
  lightState.turnRight = 0;
  lightState.hazard = 0;
  lightState.braking = 0;
  lightState.sweepPosition = 0;
  lightState.animation = 0;

  // Initialize animation config
  animationConfig.fps = DEFAULT_ANIMATION_FPS;
  animationConfig.frameDuration = 1000 / DEFAULT_ANIMATION_FPS;
  animationConfig.loopAnimation = true;
  animationConfig.brightness = DEFAULT_BRIGHTNESS;

  // Reset animation state
  resetAnimationState();

  // Register regular message handlers
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_MODE,
    handleLightMode);

  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_SIGNAL,
    handleLightSignal);

  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_BRAKE,
    handleLightBrake);

  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_CONTROLS,
    kart_controls_ControlComponentId_DIAGNOSTIC,
    kart_controls_ControlCommandId_MODE,
    handleLightTest);

  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_LOCATION,
    handleLightLocation);

  // Register animation handlers
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_ANIMATION_CONTROL,
    handleAnimationControl);

  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_ANIMATION_CONFIG,
    handleAnimationConfig);

  // Register animation data handler
  canInterface.registerAnimationHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_ANIMATION_DATA,
    processAnimationMessage);

  // Set default location (front)
  updateFrontLights = true;

  // Enable test mode if configured
  #if RUN_TEST_SEQUENCE
  testModeActive = true;
  Serial.println(F("Test mode enabled"));
  #endif
}

void loop()
{
  // Process incoming CAN messages
  canInterface.processIncoming();

  // Update the lights based on current state
  if (animationState.active && lightState.mode == kart_lights_LightModeValue_ANIMATION) {
    // When animation mode is active and an animation is loaded, run it
    updateAnimation();
  } else {
    // Otherwise update regular light modes
    updateLights();
  }

  // Run test sequence if test mode is active
  if (testModeActive) {
    runTest();
    testModeActive = false;
  }
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

void handleLightMode(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id) {
  // Only process if this message is for us (based on component ID)
  if (component_id != kart_lights_LightComponentId_ALL && 
      (component_id != kart_lights_LightComponentId_FRONT && updateFrontLights) && 
      (component_id != kart_lights_LightComponentId_REAR && !updateFrontLights)) {
    return;
  }
  
  // Parse light mode value
  uint8_t* raw_data = (uint8_t*)data;
  uint8_t value = raw_data[0];
  
  // Check if the mode is valid
  if (value <= kart_lights_LightModeValue_ANIMATION) {
    // If changing to animation mode, reset animation state
    if (value == kart_lights_LightModeValue_ANIMATION) {
      // Only switch to animation mode if we have animation data
      if (animationState.frameCount > 0) {
        // Reset animation playback
        animationState.active = true;
        animationState.currentFrame = 0;
        animationState.lastFrameTime = millis();
        
        // Set brightness for animation
        FastLED.setBrightness(animationConfig.brightness);
      } else {
        // No animation data, fallback to OFF mode
        value = kart_lights_LightModeValue_OFF;
        Serial.println(F("No animation data available"));
      }
    } else {
      // For other modes, use the standard brightness
      FastLED.setBrightness(lightState.brightness);
    }
    
    // Set the light mode
    lightState.mode = (kart_lights_LightModeValue)value;
    
    Serial.print(F("Light mode: "));
    Serial.println(value);
  }
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
  Serial.print(F("Turn signals: L="));
  Serial.print(lightState.turnLeft);
  Serial.print(F(" R="));
  Serial.println(lightState.turnRight);
}

void handleLightBrake(kart_common_MessageType msg_type,
                      kart_common_ComponentType comp_type,
                      uint8_t component_id,
                      uint8_t command_id,
                      kart_common_ValueType value_type,
                      int32_t value)
{
  lightState.braking = (value == 1) ? 1 : 0;
  Serial.print(F("Brake: "));
  Serial.println(lightState.braking);
}

void handleLightTest(kart_common_MessageType msg_type,
                     kart_common_ComponentType comp_type,
                     uint8_t component_id,
                     uint8_t command_id,
                     kart_common_ValueType value_type,
                     int32_t value)
{
  testModeActive = (value == kart_controls_ControlModeValue_TEST);
  Serial.print(F("Test mode: "));
  Serial.print(value);
  Serial.print(" ");
  Serial.print(kart_controls_ControlModeValue_TEST);
  Serial.print(" ");
  Serial.println(testModeActive);
}

void handleLightLocation(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value)
{
  updateFrontLights = (value == 1);
  Serial.print(F("Light location: "));
  Serial.println(updateFrontLights ? F("Front") : F("Rear"));
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
  const uint8_t numTestStates = 14; // Increased to add animation test states
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
      
    case 12:
      // Front animation mode test
      updateFrontLights = true;
      // First set mode to animation
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_ANIM_MODE);
      
      // Set animation config - 10 FPS, loop enabled
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_ANIMATION_CONFIG,
          kart_common_ValueType_UINT8,
          10); // 10 FPS
      
      // Set total frames to 3
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_ANIMATION,
          kart_common_ValueType_UINT8,
          103); // 100 + totalFrames (3)
      
      // Start animation playback
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_ANIMATION,
          kart_common_ValueType_UINT8,
          kart_lights_AnimationCommandValue_ANIM_PLAY);
          
      // Send test animation frame 1 (all red)
      for (int i = 0; i < 20; i++) {
        uint8_t ledData[] = {255, 0, 0}; // RGB: pure red
        uint32_t value = (ledData[0] << 16) | (ledData[1] << 8) | ledData[2];
        
        // Process as if received from CAN
        processAnimationMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          (i == 0) ? kart_common_AnimationFlag_ANIMATION_START : 
                     (i == 19) ? kart_common_AnimationFlag_ANIMATION_END : 
                                 kart_common_AnimationFlag_ANIMATION_FRAME,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_ANIMATION_DATA,
          value
        );
      }
      break;
      
    case 13:
      // Turn off animation mode
      canInterface.sendMessage(
          kart_common_MessageType_COMMAND,
          kart_common_ComponentType_LIGHTS,
          kart_lights_LightComponentId_FRONT,
          kart_lights_LightCommandId_MODE,
          kart_common_ValueType_UINT8,
          kart_lights_LightModeValue_OFF);
      break;
    }

    Serial.print(F("TS:"));
    Serial.println(testState);
  }

  // For animation mode, we need to process more frequently
  if (lightState.mode == kart_lights_LightModeValue_ANIM_MODE && 
      animationState.active && 
      !animationState.receivingFrame) {
    updateAnimation();
  } else {
    updateLights(leds, NUM_LEDS, lightState);
  }
  
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

// Animation-related functions

void resetAnimationState()
{
  animationState.active = false;
  animationState.frameCount = 0;
  animationState.currentFrame = 0;
  animationState.lastFrameTime = 0;
  animationState.dataSize = 0;
  animationState.frameSize = 0;
  
  // Clear animation buffer
  memset(animationState.animationData, 0, MAX_ANIMATION_BUFFER_SIZE);
}

void handleAnimationControl(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id)
{
  // Only process if this message is for us (based on component ID)
  if (component_id != kart_lights_LightComponentId_ALL && 
      (component_id != kart_lights_LightComponentId_FRONT && updateFrontLights) && 
      (component_id != kart_lights_LightComponentId_REAR && !updateFrontLights)) {
    return;
  }
  
  // Parse command value
  uint8_t* raw_data = (uint8_t*)data;
  uint8_t value = raw_data[0];
  
  // Animation control: 0 = Stop, 1 = Start, 2 = Pause, 3 = Resume
  switch (value) {
    case 0: // Stop
      animationState.active = false;
      lightState.mode = kart_lights_LightModeValue_OFF;
      updateLights(leds, NUM_LEDS, lightState);
      Serial.println(F("Animation stopped"));
      break;
    
    case 1: // Start
      if (animationState.frameCount > 0) {
        animationState.active = true;
        animationState.currentFrame = 0;
        animationState.lastFrameTime = millis();
        lightState.mode = kart_lights_LightModeValue_ANIMATION;
        Serial.println(F("Animation started"));
      } else {
        Serial.println(F("No animation loaded"));
      }
      break;
    
    case 2: // Pause
      animationState.active = false;
      Serial.println(F("Animation paused"));
      break;
    
    case 3: // Resume
      if (animationState.frameCount > 0) {
        animationState.active = true;
        animationState.lastFrameTime = millis();
        lightState.mode = kart_lights_LightModeValue_ANIMATION;
        Serial.println(F("Animation resumed"));
      }
      break;
      
    default:
      Serial.println(F("Unknown animation control command"));
      break;
  }
}

void handleAnimationConfig(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id)
{
  // Only process if this message is for us (based on component ID)
  if (component_id != kart_lights_LightComponentId_ALL && 
      (component_id != kart_lights_LightComponentId_FRONT && updateFrontLights) && 
      (component_id != kart_lights_LightComponentId_REAR && !updateFrontLights)) {
    return;
  }
  
  // Parse command data
  if (length < 3) {
    Serial.println(F("Invalid animation config data"));
    return;
  }
  
  uint8_t* raw_data = (uint8_t*)data;
  uint8_t config_type = raw_data[0];
  uint8_t config_value = raw_data[1];
  
  switch (config_type) {
    case 0: // FPS
      animationConfig.fps = config_value;
      animationConfig.frameDuration = 1000 / config_value;
      Serial.print(F("Animation FPS set to: "));
      Serial.println(config_value);
      break;
      
    case 1: // Loop setting
      animationConfig.loopAnimation = (config_value > 0);
      Serial.print(F("Animation loop set to: "));
      Serial.println(animationConfig.loopAnimation ? F("Enabled") : F("Disabled"));
      break;
      
    case 2: // Brightness
      if (config_value >= MIN_BRIGHTNESS && config_value <= MAX_BRIGHTNESS) {
        animationConfig.brightness = config_value;
        Serial.print(F("Animation brightness set to: "));
        Serial.println(config_value);
      } else {
        Serial.println(F("Brightness out of range"));
      }
      break;
      
    default:
      Serial.println(F("Unknown animation config type"));
      break;
  }
}

void processAnimationMessage(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id)
{
  // Only process if this message is for us (based on component ID)
  if (component_id != kart_lights_LightComponentId_ALL && 
      (component_id != kart_lights_LightComponentId_FRONT && updateFrontLights) && 
      (component_id != kart_lights_LightComponentId_REAR && !updateFrontLights)) {
    return;
  }
  
  // Check for header packet (first packet in the animation data stream)
  uint8_t* raw_data = (uint8_t*)data;
  
  if (length >= 8 && raw_data[0] == 0xFF && raw_data[1] == 0xAA) {
    // This is a header packet, format:
    // [0xFF, 0xAA, frameCount(2), frameSize(2), totalSize(2), ...]
    uint16_t frameCount = (raw_data[2] << 8) | raw_data[3];
    uint16_t frameSize = (raw_data[4] << 8) | raw_data[5];
    uint16_t totalSize = (raw_data[6] << 8) | raw_data[7];
    
    // Reset animation state for new data
    resetAnimationState();
    
    // Store animation parameters
    animationState.frameCount = frameCount;
    animationState.frameSize = frameSize;
    
    Serial.print(F("New animation: "));
    Serial.print(frameCount);
    Serial.print(F(" frames, "));
    Serial.print(frameSize);
    Serial.print(F(" bytes/frame, "));
    Serial.print(totalSize);
    Serial.println(F(" total bytes"));
    
    // Check if the animation will fit in our buffer
    if (totalSize > MAX_ANIMATION_BUFFER_SIZE) {
      Serial.println(F("Animation too large for buffer"));
      resetAnimationState();
      return;
    }
    
    // Copy data payload (after header) to animation buffer
    if (length > 8) {
      uint16_t dataSize = length - 8;
      memcpy(animationState.animationData, raw_data + 8, dataSize);
      animationState.dataSize = dataSize;
      
      Serial.print(F("Received "));
      Serial.print(dataSize);
      Serial.println(F(" bytes of animation data"));
    }
  } else {
    // This is a continuation packet, just append to buffer
    if (animationState.dataSize + length <= MAX_ANIMATION_BUFFER_SIZE) {
      memcpy(animationState.animationData + animationState.dataSize, raw_data, length);
      animationState.dataSize += length;
      
      Serial.print(F("Received additional "));
      Serial.print(length);
      Serial.println(F(" bytes of animation data"));
      
      // Check if we've received the complete animation
      uint32_t expectedSize = animationState.frameCount * animationState.frameSize;
      if (animationState.dataSize >= expectedSize) {
        Serial.println(F("Animation data complete"));
        animationState.active = true;
        animationState.currentFrame = 0;
        animationState.lastFrameTime = millis();
      }
    } else {
      Serial.println(F("Animation buffer overflow"));
    }
  }
}

void updateAnimation()
{
  if (!animationState.active || animationState.frameCount == 0) {
    return;
  }
  
  uint32_t currentTime = millis();
  if (currentTime - animationState.lastFrameTime >= animationConfig.frameDuration) {
    // Time to display the next frame
    displayAnimationFrame(animationState.currentFrame);
    
    // Update timing and frame counter
    animationState.lastFrameTime = currentTime;
    animationState.currentFrame++;
    
    // Loop back to the beginning if needed
    if (animationState.currentFrame >= animationState.frameCount) {
      if (animationConfig.loopAnimation) {
        animationState.currentFrame = 0;
      } else {
        // Stop animation playback if not looping
        animationState.active = false;
        
        // Go back to the previous light mode
        lightState.mode = kart_lights_LightModeValue_OFF;
        updateLights(leds, NUM_LEDS, lightState);
      }
    }
  }
}

void displayAnimationFrame(uint32_t frameIndex)
{
  if (!animationState.active || frameIndex >= animationState.frameCount) {
    return;
  }

  // Calculate the start position in the animation buffer for this frame
  uint32_t startPos = frameIndex * animationState.frameSize;
  
  // Check if we're updating front or rear
  uint16_t startLed = updateFrontLights ? FRONT_START_LED : REAR_START_LED;
  uint16_t endLed = updateFrontLights ? FRONT_END_LED : REAR_END_LED;
  uint16_t ledCount = endLed - startLed + 1;
  
  // Make sure we don't exceed the LED count
  if (animationState.frameSize / 3 > ledCount) {
    return;
  }
  
  // Copy RGB data to the LED buffer
  for (uint16_t i = 0; i < ledCount; i++) {
    uint32_t pixelPos = startPos + (i * 3);
    if (pixelPos + 2 < MAX_ANIMATION_BUFFER_SIZE) {
      leds[startLed + i] = CRGB(
        animationState.animationData[pixelPos],
        animationState.animationData[pixelPos + 1],
        animationState.animationData[pixelPos + 2]
      );
    }
  }
  
  FastLED.setBrightness(animationConfig.brightness);
  FastLED.show();
}

#endif