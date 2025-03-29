#include <Arduino.h>
#include <FastLED.h>

#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "../include/AnimationProtocol.h"
#include "common.pb.h"
#include "lights.pb.h"
#include "controls.pb.h"

// Global state variables
LightState lightState{};
bool testModeActive = false;
CRGB leds[NUM_LEDS];
bool updateFrontLights = true; // Default to updating front lights
bool locationSelected = false; // whether the primary controller has told this secondary which lights it controls

// Animation protocol handler
AnimationProtocol animationProtocol;
bool animationMode = false; // Whether we're currently in structured animation stream mode OR real-time mode

// Buffer for reassembling raw frame data chunks
const size_t frameTotalSize = NUM_LEDS * 3;
uint8_t frameBuffer[frameTotalSize];
size_t frameBufferIndex = 0;

// Define CAN IDs if not already included (matching protocol.py)
#ifndef ANIMATION_CTRL_ID
#define ANIMATION_CTRL_ID 0x700
#endif
#ifndef ANIMATION_DATA_ID
#define ANIMATION_DATA_ID 0x701
#endif


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

// Function to handle raw CAN messages for animations (structured stream or real-time frames)
// Add 'const' to data parameter to match RawMessageHandler type
void handleRawMessage(uint32_t canId, const uint8_t* data, uint8_t length) {
  // Check if it's a structured animation stream control/data message
  // Use the public isAnimationActive() method as intended
  if (canId == ANIM_CTRL_ID || (canId == ANIMATION_DATA_ID && animationProtocol.isAnimationActive())) {
      // Let the AnimationProtocol library handle structured stream messages
      if (animationProtocol.processMessage(canId, data, length)) {
          // If processing indicates a stream just started, ensure we're in animationMode
          if (!animationMode && animationProtocol.isAnimationActive()) {
              animationMode = true;
#if DEBUG_MODE
              Serial.println(F("Switched to structured animation mode"));
#endif
          }
          // If processing indicates a stream just ended, switch back
          else if (animationMode && !animationProtocol.isAnimationActive()) {
              animationMode = false;
              frameBufferIndex = 0; // Reset buffer index when stream ends
#if DEBUG_MODE
              Serial.println(F("Switched back to normal mode (stream ended)"));
#endif
          }
      }
  }
  // Check if it's a raw real-time frame data message (and not part of an active structured stream)
  else if (canId == ANIMATION_DATA_ID) {
      // Assume we should be in animationMode if receiving raw data directly
      // (Server should send CONFIG_MODE=MODE_ANIMATION first)
      if (!animationMode) {
          animationMode = true; // Enter animation mode if not already in it
          frameBufferIndex = 0; // Reset buffer index when starting
#if DEBUG_MODE
          Serial.println(F("Switched to real-time animation mode"));
#endif
      }

      // Append data to buffer if there's space
      if (frameBufferIndex + length <= frameTotalSize) {
          // Cast away const for memcpy destination, buffer is writable
          memcpy(&frameBuffer[frameBufferIndex], (const void*)data, length);
          frameBufferIndex += length;

          // Check if a full frame has been received
          if (frameBufferIndex >= frameTotalSize) {
              // Process the complete frame: Copy buffer to leds array
              for (int i = 0; i < NUM_LEDS; ++i) {
                  size_t bufferPos = i * 3;
                  if (bufferPos + 2 < frameTotalSize) { // Check bounds
                     leds[i].setRGB(frameBuffer[bufferPos], frameBuffer[bufferPos + 1], frameBuffer[bufferPos + 2]);
                  }
              }
              #ifndef UNIT_TEST
              FastLED.show(); // Update the physical LEDs
              #endif
              frameBufferIndex = 0; // Reset for next frame
          }
      } else {
          // Buffer overflow or unexpected data length, reset buffer
          frameBufferIndex = 0;
#if DEBUG_MODE
          Serial.println(F("WARN: Frame buffer overflow or unexpected length, resetting."));
#endif
      }
  }
  // --- The original logic below is now handled above ---
  /*
    // If we're not already in animation mode, switch to it
    if (!animationMode && animationProtocol.isAnimationActive()) {
      animationMode = true;
    }
  */
}

void setup()
{
#if DEBUG_MODE
  Serial.begin(115200);
#endif

#ifndef UNIT_TEST // Prevent hardware init during unit tests
  if (!canInterface.begin(500E3))
  {
    // Handle CAN init failure (e.g., log error, halt)
    // In a real scenario, you might want robust error handling here.
    // For now, we'll just loop indefinitely on failure.
    #if DEBUG_MODE
    Serial.println(F("CAN Initialization Failed!"));
    #endif
    while (1);
  }

  // Initialize LED strips
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
#endif // UNIT_TEST

  // Initialize animation protocol with our LED array
  // This might be okay in tests if it doesn't directly touch hardware,
  // but review AnimationProtocol if issues persist.
  animationProtocol.begin(leds, NUM_LEDS);

  // Initialize all light states to default
  lightState.mode = 0; // Lights disabled by default
  clearLights(leds, NUM_LEDS);

  // Register message handlers
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_DIAGNOSTIC, kart_controls_ControlCommandId_MODE, handleLightTest);

  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_MODE, handleLightMode);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_SIGNAL, handleLightSignal);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_BRAKE, handleLightBrake);

  // Register raw message handler for animation control and data IDs
  // Use registerRawHandler instead of non-existent setRawMessageHandler
  canInterface.registerRawHandler(ANIM_CTRL_ID, handleRawMessage);
  canInterface.registerRawHandler(ANIMATION_DATA_ID, handleRawMessage);

  // todo: remove these in favor of EEPROM-supported node identification
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_LOCATION, handleLightLocation);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_LOCATION, handleLightLocation);

  Serial.println(F("Go-Kart Lights with Animation Support"));

#if DEBUG_MODE
  setupLightsForTesting();
#endif
}

void loop()
{
  if (!testModeActive)
  {
    if (animationMode) {
      // Update the animation protocol if we're in animation mode
      // The protocol handles displaying frames itself
      animationProtocol.update();
    } else {
      // Otherwise run the normal light patterns
      updateLights(leds, NUM_LEDS, lightState);
    }
  }

  // Process incoming CAN messages
  canInterface.process();

  // Small delay to prevent flickering
  delay(10);

#if DEBUG_MODE
  if (testModeActive)
  {
    // Run the test sequence if in test mode
    runTestSequence();
  }
#endif

  // Add diagnostics for memory usage when in debug mode
#if DEBUG_MODE
  static unsigned long lastMemReport = 0;
  if (millis() - lastMemReport > 10000) { // Report every 10 seconds
    lastMemReport = millis();
    if (animationMode) {
      Serial.print(F("Animation memory usage: "));
      Serial.print(animationProtocol.getMemoryUsage());
      Serial.print(F(" bytes, frames: "));
      Serial.print(animationProtocol.getReceivedFrames());
      Serial.print(F(", dropped: "));
      Serial.println(animationProtocol.getDroppedFrames());
    }
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

#endif
