#include <Arduino.h>
#include <FastLED.h>

#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "common.pb.h"
#include "lights.pb.h"
#include "controls.pb.h"

// Global state variables
LightState lightState{};
AnimationState animationState{};
AnimationConfig animationConfig{};
bool testModeActive = false;
CRGB leds[NUM_LEDS];
bool updateFrontLights = true; // Default to updating front lights
bool locationSelected = false; // whether the primary controller has told this secondary which lights it controls

// Animation and turn signal timing variables
unsigned long lastTurnUpdate = 0;
unsigned long lastAnimationUpdate = 0;
bool turnSignalState = true;
unsigned long lastTurnToggle;
unsigned long lastSweepUpdate;

void setupLightsForTesting();

// todo: this should be pulled from EEPROM as a unique id
ProtobufCANInterface canInterface(0x01);

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
  canInterface.begin(canInterfaceConfig);

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
  canInterface.registerCommandHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightCommandId_MODE,
    handleLightMode);

  canInterface.registerCommandHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightCommandId_SIGNAL,
    handleLightSignal);

  canInterface.registerCommandHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightCommandId_BRAKE,
    handleLightBrake);

  canInterface.registerCommandHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightCommandId_TEST,
    handleLightTest);

  canInterface.registerCommandHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightCommandId_LOCATION,
    handleLightLocation);

  // Register animation handlers
  canInterface.registerCommandHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightCommandId_ANIMATION,
    handleAnimationControl);

  canInterface.registerCommandHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightCommandId_ANIMATION_CONFIG,
    handleAnimationConfig);

  // Register animation data handler
  canInterface.registerAnimationHandler(
    kart_common_ComponentType_LIGHTS,
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
  canInterface.loop();

  // Run test sequence if enabled
  if (testModeActive)
  {
    runTestSequence();
    return; // Skip normal updates when in test mode
  }

  // Animation mode has priority over other light modes
  if (lightState.mode == kart_lights_LightModeValue_ANIMATION && 
      animationState.isPlaying && 
      !animationState.receivingFrame) 
  {
    // Update animation using current animation frame
    updateAnimation(leds, NUM_LEDS, animationState, animationConfig);
  } 
  else 
  {
    // Regular light update based on current light state
    updateLights(leds, NUM_LEDS, lightState);
  }

  // Show the updated LED values
  FastLED.show();

  // Small delay to prevent excessive updates
  delay(10);
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
  // Only process if we match our location or it's a broadcast
  if (component_id != kart_lights_LightComponentId_ALL && 
      (updateFrontLights && component_id != kart_lights_LightComponentId_FRONT) &&
      (!updateFrontLights && component_id != kart_lights_LightComponentId_REAR)) {
    return;
  }
  
  // If switching to animation mode, reset animation state
  if (value == kart_lights_LightModeValue_ANIMATION && lightState.mode != kart_lights_LightModeValue_ANIMATION) {
    resetAnimationState();
    FastLED.setBrightness(animationConfig.brightness);
  }
  
  lightState.mode = value;
  Serial.print(F("Light mode: "));
  Serial.println(lightState.mode);
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
  locationSelected = true;
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
          kart_lights_LightModeValue_ANIMATION);
      
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
  if (lightState.mode == kart_lights_LightModeValue_ANIMATION && 
      animationState.isPlaying && 
      !animationState.receivingFrame) {
    updateAnimation(leds, NUM_LEDS, animationState, animationConfig);
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
  animationState.currentFrame = 0;
  animationState.receivingFrame = false;
  animationState.frameComplete = false;
  animationState.totalFrames = 0;
  animationState.receivedFrames = 0;
  animationState.lastFrameTime = 0;
  animationState.isPlaying = false;
  animationState.singlePixelIndex = 0;
}

void handleAnimationControl(uint8_t messageType, uint8_t componentType, uint8_t commandFlag, uint8_t componentId, uint8_t commandId, uint32_t value)
{
  // Only process messages for this component or broadcasts
  if (componentId != kart_lights_LightComponentId_ALL && 
      componentId != (updateFrontLights ? kart_lights_LightComponentId_FRONT : kart_lights_LightComponentId_REAR)) {
    return;
  }

  // Extract the control command
  uint8_t animCommand = value & 0xFF;
  
  // Handle special case for total frames (value > 100)
  if (animCommand >= 100) {
    animationState.totalFrames = animCommand - 100;
    Serial.print(F("Animation total frames: "));
    Serial.println(animationState.totalFrames);
    return;
  }

  switch (animCommand) {
    case kart_lights_AnimationCommandValue_ANIM_PLAY:
      // Start playing animation from start
      animationState.isPlaying = true;
      animationState.currentFrame = 0;
      animationState.lastFrameTime = millis();
      Serial.println(F("Animation: Play"));
      break;
    
    case kart_lights_AnimationCommandValue_ANIM_PAUSE:
      // Pause animation at current frame
      animationState.isPlaying = false;
      Serial.println(F("Animation: Pause"));
      break;
    
    case kart_lights_AnimationCommandValue_ANIM_STOP:
      // Stop animation and reset
      animationState.isPlaying = false;
      animationState.currentFrame = 0;
      Serial.println(F("Animation: Stop"));
      break;
    
    case kart_lights_AnimationCommandValue_ANIM_NEXT:
      // Move to next frame
      if (animationState.totalFrames > 0) {
        animationState.currentFrame = (animationState.currentFrame + 1) % animationState.totalFrames;
        animationState.lastFrameTime = millis();
        displayAnimationFrame(leds, NUM_LEDS, animationState);
      }
      Serial.print(F("Animation: Next frame "));
      Serial.println(animationState.currentFrame);
      break;
    
    case kart_lights_AnimationCommandValue_ANIM_PREV:
      // Move to previous frame
      if (animationState.totalFrames > 0) {
        animationState.currentFrame = (animationState.currentFrame > 0) ? 
                                      (animationState.currentFrame - 1) : 
                                      (animationState.totalFrames - 1);
        animationState.lastFrameTime = millis();
        displayAnimationFrame(leds, NUM_LEDS, animationState);
      }
      Serial.print(F("Animation: Previous frame "));
      Serial.println(animationState.currentFrame);
      break;
  }
}

void handleAnimationConfig(uint8_t messageType, uint8_t componentType, uint8_t commandFlag, uint8_t componentId, uint8_t commandId, uint32_t value)
{
  // Only process messages for this component or broadcasts
  if (componentId != kart_lights_LightComponentId_ALL && 
      componentId != (updateFrontLights ? kart_lights_LightComponentId_FRONT : kart_lights_LightComponentId_REAR)) {
    return;
  }

  // Value bits:
  // [7:0] = FPS (1-60)
  // [8] = Loop flag (0=no loop, 1=loop)
  uint8_t fps = value & 0xFF;
  bool loopEnabled = (value & 0x100) != 0;
  
  if (fps < 1) fps = 1;
  if (fps > 60) fps = 60;
  
  animationConfig.fps = fps;
  animationConfig.frameDuration = 1000 / fps;
  animationConfig.loopAnimation = loopEnabled;
  
  Serial.print(F("Animation Config - FPS: "));
  Serial.print(fps);
  Serial.print(F(", Loop: "));
  Serial.println(loopEnabled ? "Yes" : "No");
}

void processAnimationMessage(uint8_t messageType, uint8_t componentType, uint8_t commandFlag, uint8_t componentId, uint8_t commandId, uint32_t value)
{
  // Only process messages for this component or broadcasts
  if (componentId != kart_lights_LightComponentId_ALL && 
      componentId != (updateFrontLights ? kart_lights_LightComponentId_FRONT : kart_lights_LightComponentId_REAR)) {
    return;
  }
  
  // Check if we're in animation mode
  if (lightState.mode != kart_lights_LightModeValue_ANIMATION) {
    return;
  }
  
  // Extract RGB values from the command value
  uint8_t r = (value >> 16) & 0xFF;
  uint8_t g = (value >> 8) & 0xFF;
  uint8_t b = value & 0xFF;
  
  // Handle animation frame flags
  switch (commandFlag) {
    case kart_common_AnimationFlag_ANIMATION_START:
      // Start of a new frame
      animationState.receivingFrame = true;
      animationState.frameComplete = false;
      animationState.singlePixelIndex = 0;
      Serial.print(F("Animation frame start "));
      Serial.println(animationState.receivedFrames);
      break;
      
    case kart_common_AnimationFlag_ANIMATION_FRAME:
      // Middle of a frame data
      if (animationState.receivingFrame && animationState.singlePixelIndex < NUM_LEDS) {
        // Store pixel data in the animation buffer
        uint16_t bufferIndex = (animationState.currentFrame * NUM_LEDS + animationState.singlePixelIndex) % MAX_ANIMATION_BUFFER_SIZE;
        animationState.frameBuffer[bufferIndex] = CRGB(r, g, b);
        animationState.singlePixelIndex++;
      }
      break;
      
    case kart_common_AnimationFlag_ANIMATION_END:
      // End of a frame
      if (animationState.receivingFrame) {
        // Complete the frame
        if (animationState.singlePixelIndex < NUM_LEDS) {
          // Fill remaining pixels with the last color
          uint16_t bufferIndex;
          for (uint16_t i = animationState.singlePixelIndex; i < NUM_LEDS; i++) {
            bufferIndex = (animationState.currentFrame * NUM_LEDS + i) % MAX_ANIMATION_BUFFER_SIZE;
            animationState.frameBuffer[bufferIndex] = CRGB(r, g, b);
          }
        }
        
        animationState.receivingFrame = false;
        animationState.frameComplete = true;
        
        // Increment received frames counter
        animationState.receivedFrames++;
        
        // If this is the first frame, automatically display it
        if (animationState.receivedFrames == 1) {
          displayAnimationFrame(leds, NUM_LEDS, animationState);
        }
        
        Serial.print(F("Animation frame complete "));
        Serial.println(animationState.receivedFrames);
      }
      break;
  }
}

void displayAnimationFrame(CRGB* ledArray, uint16_t numLeds, AnimationState& anim)
{
  if (anim.totalFrames == 0 || anim.currentFrame >= anim.totalFrames) {
    return;
  }
  
  // Copy the frame data to the LED array
  for (uint16_t i = 0; i < numLeds; i++) {
    uint16_t bufferIndex = (anim.currentFrame * numLeds + i) % MAX_ANIMATION_BUFFER_SIZE;
    ledArray[i] = anim.frameBuffer[bufferIndex];
  }
}

void updateAnimation(CRGB* ledArray, uint16_t numLeds, AnimationState& anim, const AnimationConfig& config)
{
  if (!anim.isPlaying || anim.totalFrames == 0) {
    return;
  }
  
  unsigned long currentMillis = millis();
  
  // Check if it's time to update to the next frame
  if (currentMillis - anim.lastFrameTime >= config.frameDuration) {
    anim.lastFrameTime = currentMillis;
    
    // Move to the next frame
    anim.currentFrame++;
    
    // Check if we've reached the end of the animation
    if (anim.currentFrame >= anim.totalFrames) {
      if (config.loopAnimation) {
        // Loop back to the beginning
        anim.currentFrame = 0;
      } else {
        // Stop the animation
        anim.isPlaying = false;
        return;
      }
    }
    
    // Display the current frame
    displayAnimationFrame(ledArray, numLeds, anim);
  }
}

#endif