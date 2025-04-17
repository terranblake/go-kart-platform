/*
 * BasicControllerTest.cpp
 * 
 * Simple test script for verifying hardware connectivity with the Kunray MY1020 3kW BLDC controller.
 * This script focuses on testing three core functions:
 * 1. Throttle control
 * 2. Direction control (forward/reverse)
 * 3. Three-speed mode selection
 * 
 * The test will sequence through different combinations of these settings with appropriate
 * delays to allow for observation and verification.
 * 
 * WIRING NOTES:
 * - Throttle connector has BLACK (GND), RED (5V from controller), GREEN (signal) wires
 * - Reverse connector has BLUE (signal), BLACK (GND) wires
 * - 3-Speed connector has WHITE (Speed 1), BLUE (Speed 2), BLACK (GND) wires
 * - Low Brake (YELLOW wire) - Must be DISCONNECTED for normal operation
 *   To control it from Arduino, use a relay/transistor to physically connect/disconnect it
 * - High Brake has a single ORANGE (signal) wire
 * - Hall sensors (usually 3 wires per sensor: VCC, GND, Signal)
 * - Common ground must be established between Arduino and controller
 * - DO NOT connect Arduino/ESP32 5V to throttle's RED wire - controller provides its own 5V
 * 
 * CAN WIRING NOTES:
 * - MCP2515 CS -> Arduino D10 / ESP32 GPIO5
 * - MCP2515 INT -> Arduino D3 / ESP32 GPIO15
 * - MCP2515 SCK -> Arduino D13 / ESP32 GPIO18
 * - MCP2515 MISO -> Arduino D12 / ESP32 GPIO19
 * - MCP2515 MOSI -> Arduino D11 / ESP32 GPIO23
 * - MCP2515 VCC -> 5V
 * - MCP2515 GND -> GND
 */

#include <Arduino.h>

// Add sensor framework includes
#include "../../sensors/src/Sensor.h"
#include "../../sensors/src/SensorRegistry.h"
#include "../../sensors/variants/TemperatureSensor.h"
#include "../../sensors/variants/RpmSensor.h"

// Explicitly include protocol headers
#include "/Users/terranblake/Documents/go-kart-platform/protocol/generated/nanopb/sensors.pb.h"
#include "/Users/terranblake/Documents/go-kart-platform/protocol/generated/nanopb/motors.pb.h"

// Define DEBUG_MODE for serial command testing
#ifndef DEBUG_MODE
#define DEBUG_MODE 1  // Set to 1 to enable debug features
#endif

// Add platform-specific defines at the top after the includes
#if defined(ESP8266) || defined(ESP32)
#define ICACHE_RAM_ATTR ICACHE_RAM_ATTR
#else
#define ICACHE_RAM_ATTR
#endif

// We'll use motor status values for status messages
// The protocol headers are already included by CrossPlatformCAN

// PIN DEFINITIONS - must match your actual wiring
#if defined(ESP32S_WVROOM)
// ESP32S WVROOM 38-pin variant (primary recommended option)
#define THROTTLE_PIN 13        // PWM output to GREEN wire of throttle connector
#define DIRECTION_PIN 12       // Signal to BLUE wire of reverse connector: HIGH = forward, LOW = reverse

// Speed mode pins - connect to separate wires on 3-speed connector
#define SPEED_MODE_1_PIN 14    // Connect to WHITE wire (Speed 1)
#define SPEED_MODE_2_PIN 27    // Connect to BLUE wire (Speed 2)

// Brake controls
#define LOW_BRAKE_PIN 25       // Controls transistor for Low Brake (YELLOW wire)
                              // HIGH = brake engaged, LOW = normal operation
#define HIGH_BRAKE_PIN 26      // High level brake signal (to ORANGE wire)
                              // LOW = normal operation, HIGH = brake engaged

// Hall sensor pins - connect to the controller's hall feedback
#define HALL_A_PIN 32          // Hall sensor A (with interrupt)
#define HALL_B_PIN 33          // Hall sensor B (with interrupt)
#define HALL_C_PIN 34          // Hall sensor C (with interrupt)

// Temperature sensor pins
#define TEMP_SENSOR_BATTERY 36  // ADC1_0
#define TEMP_SENSOR_CONTROLLER 39  // ADC1_3
#define TEMP_SENSOR_MOTOR 35   // ADC1_7

#elif defined(ESP8266)
// ESP8266 NodeMCU pin mapping
#define THROTTLE_PIN D1        // PWM output to GREEN wire of throttle connector (GPIO5)
#define DIRECTION_PIN D2       // Signal to BLUE wire of reverse connector: HIGH = forward, LOW = reverse (GPIO4)

// Speed mode pins - connect to separate wires on 3-speed connector
#define SPEED_MODE_1_PIN D5    // Connect to WHITE wire (Speed 1) (GPIO14)
#define SPEED_MODE_2_PIN D6    // Connect to BLUE wire (Speed 2) (GPIO12)

// Brake controls
#define LOW_BRAKE_PIN D0       // Controls transistor for Low Brake (YELLOW wire) (GPIO16)
                              // HIGH = brake engaged, LOW = normal operation
#define HIGH_BRAKE_PIN D7      // High level brake signal (to ORANGE wire) (GPIO13)
                              // LOW = normal operation, HIGH = brake engaged

// Hall sensor pins - moved away from boot pins
// Avoid using D3 (GPIO0) and D4 (GPIO2) as they're used during flashing
#define HALL_A_PIN D8          // Hall sensor A (GPIO15)
#define HALL_B_PIN D9          // Hall sensor B (GPIO3/RX)
#define HALL_C_PIN D10         // Hall sensor C (GPIO1/TX)

#elif defined(ESP32)
// Standard ESP32 pin mapping
#define THROTTLE_PIN 13        // PWM output to GREEN wire of throttle connector
#define DIRECTION_PIN 12       // Signal to BLUE wire of reverse connector: HIGH = forward, LOW = reverse

// Speed mode pins - connect to separate wires on 3-speed connector
#define SPEED_MODE_1_PIN 14    // Connect to WHITE wire (Speed 1)
#define SPEED_MODE_2_PIN 27    // Connect to BLUE wire (Speed 2)

// Brake controls
#define LOW_BRAKE_PIN 26       // Controls transistor for Low Brake (YELLOW wire)
                              // HIGH = brake engaged, LOW = normal operation
#define HIGH_BRAKE_PIN 25      // High level brake signal (to ORANGE wire)
                              // LOW = normal operation, HIGH = brake engaged

// Hall sensor pins
#define HALL_A_PIN 32          // Hall sensor A (with interrupt)
#define HALL_B_PIN 33          // Hall sensor B (with interrupt)
#define HALL_C_PIN 34          // Hall sensor C (with interrupt)

#else
// Arduino Nano pin mapping (original)
#define THROTTLE_PIN 5        // PWM output to GREEN wire of throttle connector
#define DIRECTION_PIN 6       // Signal to BLUE wire of reverse connector: HIGH = forward, LOW = reverse

// Speed mode pins - connect to separate wires on 3-speed connector
#define SPEED_MODE_1_PIN 8    // Connect to WHITE wire (Speed 1)
#define SPEED_MODE_2_PIN 9    // Connect to BLUE wire (Speed 2)

// Brake controls
#define LOW_BRAKE_PIN 7       // Controls relay/transistor for Low Brake (YELLOW wire)
                              // For transistor: HIGH = brake engaged, LOW = normal operation
                              // For relay: HIGH = normal operation, LOW = brake engaged
#define HIGH_BRAKE_PIN 10     // High level brake signal (to ORANGE wire)
                              // LOW = normal operation, HIGH = brake engaged

// Hall sensor pins - connect to the controller's hall feedback (if available)
// These are digital inputs that detect motor position
#define HALL_A_PIN 2          // Hall sensor A (interrupt pin)
#define HALL_B_PIN 3          // Hall sensor B (interrupt pin)
#define HALL_C_PIN 4          // Hall sensor C (reading only)

// Temperature sensor pins
#define TEMP_SENSOR_BATTERY A0
#define TEMP_SENSOR_CONTROLLER A1
#define TEMP_SENSOR_MOTOR A2
#endif

// Controller type - uncomment the one being used
#define USING_TRANSISTOR      // Using transistor for low brake control
//#define USING_RELAY         // Using relay for low brake control

// Test timing parameters
#define STEP_DELAY 3000       // Delay between test steps (ms)
#define THROTTLE_INCREMENT 25 // Throttle increment for speed tests (0-255)
#define MAX_THROTTLE 150      // Maximum throttle level for safety during testing
#define FULL_THROTTLE 250     // Full throttle value (maximum possible)
#define MIN_THROTTLE 75       // Minimum throttle value where motor actually responds
#define RPM_UPDATE_INTERVAL 500 // How often to update RPM calculation (ms)

// NTC Thermistor parameters for NTCLE100E3203JBD
#define THERMISTOR_NOMINAL 10000   // Resistance at 25°C
#define TEMPERATURE_NOMINAL 25     // Temperature for nominal resistance (°C)
#define B_COEFFICIENT 3977         // Beta coefficient from datasheet
#define SERIES_RESISTOR 10000      // Value of the series resistor

// Temperature safety thresholds
#define BATTERY_TEMP_WARNING 45.0
#define BATTERY_TEMP_CRITICAL 55.0
#define CONTROLLER_TEMP_WARNING 65.0
#define CONTROLLER_TEMP_CRITICAL 80.0
#define MOTOR_TEMP_WARNING 75.0
#define MOTOR_TEMP_CRITICAL 90.0

// Serial output settings
#define SERIAL_BAUD 115200
#define SERIAL_ENABLED true

// Global variables for hall sensor readings
volatile unsigned long hallPulseCount = 0;    // Counter for hall sensor pulses
volatile unsigned long lastHallTime = 0;      // Timestamp of last hall sensor trigger
unsigned long lastRpmUpdate = 0;              // Timestamp of last RPM calculation
unsigned int currentRpm = 0;                  // Current RPM value
byte hallState = 0;                           // Current hall sensor state (bit 0=A, 1=B, 2=C)
// Track previous hall sensor states for edge detection
byte prevHallA = 0;
byte prevHallB = 0;
byte prevHallC = 0;

// Global variables for temperature readings
float batteryTemp = 0.0;
float controllerTemp = 0.0;
float motorTemp = 0.0;
bool tempWarningActive = false;
bool tempCriticalActive = false;
unsigned long lastTempUpdate = 0;

// Global state variables for the test
uint8_t currentThrottle = 0;
bool currentDirection = true;  // true = forward, false = reverse
uint8_t currentSpeedMode = 0;  // 0 = OFF, 1 = LOW, 2 = HIGH
bool currentLowBrake = false;  // true = engaged, false = disengaged
bool currentHighBrake = false; // true = engaged, false = disengaged
unsigned long testStartTime = 0; // Time when test sequence started
unsigned long lastSensorPrintTime = 0; // Time of last sensor value print

// Global sensor objects
ProtobufCANInterface canInterface(NODE_ID);
SensorRegistry sensorRegistry(canInterface, kart_common_ComponentType_MOTORS, NODE_ID);
TemperatureSensor* batteryTempSensor;
TemperatureSensor* controllerTempSensor;
TemperatureSensor* motorTempSensor;
RpmSensor* motorRpmSensor;

// Function prototypes
void setupPins();
void setSpeedMode(uint8_t mode);
void setDirection(bool forward);
void setThrottle(uint8_t level);
void runTestSequence();
void printStatus(const char* action, uint8_t value);
void allStop();
void setLowBrake(bool engaged);
void setHighBrake(bool engaged);
void hallSensorA_ISR();
void hallSensorB_ISR();
void hallSensorC_ISR();
void updateHallReadings();
unsigned int calculateRPM();
void updateTemperatures();
void checkTemperatureSafety();
void testCombinedFunctions();
#if DEBUG_MODE == 1
void parseSerialCommands();  // Prototype for the serial command parser
#endif
// void printFreeMemory();

// CAN message handlers
void handleMotorCommand(
  kart_common_MessageType msg_type,
  kart_common_ComponentType comp_type,
  uint8_t component_id,
  uint8_t command_id,
  kart_common_ValueType value_type,
  int32_t value
) {
  // Only process COMMAND messages
  if (msg_type != kart_common_MessageType_COMMAND) {
    return;
  }
  
  // Handle commands based on command ID
  switch (command_id) {
    case kart_motors_MotorCommandId_SPEED:
      // Set throttle (value is 0-100, convert to 0-255)
      setThrottle(map(value, 0, 100, 0, FULL_THROTTLE));
      break;
      
    case kart_motors_MotorCommandId_DIRECTION:
      // Set direction (0=forward, 1=reverse)
      setDirection(value == kart_motors_MotorDirectionValue_FORWARD);
      break;
      
    case kart_motors_MotorCommandId_MODE:
      // Set speed mode (0=LOW, 1=MEDIUM, 2=HIGH)
      setSpeedMode(value);
      break;
      
    case kart_motors_MotorCommandId_BRAKE:
      // Handle brake commands
      if (value > 0) {
        setLowBrake(true);
      } else {
        setLowBrake(false);
      }
      break;
      
    case kart_motors_MotorCommandId_EMERGENCY:
      // Handle emergency commands
      if (value == kart_motors_MotorEmergencyValue_STOP) {
        allStop();
      }
      break;
  }
}

void setup() {
  // Set up indicator LED (usually pin 13 on Arduino Nano)
  pinMode(LED_BUILTIN, OUTPUT);
  
  if (SERIAL_ENABLED) {
    Serial.begin(SERIAL_BAUD);
    delay(1000); // Short delay for serial to connect
    
    Serial.println(F("Kunray MY1020 Basic Controller Test with CAN"));
    Serial.println(F("=============================================="));
  }

  // printFreeMemory();
  
  // Init hardware pins
  setupPins();
  
  // Initialize CAN interface
  if (SERIAL_ENABLED) {
    Serial.println(F("Initializing CAN interface..."));
  }
  
  // Using SPI for MCP2515 CAN controller
  if (!canInterface.begin(500000)) {
    if (SERIAL_ENABLED) {
      Serial.println(F("ERROR: Failed to initialize CAN interface!"));
      // Continue anyway so we can at least test the motor controller
    }
  } else if (SERIAL_ENABLED) {
    Serial.println(F("CAN interface initialized successfully"));
  }
  
  // Register CAN message handlers
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_MOTORS,
    NODE_ID, // Only handle messages for our node ID
    kart_motors_MotorCommandId_SPEED,
    handleMotorCommand
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_MOTORS,
    NODE_ID,
    kart_motors_MotorCommandId_DIRECTION,
    handleMotorCommand
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_MOTORS,
    NODE_ID,
    kart_motors_MotorCommandId_MODE,
    handleMotorCommand
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_MOTORS,
    NODE_ID,
    kart_motors_MotorCommandId_BRAKE,
    handleMotorCommand
  );
  
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_MOTORS,
    NODE_ID,
    kart_motors_MotorCommandId_EMERGENCY,
    handleMotorCommand
  );
  
  // Initialize temperature sensors with location IDs from TemperatureSensorLocation enum
  batteryTempSensor = new TemperatureSensor(
    2,                        // Location ID (BATTERY=2)
    TEMP_SENSOR_BATTERY,      // Pin
    2000,                     // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  controllerTempSensor = new TemperatureSensor(
    1,                        // Location ID (CONTROLLER=1)
    TEMP_SENSOR_CONTROLLER,   // Pin
    2000,                     // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  motorTempSensor = new TemperatureSensor(
    0,                        // Location ID (MOTOR=0)
    TEMP_SENSOR_MOTOR,        // Pin
    2000,                     // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  // // Initialize RPM sensor with location ID from RpmSensorLocation enum
  // todo: handle pulling this from eeprom
  motorRpmSensor = new RpmSensor(kart_motors_MotorComponentId_MOTOR_LEFT_REAR, 100);
  
  // Register all sensors with the registry
  sensorRegistry.registerSensor(batteryTempSensor);
  // sensorRegistry.registerSensor(controllerTempSensor);
  sensorRegistry.registerSensor(motorTempSensor);
  sensorRegistry.registerSensor(motorRpmSensor);

  // printFreeMemory();
  
  // Initialize temperatures
  updateTemperatures();

  
  if (SERIAL_ENABLED) {
    Serial.println(F("Beginning test sequence in 3 seconds..."));
  }
  delay(1000);
  
  // Initialize lastSensorPrintTime to ensure first print happens after 2 seconds
  lastSensorPrintTime = millis() - 2000; // This makes the first print happen immediately
  
  // Record test start time
  testStartTime = millis();
  
  // Broadcast initial status
  
  // Run the test sequence
  // runTestSequence();
}

void loop() {
  // Update hall sensor readings and calculate RPM
  updateHallReadings();
  
  // Process all sensors using the SensorRegistry
  // printFreeMemory();
  sensorRegistry.process();
  
  // Parse serial commands in DEBUG_MODE
#if DEBUG_MODE == 1
  parseSerialCommands();
#endif
  
  // Process any incoming CAN messages
  canInterface.process();
}

void setupPins() {
  // Set up control pins
  pinMode(THROTTLE_PIN, OUTPUT);
  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(SPEED_MODE_1_PIN, OUTPUT);
  pinMode(SPEED_MODE_2_PIN, OUTPUT);
  pinMode(LOW_BRAKE_PIN, OUTPUT);
  pinMode(HIGH_BRAKE_PIN, OUTPUT);
  
  // Set up hall sensor pins
  pinMode(HALL_A_PIN, INPUT_PULLUP);
  pinMode(HALL_B_PIN, INPUT_PULLUP);
  pinMode(HALL_C_PIN, INPUT_PULLUP);
  
  // Attach interrupts for hall sensors - use RISING edge only to reduce noise
  // and prevent overcounting (CHANGE detection counts each transition twice)
#if defined(ESP32S_WVROOM) || defined(ESP32)
  // For ESP32/ESP32S, we can use interrupts on all hall sensors
  attachInterrupt(HALL_A_PIN, hallSensorA_ISR, RISING);
  attachInterrupt(HALL_B_PIN, hallSensorB_ISR, RISING);
  attachInterrupt(HALL_C_PIN, hallSensorC_ISR, RISING);
#elif defined(ESP8266)
  // For ESP8266, we can use interrupts on all hall sensors
  attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), hallSensorA_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), hallSensorB_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_C_PIN), hallSensorC_ISR, RISING);
#else
  // For Arduino Nano, only pins 2 and 3 support interrupts
  attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), hallSensorA_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), hallSensorB_ISR, RISING);
  // Pin 4 (HALL_C_PIN) doesn't support interrupts, will be polled
#endif
  
  // Initialize to safe state
  digitalWrite(DIRECTION_PIN, HIGH);     // Forward
  digitalWrite(SPEED_MODE_1_PIN, LOW);   // Speed mode OFF
  digitalWrite(SPEED_MODE_2_PIN, LOW);   // Speed mode OFF
  
#ifdef USING_TRANSISTOR
  digitalWrite(LOW_BRAKE_PIN, LOW);      // Transistor off = brake disengaged
#else
  digitalWrite(LOW_BRAKE_PIN, HIGH);     // Relay open = brake disengaged
#endif
  
  digitalWrite(HIGH_BRAKE_PIN, LOW);     // High brake disengaged
  analogWrite(THROTTLE_PIN, 0);          // Zero throttle
}

void setSpeedMode(uint8_t mode) {
  currentSpeedMode = mode;
  
  char cmdBuffer[8];
  const char* modeStr = "";
  
  switch (mode) {
    case 0: // OFF
      digitalWrite(SPEED_MODE_1_PIN, LOW);
      digitalWrite(SPEED_MODE_2_PIN, LOW);
      modeStr = "OFF";
      break;
      
    case 1: // LOW speed
      digitalWrite(SPEED_MODE_1_PIN, HIGH);
      digitalWrite(SPEED_MODE_2_PIN, LOW);
      modeStr = "LOW";
      break;
      
    case 2: // HIGH speed
      digitalWrite(SPEED_MODE_1_PIN, LOW);
      digitalWrite(SPEED_MODE_2_PIN, HIGH);
      modeStr = "HIGH";
      break;
      
    default: // Default to OFF
      digitalWrite(SPEED_MODE_1_PIN, LOW);
      digitalWrite(SPEED_MODE_2_PIN, LOW);
      modeStr = "DEF";
      break;
  }
  
  // Format the command description with shorter text
  snprintf(cmdBuffer, 8, "SPD=%s", modeStr);
  printStatus(cmdBuffer, mode);
}

void setDirection(bool forward) {
  currentDirection = forward;
  digitalWrite(DIRECTION_PIN, forward ? HIGH : LOW);
}

void setThrottle(uint8_t level) {
  currentThrottle = level;
  analogWrite(THROTTLE_PIN, level);
  
  char cmdBuffer[8];
  snprintf(cmdBuffer, 8, "THR=%d", level);
  printStatus(cmdBuffer, level);
}

void setLowBrake(bool engaged) {
  currentLowBrake = engaged;
  
  #ifdef USING_TRANSISTOR
    // When using a transistor to control the yellow wire:
    // HIGH output = transistor ON = wire connected to GND = brake engaged
    // LOW output = transistor OFF = wire disconnected = normal operation
    digitalWrite(LOW_BRAKE_PIN, engaged ? HIGH : LOW);
  #else
    // When using a relay to control the yellow wire:
    // HIGH output = relay open = wire disconnected = normal operation
    // LOW output = relay closed = wire connected = brake engaged
    digitalWrite(LOW_BRAKE_PIN, engaged ? LOW : HIGH);
  #endif
  
  printStatus("LBrk", engaged ? 1 : 0);
}

void setHighBrake(bool engaged) {
  currentHighBrake = engaged;
  digitalWrite(HIGH_BRAKE_PIN, engaged ? HIGH : LOW);
  printStatus("HBrk", engaged ? 1 : 0);
}

void allStop() {
  // Emergency stop function
  analogWrite(THROTTLE_PIN, 0);
  currentThrottle = 0;
  setLowBrake(true);                // Engage low brake (if relay/transistor circuit is used)
  setHighBrake(false);              // Keep high brake disengaged
  printStatus("EMERGENCY STOP - All systems halted", 0);
  delay(1000);
}

void printStatus(const char* action, uint8_t value) {
  if (!SERIAL_ENABLED) return;
  
  // Simple human-readable format
  Serial.print(action);
  Serial.print(F(": "));
  Serial.print(value);
  Serial.print(F(" | RPM: "));
  Serial.println(currentRpm);
}

// Fix the ISR functions to not use ICACHE_RAM_ATTR attribute incorrectly
void hallSensorA_ISR() {
  hallPulseCount++;
  lastHallTime = millis();
  // Notify the RPM sensor of the pulse
  if (motorRpmSensor) {
    motorRpmSensor->incrementPulse();
  }
}

void hallSensorB_ISR() {
  hallPulseCount++;
  lastHallTime = millis();
  // Notify the RPM sensor of the pulse
  if (motorRpmSensor) {
    motorRpmSensor->incrementPulse();
  }
}

void hallSensorC_ISR() {
  hallPulseCount++;
  lastHallTime = millis();
  // Notify the RPM sensor of the pulse
  if (motorRpmSensor) {
    motorRpmSensor->incrementPulse();
  }
}

// Update all hall sensor readings and calculate RPM
void updateHallReadings() {
  // Read current hall sensor state (binary pattern of all 3 sensors)
  hallState = 0;
  if (digitalRead(HALL_A_PIN)) hallState |= 0b001;
  if (digitalRead(HALL_B_PIN)) hallState |= 0b010;
  if (digitalRead(HALL_C_PIN)) hallState |= 0b100;
  
  // Update RPM calculation periodically
  if (millis() - lastRpmUpdate > RPM_UPDATE_INTERVAL) {
    currentRpm = calculateRPM();
    lastRpmUpdate = millis();
  }
}

// Replace calculateRPM to use the RPM sensor directly
unsigned int calculateRPM() {
  return motorRpmSensor->getRPM();
}

// Run a condensed test sequence with fewer steps
void runTestSequence() {
  // Simplified test sequence with fewer steps to save memory
  // Combined throttle, speed mode, and direction tests
  testCombinedFunctions();
  
  // Final report
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.println(F("TEST COMPLETE"));
  }
}

// Combined test of key functions to save memory
void testCombinedFunctions() {
  // Test name
  if (SERIAL_ENABLED) {
    Serial.println(F("STARTING COMBINED TEST"));
  }

  // printFreeMemory();
  
  // Release brake to begin
  setLowBrake(false);  // Disengage low brake
  setDirection(true);  // Forward
  setSpeedMode(1);     // LOW speed
  delay(1000);
  
  // Basic throttle test in LOW mode
  setThrottle(MIN_THROTTLE);
  if (SERIAL_ENABLED) {
    Serial.print(F("Throttle LOW: "));
    Serial.println(MIN_THROTTLE);
  }
  delay(2000);
  
  setThrottle(MIN_THROTTLE + 25);
  if (SERIAL_ENABLED) {
    Serial.print(F("Throttle MED: "));
    Serial.println(MIN_THROTTLE + 25);
  }
  delay(2000);
  
  // Test HIGH mode
  setThrottle(0);
  delay(1000);
  setSpeedMode(2);
  setThrottle(MIN_THROTTLE + 25);
  if (SERIAL_ENABLED) {
    Serial.println(F("Testing HIGH mode"));
  }
  delay(2000);
  
  // Test reverse
  setThrottle(0);
  delay(1000);
  setDirection(false);
  setSpeedMode(1);
  setThrottle(MIN_THROTTLE);
  if (SERIAL_ENABLED) {
    Serial.println(F("Testing REVERSE"));
  }
  delay(2000);
  
  // Return to zero and forward direction
  setThrottle(0);
  delay(1000);
  setDirection(true);
  
  // All stop
  allStop();
}

// Replace updateTemperatures with the sensor-based approach
void updateTemperatures() {
  // Read temperatures directly from sensors through the registry
  Sensor* batterySensor = sensorRegistry.getSensor(kart_sensors_SensorCommandId_TEMPERATURE, 2);
  Sensor* controllerSensor = sensorRegistry.getSensor(kart_sensors_SensorCommandId_TEMPERATURE, 1);
  Sensor* motorSensor = sensorRegistry.getSensor(kart_sensors_SensorCommandId_TEMPERATURE, 0);
  
  if (batterySensor) {
    batteryTemp = static_cast<TemperatureSensor*>(batterySensor)->getTemperature();
  }
  
  if (controllerSensor) {
    controllerTemp = static_cast<TemperatureSensor*>(controllerSensor)->getTemperature();
  }
  
  if (motorSensor) {
    motorTemp = static_cast<TemperatureSensor*>(motorSensor)->getTemperature();
  }
}

// Add this to help debug memory issues
// void printFreeMemory() {
//   extern int __heap_start, *__brkval;
//   int v;
//   Serial.print(F("Free RAM: "));
//   Serial.println((int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval));
// }

// Add this function to parse and handle serial commands
#if DEBUG_MODE == 1
void parseSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    // Command format: "T:value" for throttle where value is 0-100
    if (command.startsWith("T:")) {
      int throttlePercent = command.substring(2).toInt();
      // Constrain to valid range
      throttlePercent = constrain(throttlePercent, 0, 100);
      
      // Convert percent to throttle value (0-255 range)
      uint8_t throttleValue = map(throttlePercent, 0, 100, 0, FULL_THROTTLE);
      
      // Apply the throttle
      setThrottle(throttleValue);
      
      // Print confirmation
      Serial.print(F("DEBUG: Throttle set to "));
      Serial.print(throttlePercent);
      Serial.print(F("% ("));
      Serial.print(throttleValue);
      Serial.println(F(")"));
    }
    // Command format: "D:F" for forward, "D:R" for reverse
    else if (command.startsWith("D:")) {
      char direction = command.charAt(2);
      if (direction == 'F' || direction == 'f') {
        setDirection(true);
        Serial.println(F("DEBUG: Direction set to FORWARD"));
      } 
      else if (direction == 'R' || direction == 'r') {
        setDirection(false);
        Serial.println(F("DEBUG: Direction set to REVERSE"));
      }
    }
    // Command format: "S:0" for OFF, "S:1" for LOW, "S:2" for HIGH
    else if (command.startsWith("S:")) {
      int speedMode = command.substring(2).toInt();
      if (speedMode >= 0 && speedMode <= 2) {
        setSpeedMode(speedMode);
        Serial.print(F("DEBUG: Speed mode set to "));
        Serial.println(speedMode);
      }
    }
    // Command format: "B:E" to engage brakes, "B:D" to disengage
    else if (command.startsWith("B:")) {
      char brakeCommand = command.charAt(2);
      if (brakeCommand == 'E' || brakeCommand == 'e') {
        setLowBrake(true);
        Serial.println(F("DEBUG: Brake ENGAGED"));
      }
      else if (brakeCommand == 'D' || brakeCommand == 'd') {
        setLowBrake(false);
        Serial.println(F("DEBUG: Brake DISENGAGED"));
      }
    }
    // Emergency stop
    else if (command == "STOP") {
      allStop();
      Serial.println(F("DEBUG: EMERGENCY STOP executed"));
    }
    // Help command
    else if (command == "HELP") {
      Serial.println(F("\n--- DEBUG COMMANDS ---"));
      Serial.println(F("T:value - Set throttle (0-100%)"));
      Serial.println(F("D:F     - Set direction FORWARD"));
      Serial.println(F("D:R     - Set direction REVERSE"));
      Serial.println(F("S:0     - Speed mode OFF"));
      Serial.println(F("S:1     - Speed mode LOW"));
      Serial.println(F("S:2     - Speed mode HIGH"));
      Serial.println(F("B:E     - Engage brake"));
      Serial.println(F("B:D     - Disengage brake"));
      Serial.println(F("STOP    - Emergency stop"));
      Serial.println(F("HELP    - Show this help"));
      Serial.println(F("---------------------\n"));
    }
    else {
      Serial.println(F("DEBUG: Unknown command. Type 'HELP' for commands."));
    }
  }
}
#endif 