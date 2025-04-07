#include <Arduino.h>
#include <FastLED.h>

#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "common.pb.h"
#include "lights.pb.h"
#include "controls.pb.h"

// Global state variables
LightState lightState{};
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

  // Register handlers with the new format
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_DIAGNOSTIC, kart_controls_ControlCommandId_MODE, handleLightTest);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, 0xFF, kart_lights_LightCommandId_MODE, handleLightMode);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, 0xFF, kart_lights_LightCommandId_SIGNAL, handleLightSignal);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS, 0xFF, kart_lights_LightCommandId_BRAKE, handleLightBrake);

  Serial.println(F("Go-Kart Lights"));
}

void loop()
{
  if (!testModeActive)
  {
    // Update light states based on current settings
    updateLights(leds, NUM_LEDS, lightState);

    // Show the updated lights
    FastLED.show();
  }

  // Small delay to prevent flickering
  delay(10);
  canInterface.process();
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
  // FastLED.show();
}

void clearLights(CRGB *leds, int numLeds)
{
  // Turn off all LEDs
  fill_solid(leds, numLeds, CRGB::Black);
}

void handleLightMode(uint16_t message_id, int32_t value)
{
  lightState.mode = value;
  Serial.print(F("Light mode: "));
  Serial.println(lightState.mode);
}

void handleLightSignal(uint16_t message_id, int32_t value)
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

void handleLightBrake(uint16_t message_id, int32_t value)
{
  lightState.braking = (value == 1) ? 1 : 0;
  Serial.print(F("Brake: "));
  Serial.println(lightState.braking);
}

void handleLightTest(uint16_t message_id, int32_t value)
{
  testModeActive = (value == kart_controls_ControlModeValue_TEST);
  Serial.print(F("Test mode: "));
  Serial.print(value);
  Serial.print(" ");
  Serial.print(kart_controls_ControlModeValue_TEST);
  Serial.print(" ");
  Serial.println(testModeActive);
}

void handleLightLocation(uint16_t message_id, int32_t value)
{
  updateFrontLights = (value == 1);
  locationSelected = true;
  Serial.print(F("Light location: "));
  Serial.println(updateFrontLights ? F("Front") : F("Rear"));
}