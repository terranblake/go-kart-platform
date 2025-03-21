#include <Arduino.h>
#include <FastLED.h>
#include <CAN.h>

#include "kart_protocol.h"
#include "../include/Config.h"

// Global state variables
LightState lightState{};
bool testModeActive = false;
CRGB leds[NUM_LEDS];
bool updateFrontLights = true;  // Default to updating front lights
bool locationSelected = false;  // whether the primary controller has told this secondary which lights it controls

// Animation and turn signal timing variables
unsigned long lastTurnUpdate = 0;
unsigned long lastAnimationUpdate = 0;
bool turnSignalState = true;
unsigned long lastTurnToggle;
unsigned long lastSweepUpdate;

char serialCommandBuffer[SERIAL_COMMAND_BUFFER_SIZE];

void setup() {
  if (!CAN.begin(500E3)) {
    Serial.println("CAN initialization failed!");
    while (1)
      ;
  }
  Serial.println("CAN initialized successfully");

  // Initialize LED strips
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);

#if DEBUG_MODE

  // Initialize serial for debugging and commands
  Serial.begin(115200);

#endif

  // Initialize all light states to default
  lightState.mode = 0;  // Lights disabled by default
  clearLights(leds, NUM_LEDS);

  Serial.println(F("Go-Kart Lights"));
}

void loop() {
  // Check for serial commands

  // Process any CAN messages
  processCAN();

#if DEBUG_MODE

  if (testModeActive) {
    // Run the test sequence if in test mode
    runTestSequence();
  }

#endif
  
  if (!testModeActive) {
    // Normal operation mode - all control through CAN/Serial now

    // Update light states based on current settings
    updateLights(leds, NUM_LEDS, lightState);

    // Show the updated lights
    FastLED.show();
  } 

  // Small delay to prevent flickering
  delay(10);
}

void updateLights(CRGB* leds, int numLeds, LightState& lightState) {
  // Store previous mode to detect transitions
  static uint8_t previousStateMode = KART_LIGHT_MODE_OFF;

  // Check for state transitions that should trigger animations
  if (previousStateMode == KART_LIGHT_MODE_OFF && lightState.mode != KART_LIGHT_MODE_OFF) {
    // Transition from OFF to any ON mode - start forward animation
    lightState.animation = 1;
    lightState.sweepPosition = 0;
  } else if (previousStateMode != KART_LIGHT_MODE_OFF && lightState.mode == KART_LIGHT_MODE_OFF) {
    // Transition from any ON mode to OFF - start reverse animation
    lightState.animation = 2;                // 2 indicates reverse animation
    lightState.sweepPosition = numLeds / 2;  // Start from fully extended
  }

  // Update the previous mode for next time
  previousStateMode = lightState.mode;

  // Handle animation if it's active (takes precedence over normal display)
  if (lightState.animation > 0) {
    // Forward or reverse startup animation
    updateStartupShutdownAnimation(leds, numLeds, lightState);
    return;
  }

  // Only proceed with normal lighting if mode is not OFF
  if (lightState.mode == KART_LIGHT_MODE_OFF) {
    clearLights(leds, numLeds);
    return;
  }

  // Clear LEDs first to ensure clean state
  clearLights(leds, numLeds);

  // Calculate brightness based on mode and braking
  uint8_t brightness = DEFAULT_BRIGHTNESS;
  if (lightState.mode == KART_LIGHT_MODE_HIGH) {
    brightness = BRIGHT_BRIGHTNESS;
  }
  if (!updateFrontLights && lightState.braking) {
    brightness = BRAKE_BRIGHTNESS;
  }
  FastLED.setBrightness(brightness);

  // Standard lighting (for non-hazard modes) - main lights
  if (lightState.mode != KART_LIGHT_MODE_HAZARD) {
    CRGB baseColor = updateFrontLights ? CRGB::White : CRGB::Red;
    int mainStart = TURN_SIGNAL_COUNT;
    int mainEnd = numLeds - TURN_SIGNAL_COUNT;

    for (int i = mainStart; i < mainEnd; i++) {
      leds[i] = baseColor;
    }
  }

  // Handle turn signals
  updateTurnSignals(leds, numLeds, lightState);
}

void updateTurnSignals(CRGB* leds, int numLeds, LightState& lightState) {
  unsigned long currentMillis = millis();

  // Handle hazard mode by setting both turn signals
  if (lightState.mode == KART_LIGHT_MODE_HAZARD) {
    // First set the middle section lights (same as regular mode)
    CRGB baseColor = updateFrontLights ? CRGB::White : CRGB::Red;
    int mainStart = TURN_SIGNAL_COUNT;
    int mainEnd = numLeds - TURN_SIGNAL_COUNT;

    for (int i = mainStart; i < mainEnd; i++) {
      leds[i] = baseColor;
    }

    // In hazard mode, both turn signals blink together
    if (currentMillis - lastTurnToggle > TURN_SIGNAL_BLINK_RATE) {
      turnSignalState = !turnSignalState;
      lastTurnToggle = currentMillis;
    }

    if (turnSignalState) {
      // Left side (first TURN_SIGNAL_COUNT LEDs)
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++) {
        leds[i] = CRGB::Orange;
      }

      // Right side (last TURN_SIGNAL_COUNT LEDs)
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++) {
        leds[numLeds - 1 - i] = CRGB::Orange;
      }
    }
    return;
  }

  // Only process regular turn signals if there's a signal active
  if (!lightState.turnLeft && !lightState.turnRight) return;

  // Update blink state based on timing
  if (currentMillis - lastTurnToggle > TURN_SIGNAL_BLINK_RATE) {
    turnSignalState = !turnSignalState;
    lastTurnToggle = currentMillis;

    // Reset sweep position when starting a new blink cycle
    if (turnSignalState) {
      lightState.sweepPosition = 0;
      lastSweepUpdate = currentMillis;
    }
  }

  // Process turn signals only if they're in the ON phase of blinking
  if (turnSignalState) {
    // Update sweep position based on timing
    if (currentMillis - lastSweepUpdate > TURN_SIGNAL_SWEEP_STEP && lightState.sweepPosition < TURN_SIGNAL_COUNT) {
      lightState.sweepPosition++;
      lastSweepUpdate = currentMillis;
    }

    // Left turn signal - sweeping right to left
    if (lightState.turnLeft) {
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++) {
        if (i <= lightState.sweepPosition) {
          leds[TURN_SIGNAL_COUNT - 1 - i] = CRGB::Orange;
        }
      }
    }

    // Right turn signal - sweeping left to right
    if (lightState.turnRight) {
      for (int i = 0; i < TURN_SIGNAL_COUNT; i++) {
        if (i <= lightState.sweepPosition) {
          leds[numLeds - TURN_SIGNAL_COUNT + i] = CRGB::Orange;
        }
      }
    }
  }
}

void updateStartupShutdownAnimation(CRGB* leds, int numLeds, LightState& lightState) {
  // Determine the color based on front/rear
  CRGB color = updateFrontLights ? CRGB::White : CRGB::Red;

  // Get middle position for animation
  int middle = numLeds / 2;

  // Forward animation speed control
  unsigned long currentMillis = millis();
  static unsigned long lastStepTime = 0;

  if (currentMillis - lastStepTime < ANIMATION_STEP_DELAY) {
    return;
  }
  lastStepTime = currentMillis;

  // Clear all LEDs first to ensure clean animation
  clearLights(leds, numLeds);

  if (lightState.animation == 1) {
    // STARTUP ANIMATION
    // Fill from middle to current position in both directions
    for (int i = middle - lightState.sweepPosition; i <= middle + lightState.sweepPosition; i++) {
      if (i >= 0 && i < numLeds) {
        leds[i] = color;
      }
    }

    // Increment position with accelerating speed
    int speedMultiplier = 1 + (lightState.sweepPosition / 10);
    lightState.sweepPosition += speedMultiplier;

    // End animation when it reaches edges
    if (lightState.sweepPosition >= middle) {
      lightState.animation = 0;
      lightState.sweepPosition = 0;

      // Fill in the regular lighting pattern
      if (lightState.mode != KART_LIGHT_MODE_OFF) {
        if (updateFrontLights) {
          for (int i = TURN_SIGNAL_COUNT; i < numLeds - TURN_SIGNAL_COUNT; i++) {
            leds[i] = CRGB::White;
          }
        } else {
          for (int i = TURN_SIGNAL_COUNT; i < numLeds - TURN_SIGNAL_COUNT; i++) {
            leds[i] = CRGB::Red;
          }
        }
      }
    }
  } else if (lightState.animation == 2) {
    // SHUTDOWN ANIMATION
    // Ensure lightState.sweepPosition stays within valid range (signed int)
    if (lightState.sweepPosition > 200) {  // This catches overflow to large values
      lightState.sweepPosition = middle;
    }

    // Animation starts with full strip lit, then contracts to middle
    int currentWidth = lightState.sweepPosition;

    // Fill all LEDs from middle-width to middle+width
    for (int i = middle - currentWidth; i <= middle + currentWidth; i++) {
      if (i >= 0 && i < numLeds) {
        leds[i] = color;
      }
    }

    // Decrement position with accelerating speed
    int speedMultiplier = 1 + ((middle - lightState.sweepPosition) / 10);
    if (speedMultiplier <= 0) speedMultiplier = 1;  // Ensure positive multiplier

    // Fix for overflow - check bounds before subtraction
    if (lightState.sweepPosition <= speedMultiplier) {
      lightState.sweepPosition = 0;  // About to reach 0
    } else {
      lightState.sweepPosition -= speedMultiplier;
    }

    // End animation when it reaches the middle
    if (lightState.sweepPosition <= 0) {
      lightState.animation = 0;
      lightState.sweepPosition = 0;
      clearLights(leds, numLeds);
    }
  }

  // Force update the display
  FastLED.show();
}

void clearLights(CRGB* leds, int numLeds) {
  // Turn off all LEDs
  fill_solid(leds, numLeds, CRGB::Black);
}

// Process incoming CAN messages
void processCAN() {
  // Check for available message
  if (CAN.parsePacket()) {
    // Read message ID
    unsigned long id = CAN.packetId();

    // Read data
    uint8_t buf[8] = { 0 };
    uint8_t len = 0;

    while (CAN.available() && len < 8) {
      buf[len++] = CAN.read();
    }

    // Process the message
    processCANMessage(id, buf, len);
  }
}

// Process CAN message using the registry
void processCANMessage(uint32_t id, uint8_t* data, uint8_t length) {
  if (length < 3) return;  // Need at least header, component ID, and value ID

  uint8_t header = data[0];
  uint8_t componentType = header >> 4;
  uint8_t commandId = header & 0x0F;
  uint8_t componentId = data[1];
  uint8_t valueId = data[2];

  // Check if this message is for us (lights component)
  if (componentType != KART_TYPE_LIGHTS) {
    return;  // Silently ignore messages not intended for the lighting system
  }

  // Find and call the appropriate handler
  for (int i = 0; i < NUM_HANDLERS; i++) {
    if (commandHandlers[i].componentType == componentType && commandHandlers[i].commandId == commandId) {
      commandHandlers[i].handler(componentId, valueId, &data[3]);
      return;
    }
  }

  // Only log unknown commands if they're for our component type
  Serial.print(F("Unknown light command: cmd="));
  Serial.println(commandId);
}

// Command handler implementations
void handleLightMode(uint8_t componentId, uint8_t valueId, uint8_t* data) {
  switch (valueId) {
    case KART_LIGHT_MODE_OFF: lightState.mode = 0; break;
    case KART_LIGHT_MODE_LOW: lightState.mode = 1; break;
    case KART_LIGHT_MODE_HIGH: lightState.mode = 2; break;
    case KART_LIGHT_MODE_HAZARD: lightState.mode = 3; break;
  }
  Serial.print(F("Light mode: "));
  Serial.println(lightState.mode);
}

void handleLightSignal(uint8_t componentId, uint8_t valueId, uint8_t* data) {
  switch (valueId) {
    case KART_LIGHT_SIGNAL_OFF:
      lightState.turnLeft = 0;
      lightState.turnRight = 0;
      break;
    case KART_LIGHT_SIGNAL_LEFT:
      lightState.turnLeft = 1;
      lightState.turnRight = 0;
      break;
    case KART_LIGHT_SIGNAL_RIGHT:
      lightState.turnLeft = 0;
      lightState.turnRight = 1;
      break;
  }
  Serial.print(F("Turn signals: L="));
  Serial.print(lightState.turnLeft);
  Serial.print(F(" R="));
  Serial.println(lightState.turnRight);
}

void handleLightBrake(uint8_t componentId, uint8_t valueId, uint8_t* data) {
  lightState.braking = (valueId == KART_LIGHT_ON) ? 1 : 0;
  Serial.print(F("Brake: "));
  Serial.println(lightState.braking);
}

void handleLightTest(uint8_t componentId, uint8_t valueId, uint8_t* data) {
  testModeActive = (valueId == KART_LIGHT_ON);
  Serial.print(F("Test mode: "));
  Serial.println(testModeActive);
}

void handleLightLocation(uint8_t componentId, uint8_t valueId, uint8_t* data) {
  updateFrontLights = (valueId == 0x01);
  locationSelected = true;
  Serial.print(F("Light location: "));
  Serial.println(updateFrontLights ? F("Front") : F("Rear"));
}

#if DEBUG_MODE

void runTestSequence() {
  static unsigned long lastStateChange = 0;
  static uint8_t testState = 0;
  const uint8_t numTestStates = 12;
  const unsigned long testInterval = 3000;  // 3 seconds per state

  unsigned long currentMillis = millis();

  // Check if it's time to change state
  if (currentMillis - lastStateChange > testInterval) {
    lastStateChange = currentMillis;
    testState = (testState + 1) % numTestStates;

    // Reset all states
    updateFrontLights = true;
    lightState.mode = 0;
    lightState.turnLeft = 0;
    lightState.turnRight = 0;
    lightState.braking = 0;
    lightState.sweepPosition = 0;
    lightState.animation = 0;

    switch (testState) {
      case 0:
        // Startup front - lights low
        executeSerialCommand("CAN:100:17,1,1");  // Light mode low
        break;

      case 1:
        // Shutdown front
        executeSerialCommand("CAN:100:17,1,0");  // Light mode off
        break;

      case 2:
        // Low mode with left turn signal
        executeSerialCommand("CAN:100:17,1,1");  // Light mode low
        executeSerialCommand("CAN:100:18,1,1");  // Left turn on
        break;

      case 3:
        // Low mode with right turn signal
        executeSerialCommand("CAN:100:17,1,1");  // Light mode low
        executeSerialCommand("CAN:100:18,1,2");  // Right turn on
        break;

      case 4:
        // High mode with left turn signal
        executeSerialCommand("CAN:100:17,1,2");  // Light mode high
        executeSerialCommand("CAN:100:18,1,1");  // Left turn on
        break;

      case 5:
        // High mode with right turn signal
        executeSerialCommand("CAN:100:17,1,2");  // Light mode high
        executeSerialCommand("CAN:100:18,1,2");  // Right turn on
        break;

      case 6:
        // Hazard lights (both turn signals)
        executeSerialCommand("CAN:100:17,1,3");  // Light mode hazard
        break;

      case 7:
        // Set to rear lights and hazard
        executeSerialCommand("CAN:100:21,1,0");  // Set as rear
        executeSerialCommand("CAN:100:17,1,3");  // Light mode hazard
        break;

      case 8:
        // Brake lights test
        executeSerialCommand("CAN:100:17,1,1");  // Light mode low
        executeSerialCommand("CAN:100:19,1,1");  // Brake on
        break;

      case 9:
        // Brake lights with left turn signals
        executeSerialCommand("CAN:100:17,1,1");  // Light mode low
        executeSerialCommand("CAN:100:19,1,1");  // Brake on
        executeSerialCommand("CAN:100:18,1,1");  // Left turn on
        break;

      case 10:
        // Brake lights with right turn signals
        executeSerialCommand("CAN:100:17,1,1");  // Light mode low
        executeSerialCommand("CAN:100:19,1,1");  // Brake on
        executeSerialCommand("CAN:100:18,1,2");  // Right turn on
        break;

      case 11:
        // Shutdown rear animation
        executeSerialCommand("CAN:100:17,1,0");  // Light mode off
        break;
    }

    Serial.print(F("TS:"));
    Serial.println(testState);
  }

  updateLights(leds, NUM_LEDS, lightState);
  FastLED.show();
}

// Process commands received over Serial
void processSerialCommands() {
  static int bufferIndex = 0;

  // Check if data is available
  while (Serial.available() > 0) {
    char c = Serial.read();

    // Handle newline as command terminator
    if (c == '\n' || c == '\r') {
      if (bufferIndex > 0) {
        // Null-terminate the buffer
        serialCommandBuffer[bufferIndex] = '\0';

        // Process the command
        executeSerialCommand(serialCommandBuffer);

        // Reset buffer index
        bufferIndex = 0;
      }
    }
    // Add character to buffer if there's room
    else if (bufferIndex < SERIAL_COMMAND_BUFFER_SIZE - 1) {
      serialCommandBuffer[bufferIndex++] = c;
    }
  }
}

// Execute a serial command
void executeSerialCommand(const char* command) {
  // Process CAN message simulation
  if (strncmp(command, "CAN:", 4) == 0) {
    char* ptr = (char*)command + 4;

    // Parse CAN ID
    uint32_t id = strtoul(ptr, &ptr, 16);
    if (*ptr == ':') {
      ptr++;

      // Parse data bytes
      uint8_t data[8];
      uint8_t dataLen = 0;

      while (dataLen < 8 && *ptr) {
        // Skip any non-digit characters (like commas)
        while (*ptr && !isdigit(*ptr)) ptr++;

        if (*ptr) {
          // Parse the byte
          data[dataLen++] = strtoul(ptr, &ptr, 10);
        }
      }

      // Process the CAN message
      if (dataLen > 0) {
        Serial.print(F("CAN:"));
        Serial.print(id, HEX);
        Serial.print(F(" D="));
        Serial.println(data[0]);

        processCANMessage(id, data, dataLen);
      }
    }
  }
  // Simple ping command to test serial connectivity
  else if (strcmp(command, "PING") == 0) {
    Serial.println(F("PONG"));
  }
  else {
    Serial.println("Unsupported command");
  }
}

#endif