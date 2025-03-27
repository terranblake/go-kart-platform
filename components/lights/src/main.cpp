#include <Arduino.h>
#include <FastLED.h>
#include <SPI.h>
#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "../include/LightConstants.h"
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

// Utility functions
void clearLights(CRGB* leds, int numLeds) {
  for (int i = 0; i < numLeds; i++) {
    leds[i] = CRGB::Black;
  }
  FastLED.show();
}

void updateStartupShutdownAnimation(CRGB* leds, int numLeds, LightState& lightState) {
  static uint8_t startupStep = 0;
  static uint8_t shutdownStep = 0;
  static unsigned long lastAnimationTime = 0;
  
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastAnimationTime < ANIMATION_STEP_DELAY) {
    return;
  }
  
  lastAnimationTime = currentMillis;
  
  if (lightState.animation == 1) { // Startup animation
    clearLights(leds, numLeds);
    
    if (startupStep <= 100) {
      int brightness = map(startupStep, 0, 100, 0, lightState.brightness);
      
      // For startup, gradually increase brightness from center outward
      int centerLed = numLeds / 2;
      int range = map(startupStep, 0, 100, 0, centerLed);
      
      for (int i = centerLed - range; i <= centerLed + range; i++) {
        if (i >= 0 && i < numLeds) {
          leds[i] = CRGB::White;
          leds[i].fadeToBlackBy(255 - brightness);
        }
      }
      
      startupStep += 2;
      if (startupStep > 100) {
        lightState.animation = 0; // Animation complete
        startupStep = 0;
      }
    }
    
    FastLED.show();
  } 
  else if (lightState.animation == 2) { // Shutdown animation
    if (shutdownStep <= 100) {
      int brightness = map(shutdownStep, 0, 100, lightState.brightness, 0);
      
      // For shutdown, gradually decrease brightness from outside inward
      int centerLed = numLeds / 2;
      int range = map(shutdownStep, 0, 100, centerLed, 0);
      
      for (int i = 0; i < numLeds; i++) {
        if (i < centerLed - range || i > centerLed + range) {
          leds[i] = CRGB::Black;
        } else {
          leds[i].fadeToBlackBy(255 - brightness);
        }
      }
      
      shutdownStep += 2;
      if (shutdownStep > 100) {
        lightState.animation = 0; // Animation complete
        clearLights(leds, numLeds);
        shutdownStep = 0;
      }
    }
    
    FastLED.show();
  }
}

void executeSerialCommand() {
  // Check if any serial data is available
  if (Serial.available()) {
    // Read the command
    char command[32];
    int idx = 0;
    
    // Read until newline or end of buffer
    while (Serial.available() && idx < 31) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') break;
      command[idx++] = c;
    }
    command[idx] = '\0';
    
    // Basic command parsing
    if (strncmp(command, "mode:", 5) == 0) {
      int mode = atoi(command + 5);
      if (mode >= 0 && mode <= kart_lights_LightModeValue_ANIMATION) {
        lightState.mode = (kart_lights_LightModeValue)mode;
        Serial.print("Mode set to: ");
        Serial.println(mode);
      }
    }
    else if (strncmp(command, "brightness:", 11) == 0) {
      int brightness = atoi(command + 11);
      if (brightness >= MIN_BRIGHTNESS && brightness <= MAX_BRIGHTNESS) {
        lightState.brightness = brightness;
        Serial.print("Brightness set to: ");
        Serial.println(brightness);
      }
    }
    else if (strncmp(command, "signal:", 7) == 0) {
      int signal = atoi(command + 7);
      if (signal == 0) {
        lightState.turnLeft = 0;
        lightState.turnRight = 0;
        lightState.hazard = 0;
        Serial.println("Signals cleared");
      } 
      else if (signal == 1) {
        lightState.turnLeft = 1;
        lightState.turnRight = 0;
        lightState.hazard = 0;
        Serial.println("Left signal activated");
      } 
      else if (signal == 2) {
        lightState.turnLeft = 0;
        lightState.turnRight = 1;
        lightState.hazard = 0;
        Serial.println("Right signal activated");
      } 
      else if (signal == 3) {
        lightState.turnLeft = 1;
        lightState.turnRight = 1;
        lightState.hazard = 1;
        Serial.println("Hazard activated");
      }
    }
    else if (strncmp(command, "brake:", 6) == 0) {
      int brake = atoi(command + 6);
      lightState.braking = brake > 0 ? 1 : 0;
      Serial.print("Brake set to: ");
      Serial.println(lightState.braking);
    }
    else if (strcmp(command, "test") == 0) {
    runTestSequence();
      Serial.println("Running test sequence");
    }
  }
}

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
  if (!canInterface.begin(500000, "can0")) {
    Serial.println("Failed to initialize CAN interface");
  }

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

  // Register message handlers for various light commands
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    NODE_ID,
    kart_lights_LightCommandId_MODE,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
       uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType value_type, int32_t value) {
      handleLightMode(&value, sizeof(value), comp_type, comp_id, cmd_id);
    }
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    NODE_ID,
    kart_lights_LightCommandId_SIGNAL,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
       uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType value_type, int32_t value) {
      handleLightSignal(&value, sizeof(value), comp_type, comp_id, cmd_id);
    }
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    NODE_ID,
    kart_lights_LightCommandId_BRAKE,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
       uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType value_type, int32_t value) {
      handleLightBrake(&value, sizeof(value), comp_type, comp_id, cmd_id);
    }
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    NODE_ID,
    kart_lights_LightCommandId_TEST,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
       uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType value_type, int32_t value) {
      handleLightTest(&value, sizeof(value), comp_type, comp_id, cmd_id);
    }
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    NODE_ID,
    kart_lights_LightCommandId_LOCATION,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
       uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType value_type, int32_t value) {
      handleLightLocation(&value, sizeof(value), comp_type, comp_id, cmd_id);
    }
  );
  
  // Register animation control handler
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    NODE_ID,
    kart_lights_LightCommandId_ANIMATION_CONTROL,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
       uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType value_type, int32_t value) {
      handleAnimationControl(&value, sizeof(value), comp_type, comp_id, cmd_id);
    }
  );
  
  // Register animation config handler
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    NODE_ID,
    kart_lights_LightCommandId_ANIMATION_CONFIG,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
       uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType value_type, int32_t value) {
      handleAnimationConfig(&value, sizeof(value), comp_type, comp_id, cmd_id);
    }
  );
  
  // Register animation data handler
  canInterface.registerAnimationHandler(
    NODE_ID,
    kart_lights_LightCommandId_ANIMATION_DATA,
    [](uint8_t component_id, uint8_t command_id, const uint8_t* data, size_t length, bool isLastChunk) {
      processAnimationMessage(component_id, command_id, data, length, isLastChunk);
    }
  );
  
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
  canInterface.process();
  
  // Process any incoming serial commands
  executeSerialCommand();
  
  // Check if we need to update animation playback
  if (animationState.active && lightState.mode == kart_lights_LightModeValue_ANIMATION) {
    updateAnimation();
  } else {
    // Update regular light patterns based on current mode
    updateLights(leds, NUM_LEDS, lightState);
  }
  
  // Run test sequence if enabled
  if (RUN_TEST_SEQUENCE) {
    runTestSequence();
  }
}

void updateLights(CRGB* leds, int numLeds, LightState& lightState) {
  static unsigned long lastUpdateTime = 0;
  unsigned long currentMillis = millis();
  
  // No need to update too frequently
  if (currentMillis - lastUpdateTime < 20) {
    return;
  }
  lastUpdateTime = currentMillis;
  
  // Startup/shutdown animations take precedence
  if (lightState.animation > 0) {
    updateStartupShutdownAnimation(leds, numLeds, lightState);
    return;
  }

  // Handle different lighting modes
  switch (lightState.mode) {
    case kart_lights_LightModeValue_OFF:
  clearLights(leds, numLeds);
      break;
      
    case kart_lights_LightModeValue_ON:
    case kart_lights_LightModeValue_AUTO:
    case kart_lights_LightModeValue_DIM:
      // Set base lighting
      uint8_t brightness = lightState.brightness;
      if (lightState.mode == kart_lights_LightModeValue_DIM) {
        brightness = MIN_BRIGHTNESS;
      } else if (lightState.mode == kart_lights_LightModeValue_BRIGHT) {
    brightness = BRIGHT_BRIGHTNESS;
      } else if (lightState.braking) {
    brightness = BRAKE_BRIGHTNESS;
  }

      // Configure the main LED area
    int mainStart = TURN_SIGNAL_COUNT;
    int mainEnd = numLeds - TURN_SIGNAL_COUNT;

      for (int i = mainStart; i < mainEnd; i++) {
        // Different color for front/rear
        if (i < numLeds/2) {
          // Front lights are white
          leds[i] = CRGB::White;
        } else {
          // Rear lights are red
          leds[i] = CRGB::Red;
        }
        
        // Apply brightness
        leds[i].fadeToBlackBy(255 - brightness);
      }
      
      // Update turn signals on top of base lighting
  updateTurnSignals(leds, numLeds, lightState);
      break;
      
    // Other modes as needed
  }
  
  // Update brake lights if needed
  if (lightState.braking) {
    updateBrake(leds, numLeds, lightState);
  }
  
  // Show the updated LEDs
  FastLED.show();
}

void updateTurnSignals(CRGB* leds, int numLeds, LightState& lightState) {
  static unsigned long lastTurnToggle = 0;
  static unsigned long lastSweepUpdate = 0;
  static bool turnSignalState = false;
  unsigned long currentMillis = millis();

  // If no signals active, early exit
  if (!lightState.turnLeft && !lightState.turnRight && !lightState.hazard) {
    return;
  }
  
  // Main section start/end
    int mainStart = TURN_SIGNAL_COUNT;
    int mainEnd = numLeds - TURN_SIGNAL_COUNT;

  // Hazard lights blink both turn signals
  if (lightState.hazard) {
    // Blink rate control
    if (currentMillis - lastTurnToggle > TURN_SIGNAL_BLINK_RATE) {
      lastTurnToggle = currentMillis;
      turnSignalState = !turnSignalState;
    }
    
    if (turnSignalState) {
      // Left and right signals
      for (int i = 0; i < mainStart; i++) {
        leds[i] = CRGB::Orange;
      }
      for (int i = mainEnd; i < numLeds; i++) {
        leds[i] = CRGB::Orange;
      }
    } else {
      // Off phase
      for (int i = 0; i < mainStart; i++) {
        leds[i] = CRGB::Black;
      }
      for (int i = mainEnd; i < numLeds; i++) {
        leds[i] = CRGB::Black;
      }
    }
    return;
  }

  // Normal turn signal blinking
  if (currentMillis - lastTurnToggle > TURN_SIGNAL_BLINK_RATE) {
    lastTurnToggle = currentMillis;
    turnSignalState = !turnSignalState;
  }
  
  // Left signal
  if (lightState.turnLeft && turnSignalState) {
    for (int i = 0; i < mainStart; i++) {
      leds[i] = CRGB::Orange;
    }
  } else if (lightState.turnLeft && !turnSignalState) {
    for (int i = 0; i < mainStart; i++) {
      leds[i] = CRGB::Black;
    }
  }
  
  // Right signal
  if (lightState.turnRight && turnSignalState) {
    for (int i = mainEnd; i < numLeds; i++) {
      leds[i] = CRGB::Orange;
    }
  } else if (lightState.turnRight && !turnSignalState) {
    for (int i = mainEnd; i < numLeds; i++) {
      leds[i] = CRGB::Black;
    }
  }
  
  // Animated sweep for turn signals if needed
  if (currentMillis - lastSweepUpdate > TURN_SIGNAL_SWEEP_STEP && lightState.sweepPosition < TURN_SIGNAL_COUNT) {
    lastSweepUpdate = currentMillis;
    
    // Update sweep position
    lightState.sweepPosition++;
    
    // Apply sweep effect to turn signals
    if (lightState.turnLeft) {
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++) {
        if (i < lightState.sweepPosition) {
          leds[TURN_SIGNAL_COUNT - 1 - i] = CRGB::Orange;
        }
      }
    }

    if (lightState.turnRight) {
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++) {
        if (i < lightState.sweepPosition) {
          leds[numLeds - TURN_SIGNAL_COUNT + i] = CRGB::Orange;
        }
      }
    }
  }
}

void updateBrake(CRGB* leds, int numLeds, LightState& lightState) {
  static unsigned long lastPulseTime = 0;
  static uint8_t pulseDir = 0;  // 0 = up, 1 = down
  static uint8_t pulseValue = 0;
  unsigned long currentMillis = millis();
  
  // Only apply to rear lights
  int rearStart = numLeds / 2;
  
  if (lightState.braking) {
    // Determine if we want a solid brake light or pulsing effect
    if (lightState.mode == kart_lights_LightModeValue_PULSE) {
      // Pulse effect timing
      if (currentMillis - lastPulseTime > BRAKE_PULSE_INTERVAL) {
        lastPulseTime = currentMillis;
        
        // Update pulse value
        if (pulseDir == 0) {
          pulseValue += 5;
          if (pulseValue >= 250) pulseDir = 1;
        } else {
          pulseValue -= 5;
          if (pulseValue <= 50) pulseDir = 0;
        }
      }
      
      // Apply pulse effect to rear lights
      for (int i = rearStart; i < numLeds; i++) {
        // Skip turn signal area
        if (i >= numLeds - TURN_SIGNAL_COUNT) continue;
        
        // Apply pulsing red
        leds[i] = CRGB::Red;
        leds[i].fadeToBlackBy(255 - pulseValue);
      }
    } else {
      // Solid bright brake lights
      for (int i = rearStart; i < numLeds; i++) {
        // Skip turn signal area
        if (i >= numLeds - TURN_SIGNAL_COUNT) continue;
        
        // Full brightness red
            leds[i] = CRGB::Red;
          }
        }
      }
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

void runTestSequence() {
  static unsigned long lastTestStep = 0;
  static uint8_t testStep = 0;
  static bool testActive = false;
  static const unsigned long TEST_STEP_DELAY = 1000;

  unsigned long currentMillis = millis();

  // Start a new test sequence
  if (!testActive) {
    testActive = true;
    testStep = 0;
    lastTestStep = currentMillis;
    Serial.println("Starting test sequence");
  }
  
  // Only advance to next step after delay
  if (currentMillis - lastTestStep < TEST_STEP_DELAY) {
    return;
  }
  
  lastTestStep = currentMillis;
  
  // Run through various test steps
  switch (testStep) {
    case 0:
      // Test basic light modes
      lightState.mode = kart_lights_LightModeValue_ON;
      lightState.brightness = DEFAULT_BRIGHTNESS;
    lightState.turnLeft = 0;
    lightState.turnRight = 0;
      lightState.hazard = 0;
    lightState.braking = 0;
      Serial.println("Test: Lights ON");
      break;

    case 1:
      // Test turn signals - left
      lightState.turnLeft = 1;
      lightState.turnRight = 0;
      Serial.println("Test: Left turn signal");
      break;

    case 2:
      // Test turn signals - right
      lightState.turnLeft = 0;
      lightState.turnRight = 1;
      Serial.println("Test: Right turn signal");
      break;

    case 3:
      // Test hazard lights
      lightState.turnLeft = 1;
      lightState.turnRight = 1;
      lightState.hazard = 1;
      Serial.println("Test: Hazard lights");
      break;

    case 4:
      // Test brake lights
      lightState.turnLeft = 0;
      lightState.turnRight = 0;
      lightState.hazard = 0;
      lightState.braking = 1;
      Serial.println("Test: Brake lights");
      break;

    case 5:
      // Test animation mode (if we have animation data)
      if (animationState.dataSize > 0) {
        lightState.mode = kart_lights_LightModeValue_ANIMATION;
        animationState.active = true;
        animationState.currentFrame = 0;
        animationState.lastFrameTime = millis();
        Serial.println("Test: Animation playback");
      } else {
        Serial.println("Test: No animation data available");
      }
      break;

    case 6:
      // End test sequence
      lightState.mode = kart_lights_LightModeValue_OFF;
      lightState.turnLeft = 0;
      lightState.turnRight = 0;
      lightState.hazard = 0;
      lightState.braking = 0;
      animationState.active = false;
      clearLights(leds, NUM_LEDS);
      Serial.println("Test sequence complete");
      testActive = false;
      break;
  }
  
  // Move to next test step
  testStep++;
}

// Animation-related functions

void updateAnimation() {
  // Only proceed if animation is active and we have frames
  if (!animationState.active || animationState.frameCount == 0) {
    return;
  }
  
  unsigned long currentMillis = millis();
  unsigned long frameDuration = 1000 / animationConfig.fps;
  
  // Check if it's time to show the next frame
  if (currentMillis - animationState.lastFrameTime >= frameDuration) {
    animationState.lastFrameTime = currentMillis;
    
    // Display the current frame
    displayAnimationFrame(animationState.currentFrame);
    
    // Advance to next frame
    animationState.currentFrame++;
    
    // Check if we've reached the end of animation
    if (animationState.currentFrame >= animationState.frameCount) {
      if (animationConfig.loopAnimation) {
        // Loop back to start
        animationState.currentFrame = 0;
      } else {
        // End animation playback
        animationState.active = false;
        // Fall back to regular light mode
        lightState.mode = kart_lights_LightModeValue_ON;
        clearLights(leds, NUM_LEDS);
      }
    }
  }
}

void displayAnimationFrame(uint32_t frameIndex) {
  if (!animationState.active || frameIndex >= animationState.frameCount) {
    return;
  }
  
  // Calculate the offset for this frame in the buffer
  uint32_t frameOffset = frameIndex * animationState.frameSize;
  
  // Ensure we don't read past the buffer
  if (frameOffset + animationState.frameSize > animationState.dataSize) {
    return;
  }
  
  // Assuming RGB data format (3 bytes per LED)
  uint8_t* frameData = animationState.animationData + frameOffset;
  
  // Update LEDs with frame data
  for (int i = 0; i < NUM_LEDS && i * 3 < animationState.frameSize; i++) {
    uint8_t r = frameData[i * 3];
    uint8_t g = frameData[i * 3 + 1];
    uint8_t b = frameData[i * 3 + 2];
    
    leds[i] = CRGB(r, g, b);
    
    // Apply brightness scaling if configured
    if (animationConfig.brightness < MAX_BRIGHTNESS) {
      uint8_t scale = map(animationConfig.brightness, 0, MAX_BRIGHTNESS, 0, 255);
      leds[i].fadeToBlackBy(255 - scale);
    }
  }
  
  // Show the updated frame
  FastLED.show();
}

void resetAnimationState() {
  animationState.active = false;
  animationState.frameCount = 0;
  animationState.currentFrame = 0;
  animationState.dataSize = 0;
  animationState.frameSize = 0;
  animationState.lastFrameTime = 0;
  
  // Clear animation data buffer
  memset(animationState.animationData, 0, MAX_ANIMATION_BUFFER_SIZE);
}

void handleAnimationControl(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id) {
  // Only process if this message is for us
  if (component_id != NODE_ID && component_id != kart_lights_LightComponentId_ALL) {
    return;
  }
  
  // Extract the animation command value
  int32_t value = 0;
  if (data && length >= sizeof(value)) {
    memcpy(&value, data, sizeof(value));
  }
  
  Serial.print("Animation control: ");
  Serial.println(value);
  
  switch (value) {
    case kart_lights_AnimationCommandValue_ANIM_PLAY:
      // Start animation playback
      if (animationState.dataSize > 0) {
        lightState.mode = kart_lights_LightModeValue_ANIMATION;
        animationState.active = true;
        animationState.currentFrame = 0;
        animationState.lastFrameTime = millis();
        Serial.println("Animation playback started");
      }
      break;
      
    case kart_lights_AnimationCommandValue_ANIM_STOP:
      // Stop animation playback
      animationState.active = false;
      lightState.mode = kart_lights_LightModeValue_ON;
      clearLights(leds, NUM_LEDS);
      Serial.println("Animation stopped");
      break;
      
    case kart_lights_AnimationCommandValue_ANIM_PAUSE:
      // Pause animation
      animationState.active = false;
      Serial.println("Animation paused");
      break;
      
    case kart_lights_AnimationCommandValue_ANIM_RESET:
      // Reset animation state
      resetAnimationState();
      lightState.mode = kart_lights_LightModeValue_ON;
      Serial.println("Animation reset");
      break;
      
    case kart_lights_AnimationCommandValue_ANIM_NEXT_FRAME:
      // Advance to next frame
      if (animationState.frameCount > 0) {
        animationState.currentFrame = (animationState.currentFrame + 1) % animationState.frameCount;
        displayAnimationFrame(animationState.currentFrame);
        Serial.print("Showing frame: ");
        Serial.println(animationState.currentFrame);
      }
      break;
      
    case kart_lights_AnimationCommandValue_ANIM_PREV_FRAME:
      // Go to previous frame
      if (animationState.frameCount > 0) {
        if (animationState.currentFrame > 0) {
          animationState.currentFrame--;
        } else {
          animationState.currentFrame = animationState.frameCount - 1;
        }
        displayAnimationFrame(animationState.currentFrame);
        Serial.print("Showing frame: ");
        Serial.println(animationState.currentFrame);
      }
      break;
  }
}

void handleAnimationConfig(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id) {
  // Only process if this message is for us
  if (component_id != NODE_ID && component_id != kart_lights_LightComponentId_ALL) {
    return;
  }
  
  // Need at least type and value
  if (!data || length < 2) {
    return;
  }
  
  uint8_t* configData = (uint8_t*)data;
  uint8_t configType = configData[0];
  uint8_t configValue = configData[1];
  
  switch (configType) {
    case kart_lights_AnimationConfigValue_ANIM_CONFIG_FPS:
      // Set animation FPS
      if (configValue > 0 && configValue <= 60) {
        animationConfig.fps = configValue;
        animationConfig.frameDuration = 1000 / configValue;
        Serial.print("Animation FPS set to: ");
        Serial.println(configValue);
      }
      break;
      
    case kart_lights_AnimationConfigValue_ANIM_CONFIG_LOOP:
      // Set loop mode
      animationConfig.loopAnimation = (configValue > 0);
      Serial.print("Animation loop: ");
      Serial.println(animationConfig.loopAnimation ? "ON" : "OFF");
      break;
      
    case kart_lights_AnimationConfigValue_ANIM_CONFIG_BRIGHTNESS:
      // Set animation brightness
      if (configValue >= MIN_BRIGHTNESS && configValue <= MAX_BRIGHTNESS) {
        animationConfig.brightness = configValue;
        Serial.print("Animation brightness: ");
        Serial.println(configValue);
      }
      break;
      
    case kart_lights_AnimationConfigValue_ANIM_CONFIG_LED_COUNT:
      // Set LED count (for animations that need to know the LED count)
      if (configValue > 0 && configValue <= NUM_LEDS) {
        // Currently not used directly, but could be useful for partial animations
        Serial.print("Animation LED count: ");
        Serial.println(configValue);
      }
      break;
  }
}

// Function for processing animation data
void processAnimationMessage(uint8_t component_id, uint8_t command_id, const uint8_t* data, size_t length, bool isLastChunk) {
  if (lightState.mode != kart_lights_LightModeValue_ANIMATION) {
    // Auto-switch to animation mode when receiving animation data
    lightState.mode = kart_lights_LightModeValue_ANIMATION;
    resetAnimationState(); // Reset animation state when switching modes
  }

  // Process the animation data
  if (data && length > 0) {
    // Calculate position where this chunk should be stored
    uint32_t position = animationState.dataSize;
    
    // Check if we can fit this chunk
    if (position + length <= MAX_ANIMATION_BUFFER_SIZE) {
      // Copy data to our buffer
      memcpy(animationState.animationData + position, data, length);
      animationState.dataSize += length;
      
      // If this is the last chunk, prepare animation for playback
      if (isLastChunk) {
        // Assuming RGB data format - 3 bytes per LED
        animationState.frameSize = NUM_LEDS * 3;
        animationState.frameCount = animationState.dataSize / animationState.frameSize;
        
        // Activate animation
        animationState.active = true;
        animationState.currentFrame = 0;
        animationState.lastFrameTime = millis();
        
        Serial.print("Animation loaded: ");
        Serial.print(animationState.frameCount);
        Serial.print(" frames, ");
        Serial.print(animationState.frameSize);
        Serial.println(" bytes per frame");
      }
    } else {
      Serial.println("Animation buffer overflow - discarding data");
    }
  }
}

#endif