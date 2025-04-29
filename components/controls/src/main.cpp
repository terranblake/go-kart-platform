#include "../include/Config.h"

#include <Arduino.h>
#include <Wire.h> // May not be needed if no I2C devices
#include "ProtobufCANInterface.h"
#include "Sensor.h"
#include "SensorRegistry.h"

// Include Protobuf definitions
#include "common.pb.h"
#include "controls.pb.h"

// Include Sensor Variants
#include "MappedAnalogSensor.h"
#include "DigitalInputSensor.h"

// Include ADC Reader framework
#include "AnalogReader.h"
#include "InternalADCReader.h"

// Global CAN Interface
ProtobufCANInterface canInterface(NODE_ID, CAN_CS_PIN, CAN_INT_PIN);

// Global ADC Reader (for throttle/brake/steering)
InternalADCReader internalReader(ADC_VREF_MV);

// Global Sensor Registry
SensorRegistry sensorRegistry(canInterface, kart_common_ComponentType_CONTROLS, NODE_ID);

// --- Sensor Object Pointers ---

// Analog Inputs
MappedAnalogSensor* throttleSensor = nullptr;
MappedAnalogSensor* brakeSensor = nullptr; // Placeholder
// MappedAnalogSensor* steeringSensor = nullptr; // Placeholder if using analog steering

// Digital Inputs
DigitalInputSensor* turnLeftSensor = nullptr;
DigitalInputSensor* turnRightSensor = nullptr;
DigitalInputSensor* hazardSensor = nullptr;
DigitalInputSensor* lightsOffSensor = nullptr; // Example if using separate pins
DigitalInputSensor* lightsOnSensor = nullptr;
DigitalInputSensor* lightsLowBeamSensor = nullptr;
DigitalInputSensor* lightsHighBeamSensor = nullptr;

// Combined State Sensors (Alternative for complex switches)
// You might create a more complex Sensor variant that reads multiple pins 
// and reports a single state enum (e.g., TurnSignalStateValue)
// DigitalInputSensor* turnSignalSensor = nullptr; 
// DigitalInputSensor* lightsSwitchSensor = nullptr;

// Function Prototypes
void setupPins();
void setupSensors();
void parseSerialCommands();

void setup() {
#if DEBUG_MODE
  Serial.begin(115200);
  while (!Serial) { delay(10); } 
  Serial.println(F("Controls Component Initializing..."));
#endif

  // Initialize hardware pins
  setupPins(); 

  // Initialize Internal ADC Reader
  internalReader.begin();
#if DEBUG_MODE
    Serial.println(F("Internal ADC Reader initialized"));
#endif

  // Initialize CAN interface
  if (!canInterface.begin(CAN_BAUDRATE)) {
#if DEBUG_MODE
    Serial.println(F("Failed to initialize CAN interface"));
    // Consider halting or indicating error
#endif
  } else {
#if DEBUG_MODE
    Serial.println(F("CAN interface initialized"));
#endif
  }

  // Initialize and register Sensors
  setupSensors();

  // Register command handlers (if Controls component needs to receive commands)
  // Example: Register handler for CALIBRATE command
  // canInterface.registerHandler(kart_common_MessageType_COMMAND, 
  //                              kart_common_ComponentType_CONTROLS, 
  //                              kart_controls_ControlComponentId_SYSTEM, // Or specific component like THROTTLE
  //                              kart_controls_ControlCommandId_CALIBRATE, 
  //                              handleCalibrationCommand); // Need to implement handleCalibrationCommand

#if DEBUG_MODE
  Serial.println(F("Controls Component Setup Complete"));
#endif
}

void loop() {
  // Process incoming/outgoing CAN messages
  canInterface.process();

  // Process all registered sensors (reads values and sends CAN messages if needed)
  sensorRegistry.process();

  // Parse serial commands in DEBUG_MODE
#if DEBUG_MODE == 1
  parseSerialCommands();
#endif

  // Yield control briefly
  delay(1);
}

void setupPins() {
#if DEBUG_MODE
    Serial.println(F("Setting up GPIO pins..."));
#endif
    // Analog Inputs (ADC setup is handled by InternalADCReader::configureAttenuation)
    // Map GPIO pins to ADC1 channels (Check ESP32 datasheet/pinout for your specific board)
    // GPIO 34 is ADC1_CH6
    // GPIO 35 is ADC1_CH7
    // GPIO 32 is ADC1_CH4
    internalReader.configureAttenuation(ADC1_CHANNEL_6, THROTTLE_ATTEN); // Use enum for channel
    internalReader.configureAttenuation(ADC1_CHANNEL_7, BRAKE_ATTEN);    // Use enum for channel
    // internalReader.configureAttenuation(ADC1_CHANNEL_4, STEERING_ATTEN); // Use enum for channel


#if DEBUG_MODE
    Serial.println(F("GPIO pin setup complete (Digital pins handled by Sensor::begin())."));
#endif
}

void setupSensors() {
#if DEBUG_MODE
  Serial.println(F("Initializing and Registering Sensors..."));
#endif

  // --- Throttle Sensor ---
  throttleSensor = new MappedAnalogSensor(
    kart_common_ComponentType_CONTROLS,
    kart_controls_ControlComponentId_THROTTLE,
    kart_controls_ControlCommandId_POSITION,
    kart_common_ValueType_UINT16, // Output 0-1024 fits in UINT16
    &internalReader,
    THROTTLE_PIN,
    THROTTLE_UPDATE_INTERVAL,
    THROTTLE_MIN_ADC, // Raw min ADC from Config.h
    THROTTLE_MAX_ADC, // Raw max ADC from Config.h
    0,                // Output min value
    1024              // Output max value
  );
  sensorRegistry.registerSensor(throttleSensor);

  // --- Brake Sensor (Placeholder - uses same settings as throttle for now) ---
  //  brakeSensor = new MappedAnalogSensor(
  //   kart_common_ComponentType_CONTROLS,
  //   kart_controls_ControlComponentId_BRAKE,
  //   kart_controls_ControlCommandId_POSITION,
  //   kart_common_ValueType_UINT16, 
  //   &internalReader,
  //   BRAKE_PIN,
  //   BRAKE_UPDATE_INTERVAL,
  //   BRAKE_MIN_ADC, 
  //   BRAKE_MAX_ADC, 
  //   0,
  //   1024              
  // );
  // sensorRegistry.registerSensor(brakeSensor);

  // --- Steering Sensor (Placeholder - if using analog) ---
  // steeringSensor = new MappedAnalogSensor(...);
  // sensorRegistry.registerSensor(steeringSensor);

  // --- Digital Input Sensors ---
  // Note: These simple sensors report 0/1 based on pin state.
  // More complex logic might be needed to combine them into specific states (e.g., TurnSignalStateValue)
  // Using activeLow=true because pull-ups mean the pin is HIGH when switch is OFF, LOW when ON.

  turnLeftSensor = new DigitalInputSensor(
    kart_common_ComponentType_CONTROLS,
    kart_controls_ControlComponentId_TURN_SIGNAL_SWITCH, // Report under the switch ID
    kart_controls_ControlCommandId_STATE, // Use generic STATE command
    TURN_SIGNAL_LEFT_PIN,
    SWITCH_UPDATE_INTERVAL,
    INPUT_PULLUP,
    true // Active LOW
  );
  sensorRegistry.registerSensor(turnLeftSensor);
  // Value interpretation: 1 = Left Turn Active, 0 = Left Turn Inactive

  turnRightSensor = new DigitalInputSensor(
    kart_common_ComponentType_CONTROLS,
    kart_controls_ControlComponentId_TURN_SIGNAL_SWITCH, // Report under the switch ID
    kart_controls_ControlCommandId_STATE, // Use generic STATE command
    TURN_SIGNAL_RIGHT_PIN,
    SWITCH_UPDATE_INTERVAL,
    INPUT_PULLUP,
    true // Active LOW
  );
   sensorRegistry.registerSensor(turnRightSensor);
  // Value interpretation: 1 = Right Turn Active, 0 = Right Turn Inactive 
  // Note: Need logic elsewhere to combine left/right into TurnSignalStateValue (OFF, LEFT, RIGHT)

   hazardSensor = new DigitalInputSensor(
    kart_common_ComponentType_CONTROLS,
    kart_controls_ControlComponentId_HAZARD_SWITCH,
    kart_controls_ControlCommandId_STATE,
    HAZARD_SWITCH_PIN,
    SWITCH_UPDATE_INTERVAL,
    INPUT_PULLUP,
    true // Active LOW
  );
  sensorRegistry.registerSensor(hazardSensor);
  // Value interpretation: 1 = Hazard ON, 0 = Hazard OFF (Matches HazardStateValue)

  // Example for individual light switch pins - this is complex
  lightsOffSensor = new DigitalInputSensor(kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_LIGHTS_SWITCH, kart_controls_ControlCommandId_STATE, LIGHTS_MODE_OFF_PIN, SWITCH_UPDATE_INTERVAL, INPUT_PULLUP, true);
  lightsOnSensor = new DigitalInputSensor(kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_LIGHTS_SWITCH, kart_controls_ControlCommandId_STATE, LIGHTS_MODE_ON_PIN, SWITCH_UPDATE_INTERVAL, INPUT_PULLUP, true);
  lightsLowBeamSensor = new DigitalInputSensor(kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_LIGHTS_SWITCH, kart_controls_ControlCommandId_STATE, LIGHTS_MODE_LOW_PIN, SWITCH_UPDATE_INTERVAL, INPUT_PULLUP, true);
  lightsHighBeamSensor = new DigitalInputSensor(kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_LIGHTS_SWITCH, kart_controls_ControlCommandId_STATE, LIGHTS_MODE_HIGH_PIN, SWITCH_UPDATE_INTERVAL, INPUT_PULLUP, true);
  // sensorRegistry.registerSensor(lightsOffSensor);
  // sensorRegistry.registerSensor(lightsOnSensor);
  // sensorRegistry.registerSensor(lightsLowBeamSensor);
  // sensorRegistry.registerSensor(lightsHighBeamSensor);
  // Note: This setup sends separate messages for each pin. Logic elsewhere needed 
  // to determine the actual LightsSwitchModeValue based on which pin is active.
  // A better approach is a dedicated LightsSwitchSensor variant or reading an analog pin if it's a rotary switch.

#if DEBUG_MODE
  Serial.println(F("Sensors initialized and registered."));
#endif
}

#if DEBUG_MODE == 1
// Serial Command Parsing for Debugging Controls
void parseSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    String commandUpper = command;
    commandUpper.toUpperCase();

    if (commandUpper == "STATUS") {
      Serial.println(F("--- Controls Status ---"));
      // Manually trigger reads and print for debug
      if (throttleSensor) Serial.printf("Throttle: %d / 1024\n", throttleSensor->getValue());
      if (brakeSensor) Serial.printf("Brake: %d / 1024\n", brakeSensor->getValue());
      if (turnLeftSensor) Serial.printf("Turn Left Pin: %s\n", turnLeftSensor->getValue() ? "ACTIVE (LOW)" : "Inactive (HIGH)");
      if (turnRightSensor) Serial.printf("Turn Right Pin: %s\n", turnRightSensor->getValue() ? "ACTIVE (LOW)" : "Inactive (HIGH)");
      if (hazardSensor) Serial.printf("Hazard Pin: %s\n", hazardSensor->getValue() ? "ACTIVE (LOW)" : "Inactive (HIGH)");
      // Add printing for light switch pins if registered
      Serial.println(F("-----------------------"));
      Serial.println(F("ACK: STATUS command processed"));
    }
    else if (commandUpper == "HELP") {
      Serial.println(F("--- Controls Debug Commands ---"));
      Serial.println(F("STATUS - Show current sensor raw values."));
      Serial.println(F("HELP   - Show this help message."));
      Serial.println(F("--- Simulate CAN Messages (Sends STATUS update) ---"));
      Serial.println(F("T:[0-1024] - Simulate Throttle Position"));
      Serial.println(F("B:[0-1024] - Simulate Brake Position"));
      Serial.println(F("TS:[0|L|R] - Simulate Turn Signal (Off, Left, Right)"));
      Serial.println(F("HZ:[0|1]   - Simulate Hazard Switch (Off, On)"));
      Serial.println(F("L:[0-3]    - Simulate Lights Switch (Off, On, Low, High)"));
      Serial.println(F("------------------------------"));
      Serial.println(F("ACK: HELP command processed"));
    }
    // Simulate incoming CAN messages for testing handlers (if any) - Not implemented yet
    // Simulate outgoing status messages based on serial input
    else if (commandUpper.startsWith("T:")) {
      int val = command.substring(2).toInt();
      val = constrain(val, 0, 1024);
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_THROTTLE, kart_controls_ControlCommandId_POSITION, kart_common_ValueType_UINT16, val);
      Serial.printf("ACK: Sent Throttle STATUS %d\n", val);
    }
     else if (commandUpper.startsWith("B:")) {
      int val = command.substring(2).toInt();
      val = constrain(val, 0, 1024);
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_BRAKE, kart_controls_ControlCommandId_POSITION, kart_common_ValueType_UINT16, val);
      Serial.printf("ACK: Sent Brake STATUS %d\n", val);
    }
     else if (commandUpper.startsWith("TS:")) {
      char val = command.charAt(3);
      kart_controls_TurnSignalStateValue state = kart_controls_TurnSignalStateValue_TURN_SIGNAL_OFF;
      if (val == 'L' || val == 'l') state = kart_controls_TurnSignalStateValue_TURN_SIGNAL_LEFT;
      else if (val == 'R' || val == 'r') state = kart_controls_TurnSignalStateValue_TURN_SIGNAL_RIGHT;
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_TURN_SIGNAL_SWITCH, kart_controls_ControlCommandId_STATE, kart_common_ValueType_UINT8, state);
      Serial.printf("ACK: Sent Turn Signal STATUS %d\n", state);
    }
    else if (commandUpper.startsWith("HZ:")) {
      int val = command.substring(3).toInt();
      kart_controls_HazardStateValue state = (val == 1) ? kart_controls_HazardStateValue_HAZARD_ON : kart_controls_HazardStateValue_HAZARD_OFF;
       canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_HAZARD_SWITCH, kart_controls_ControlCommandId_STATE, kart_common_ValueType_UINT8, state);
       Serial.printf("ACK: Sent Hazard STATUS %d\n", state);
    }
    else if (commandUpper.startsWith("L:")) {
      int val = command.substring(2).toInt();
      kart_controls_LightsSwitchModeValue mode = kart_controls_LightsSwitchModeValue_LIGHTS_MODE_OFF;
      if (val == 1) mode = kart_controls_LightsSwitchModeValue_LIGHTS_MODE_ON;
      else if (val == 2) mode = kart_controls_LightsSwitchModeValue_LIGHTS_MODE_LOW;
      else if (val == 3) mode = kart_controls_LightsSwitchModeValue_LIGHTS_MODE_HIGH;
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_CONTROLS, kart_controls_ControlComponentId_LIGHTS_SWITCH, kart_controls_ControlCommandId_MODE, kart_common_ValueType_UINT8, mode);
      Serial.printf("ACK: Sent Lights Switch MODE %d\n", mode);
    }
    else if (command.length() > 0) {
      Serial.print(F("Unknown command: "));
      Serial.println(command);
      Serial.println(F("ACK: UNKNOWN command rejected"));
    }
  }
}
#endif // DEBUG_MODE
