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
 * - DO NOT connect Arduino 5V to throttle's RED wire - controller provides its own 5V
 */

#include <Arduino.h>

// Add sensor framework includes
#include "../../sensors/src/Sensor.h"
#include "../../sensors/src/SensorRegistry.h"
#include "../../sensors/src/SensorManager.h"
#include "../../sensors/variants/TemperatureSensor.h"
#include "../../sensors/variants/RpmSensor.h"

// PIN DEFINITIONS - must match your actual wiring
#if defined(ESP8266)
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
#endif

// Controller type - uncomment the one being used
#define USING_TRANSISTOR      // Using transistor for low brake control
//#define USING_RELAY         // Using relay for low brake control

// Test timing parameters
#define STEP_DELAY 3000       // Delay between test steps (ms)
#define THROTTLE_INCREMENT 25 // Throttle increment for speed tests (0-255)
#define MAX_THROTTLE 150      // Maximum throttle level for safety during testing
#define FULL_THROTTLE 200     // Full throttle value (maximum possible)
#define MIN_THROTTLE 75       // Minimum throttle value where motor actually responds
#define RPM_UPDATE_INTERVAL 500 // How often to update RPM calculation (ms)

// NTC Thermistor parameters for NTCLE100E3203JBD
#define TEMP_SENSOR_BATTERY A0
#define TEMP_SENSOR_CONTROLLER A1
#define TEMP_SENSOR_MOTOR A2
#define THERMISTOR_NOMINAL 10000   // Resistance at 25°C
#define TEMPERATURE_NOMINAL 25     // Temperature for nominal resistance (°C)
#define B_COEFFICIENT 3977         // Beta coefficient from datasheet
#define SERIES_RESISTOR 10000      // Value of the series resistor

// Temperature thresholds (°C)
#define TEMP_BATTERY_WARNING 45
#define TEMP_BATTERY_CRITICAL 55
#define TEMP_CONTROLLER_WARNING 65
#define TEMP_CONTROLLER_CRITICAL 80
#define TEMP_MOTOR_WARNING 75
#define TEMP_MOTOR_CRITICAL 90

// Data logging parameters
#define MAX_DATA_POINTS 0     // We'll stream data in real-time instead of storing
#define STREAM_DATA true      // Stream each data point as it's collected

// Serial output settings
#define SERIAL_BAUD 115200
#define SERIAL_ENABLED true
#define JSON_OUTPUT true   // Set to true for JSON output, false for human-readable

// Global variables for hall sensor readings
volatile unsigned long hallPulseCount = 0;    // Counter for hall sensor pulses
volatile unsigned long lastHallTime = 0;      // Timestamp of last hall sensor trigger
unsigned long lastRpmUpdate = 0;              // Timestamp of last RPM calculation
unsigned int currentRpm = 0;                  // Current RPM value
byte hallState = 0;                           // Current hall sensor state (bit 0=A, 1=B, 2=C)

// Global variables for temperature readings
float batteryTemp = 0.0;
float controllerTemp = 0.0;
float motorTemp = 0.0;
bool tempWarningActive = false;
bool tempCriticalActive = false;
unsigned long lastTempUpdate = 0;
#define TEMP_UPDATE_INTERVAL 1000  // Update temperatures every 1 second

// For calculating min/max/avg without storing all points
struct TestStats {
  unsigned int minRpm;
  unsigned int maxRpm;
  unsigned long totalRpm;
  unsigned int countNonZero;
};

// Optimized data storage structure - packed to save memory
struct TestDataPoint {
  unsigned int timestamp;     // Time since test start (ms/100 to save space - divide by 10 to get seconds)
  byte throttle;              // Current throttle value (0-255)
  unsigned int rpm;           // RPM reading
  byte flags;                 // Bit-packed flags: 
                              // bits 0-1: speed mode (0=OFF, 1=LOW, 2=HIGH)
                              // bit 2: direction (0=REV, 1=FWD)
                              // bit 3: low brake (0=OFF, 1=ON)
                              // bit 4: high brake (0=OFF, 1=ON)
                              // bits 5-7: hall pattern (0-7)
};

// No need to store data points in memory
TestStats currentTestStats;
String currentCommand = "";  // Current command being executed
String currentTest = "";     // Name of current test

// Global state variables for the test
uint8_t currentThrottle = 0;
bool currentDirection = true;  // true = forward, false = reverse
uint8_t currentSpeedMode = 0;  // 0 = OFF, 1 = LOW, 2 = HIGH
bool currentLowBrake = false;  // true = engaged, false = disengaged
bool currentHighBrake = false; // true = engaged, false = disengaged
unsigned long testStartTime = 0; // Time when test sequence started

// Mock CAN interface for testing
class MockCANInterface : public ProtobufCANInterface {
public:
  MockCANInterface(uint8_t nodeId) : ProtobufCANInterface(nodeId) {}
  
  bool begin(uint32_t baudrate = 500000) override { 
    return true; 
  }
  
  bool sendMessage(
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id,
    kart_common_ValueType value_type,
    int32_t value) override {
    
    // Print message to serial
    if (SERIAL_ENABLED && JSON_OUTPUT) {
      Serial.print(F("{\"can_msg\":{\"type\":"));
      Serial.print((int)msg_type);
      Serial.print(F(",\"comp_type\":"));
      Serial.print((int)comp_type);
      Serial.print(F(",\"comp_id\":"));
      Serial.print(component_id);
      Serial.print(F(",\"cmd_id\":"));
      Serial.print(command_id);
      Serial.print(F(",\"value_type\":"));
      Serial.print((int)value_type);
      Serial.print(F(",\"value\":"));
      Serial.print(value);
      Serial.println(F("}}"));
    }
    
    return true;
  }
  
  void process() override {
    // Nothing to do for mock interface
  }
};

// Global sensor objects
MockCANInterface canInterface(0x01);
SensorRegistry sensorRegistry(canInterface, kart_common_ComponentType_MOTORS, 0x01);
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
void recordDataPoint();
void updateTestStats(unsigned int rpm);
void printTestSummary();
void runTest1_ThrottleTest();
void runTest2_SpeedModeTest();
void runTest3_DirectionTest();
void runTest4_CombinedTest();
void runTest5_BrakeTest();
void clearTestData();
void resetTestStats();
void streamDataPoint();
void resetDevice();
void updateTemperatures();
void checkTemperatureSafety();

void setup() {
  // Reset test statistics
  resetTestStats();
  
  // Set up indicator LED (usually pin 13 on Arduino Nano)
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Blink LED to indicate startup
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(200);
    digitalWrite(LED_BUILTIN, LOW);
    delay(200);
  }
  
  if (SERIAL_ENABLED) {
    Serial.begin(SERIAL_BAUD);
    
    // Longer delay for serial to connect
    delay(5000);
    
    Serial.println(F("Kunray MY1020 Basic Controller Test with Sensor Framework"));
    Serial.println(F("============================================================="));
    Serial.println(F("This test will cycle through:"));
    Serial.println(F("1. Different throttle levels"));
    Serial.println(F("2. Forward and reverse directions"));
    Serial.println(F("3. Speed modes (OFF, LOW, HIGH)"));
    Serial.println();
    Serial.println(F("CAUTION: Motor will move during this test!"));
    Serial.println(F("IMPORTANT: For direct wiring, Low brake wire (YELLOW) must be DISCONNECTED"));
    Serial.println(F("           Alternatively, use a transistor/relay controlled by D7"));
    Serial.println(F("NOTE: The controller doesn't respond until throttle reaches ~75 (MIN_THROTTLE)"));
    Serial.println(F("Hall sensor data will be collected and all results shown at the end of the test"));
    Serial.println(F("Press any key to begin, or wait 10 seconds for auto-start"));
    
    // Wait for user input with a 10-second timeout
    unsigned long startWaitTime = millis();
    while (!Serial.available() && (millis() - startWaitTime < 10000)) {
      // Blink LED while waiting for input
      digitalWrite(LED_BUILTIN, (millis() / 500) % 2);
      delay(100);
    }
    
    // Clear any input received
    while (Serial.available()) {
      Serial.read();
    }
    
    // Turn off LED
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  // Init hardware pins
  setupPins();
  
  // Initialize the sensor manager
  SensorManager::initialize();
  
  // Initialize temperature sensors
  batteryTempSensor = new TemperatureSensor(
    1,                        // ID
    TEMP_SENSOR_BATTERY,      // Pin
    2000,                     // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  controllerTempSensor = new TemperatureSensor(
    2,                        // ID
    TEMP_SENSOR_CONTROLLER,   // Pin
    2000,                     // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  motorTempSensor = new TemperatureSensor(
    3,                        // ID
    TEMP_SENSOR_MOTOR,        // Pin
    2000,                     // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  // Initialize RPM sensor
  motorRpmSensor = new RpmSensor(4, 100); // ID 4, update every 100ms
  SensorManager::registerRpmSensor(motorRpmSensor);
  
  // Register all sensors with the registry
  sensorRegistry.registerSensor(batteryTempSensor);
  sensorRegistry.registerSensor(controllerTempSensor);
  sensorRegistry.registerSensor(motorTempSensor);
  sensorRegistry.registerSensor(motorRpmSensor);
  
  // Initialize temperatures
  updateTemperatures();
  if (SERIAL_ENABLED) {
    Serial.println(F("Temperature Sensor Readings:"));
    Serial.print(F("Battery: "));
    Serial.print(batteryTempSensor->getTemperature(), 1);
    Serial.print(F("°C, Controller: "));
    Serial.print(controllerTempSensor->getTemperature(), 1);
    Serial.print(F("°C, Motor: "));
    Serial.print(motorTempSensor->getTemperature(), 1);
    Serial.println(F("°C"));
  }
  
  // Start with everything stopped/safe
  allStop();
  
  if (SERIAL_ENABLED) {
    Serial.println(F("Beginning test sequence in 3 seconds..."));
    Serial.println(F("Throttle | RPM | Hall State | Temperature"));
    Serial.println();
  }
  delay(3000);
  
  // Record test start time
  testStartTime = millis();
  
  // Run the test sequence
  runTestSequence();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Process serial commands
  if (SERIAL_ENABLED && Serial.available()) {
    char c = Serial.read();
    
    if (c == 's' || c == 'S') {
      // Stop
      allStop();
      Serial.println(F("EMERGENCY STOP"));
    } else if (c == 'r' || c == 'R') {
      // Restart
      Serial.println(F("Restarting test..."));
      delay(1000);
      resetDevice();
    } else if (c == 'j' || c == 'J') {
      // Output JSON format regardless of current setting
      JSON_OUTPUT = true;
      Serial.println(F("{\"status\":\"JSON output enabled\"}"));
    } else if (c == 't' || c == 'T') {
      // Output text format
      JSON_OUTPUT = false;
      Serial.println(F("Text output enabled"));
    }
  }
  
  // Update hall sensor readings and calculate RPM
  updateHallReadings();
  
  // Process all sensors
  sensorRegistry.process();
  
  // Check temperature safety periodically
  if (currentMillis - lastTempUpdate > TEMP_UPDATE_INTERVAL) {
    checkTemperatureSafety();
    lastTempUpdate = currentMillis;
  }
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
  attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), hallSensorA_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), hallSensorB_ISR, RISING);
  
  // For ESP8266, we can use interrupts on all hall sensors
  // For Arduino Nano, pin 4 doesn't support interrupts
#if defined(ESP8266)
  attachInterrupt(digitalPinToInterrupt(HALL_C_PIN), hallSensorC_ISR, RISING);
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
  
  if (forward) {
    digitalWrite(DIRECTION_PIN, HIGH);
    printStatus("DIR=FWD", 1);
  } else {
    digitalWrite(DIRECTION_PIN, LOW);
    printStatus("DIR=REV", 0);
  }
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
  
  if (JSON_OUTPUT) {
    // Output in JSON format for data logging
    Serial.print(F("{\"action\":\""));
    Serial.print(action);
    Serial.print(F("\",\"value\":"));
    Serial.print(value);
    Serial.print(F(",\"rpm\":"));
    Serial.print(currentRpm);
    Serial.print(F(",\"hall\":"));
    Serial.print(hallState);
    Serial.print(F(",\"temps\":{\"bat\":"));
    Serial.print(batteryTemp);
    Serial.print(F(",\"ctrl\":"));
    Serial.print(controllerTemp);
    Serial.print(F(",\"mot\":"));
    Serial.print(motorTemp);
    Serial.println(F("}}"));
  } else {
    // Human-readable format
    Serial.print(action);
    Serial.print(F(": "));
    Serial.print(value);
    Serial.print(F(" | RPM: "));
    Serial.print(currentRpm);
    Serial.print(F(" | Hall: "));
    Serial.print(hallState);
    Serial.print(F(" | Temp(°C): Bat="));
    Serial.print(batteryTemp, 1);
    Serial.print(F(" Ctrl="));
    Serial.print(controllerTemp, 1);
    Serial.print(F(" Mot="));
    Serial.print(motorTemp, 1);
    Serial.println();
  }
  
  // Store the command for the next data point
  currentCommand = action;
}

// Update the hall sensor ISR to use our SensorManager
void ICACHE_RAM_ATTR hallSensorA_ISR() {
  SensorManager::hallSensorInterrupt();
  
  // Update hall state for display purposes
  hallState = (hallState & 0b110) | (digitalRead(HALL_A_PIN) ? 0b001 : 0);
}

void ICACHE_RAM_ATTR hallSensorB_ISR() {
  SensorManager::hallSensorInterrupt();
  
  // Update hall state for display purposes
  hallState = (hallState & 0b101) | (digitalRead(HALL_B_PIN) ? 0b010 : 0);
}

void ICACHE_RAM_ATTR hallSensorC_ISR() {
  SensorManager::hallSensorInterrupt();
  
  // Update hall state for display purposes
  hallState = (hallState & 0b011) | (digitalRead(HALL_C_PIN) ? 0b100 : 0);
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

// Replace calculateRPM with the sensor-based approach
unsigned int calculateRPM() {
  return motorRpmSensor->getRPM();
}

void resetTestStats() {
  // Initialize statistics for the current test
  currentTestStats.minRpm = 65535;
  currentTestStats.maxRpm = 0;
  currentTestStats.totalRpm = 0;
  currentTestStats.countNonZero = 0;
}

// Update test statistics with new RPM reading
void updateTestStats(unsigned int rpm) {
  if (rpm > 0) {
    currentTestStats.countNonZero++;
    currentTestStats.totalRpm += rpm;
    
    if (rpm < currentTestStats.minRpm) 
      currentTestStats.minRpm = rpm;
      
    if (rpm > currentTestStats.maxRpm) 
      currentTestStats.maxRpm = rpm;
  }
}

// Stream a single data point in JSON format
void streamDataPoint() {
  if (!SERIAL_ENABLED || !STREAM_DATA) return;
  
  // Update statistics continuously
  updateTestStats(currentRpm);
  
  // Create a data point structure locally
  TestDataPoint dp;
  dp.timestamp = (millis() - testStartTime) / 100;
  dp.throttle = currentThrottle;
  dp.rpm = currentRpm;
  
  // Pack flags into a single byte
  byte flags = 0;
  flags |= (currentSpeedMode & 0x03);           // Bits 0-1: Speed mode
  flags |= (currentDirection ? 0x04 : 0);       // Bit 2: Direction
  flags |= (currentLowBrake ? 0x08 : 0);        // Bit 3: Low brake
  flags |= (currentHighBrake ? 0x10 : 0);       // Bit 4: High brake
  flags |= ((hallState & 0x07) << 5);           // Bits 5-7: Hall pattern
  dp.flags = flags;
  
  // Unpack flags for JSON output
  byte speedMode = dp.flags & 0x03;
  bool direction = dp.flags & 0x04;
  bool lowBrake = dp.flags & 0x08;
  bool highBrake = dp.flags & 0x10;
  byte hallState = (dp.flags >> 5) & 0x07;
  
  // Convert speed mode to string
  const char* speedModeStr;
  switch (speedMode) {
    case 0: speedModeStr = "OFF"; break;
    case 1: speedModeStr = "LOW"; break;
    case 2: speedModeStr = "HIGH"; break;
    default: speedModeStr = "UNKNOWN"; break;
  }
  
  // Output JSON data point immediately
  if (JSON_OUTPUT) {
    Serial.print(F("{\"test\": \""));
    Serial.print(currentTest);
    Serial.print(F("\", \"time\": "));
    Serial.print(dp.timestamp / 10.0);
    Serial.print(F(", \"throttle\": "));
    Serial.print(dp.throttle);
    Serial.print(F(", \"speed_mode\": \""));
    Serial.print(speedModeStr);
    Serial.print(F("\", \"direction\": \""));
    Serial.print(direction ? F("FWD") : F("REV"));
    Serial.print(F("\", \"low_brake\": "));
    Serial.print(lowBrake ? F("true") : F("false"));
    Serial.print(F(", \"high_brake\": "));
    Serial.print(highBrake ? F("true") : F("false"));
    Serial.print(F(", \"rpm\": "));
    Serial.print(dp.rpm);
    Serial.print(F(", \"hall_pattern\": "));
    Serial.print(hallState);
    
    // Add command if available
    if (currentCommand.length() > 0) {
      Serial.print(F(", \"command\": \""));
      Serial.print(currentCommand);
      Serial.print(F("\""));
      currentCommand = ""; // Clear the command after using it
    }
    
    Serial.println(F("}"));
  }
  else {
    // Human-readable format
    Serial.print(dp.timestamp / 10.0);
    Serial.print(F("s | "));
    Serial.print(currentTest);
    Serial.print(F(" | Thr:"));
    Serial.print(dp.throttle);
    Serial.print(F(" | Mode:"));
    Serial.print(speedModeStr);
    Serial.print(F(" | Dir:"));
    Serial.print(direction ? F("FWD") : F("REV"));
    Serial.print(F(" | Brk:"));
    Serial.print(lowBrake ? F("L") : F(""));
    Serial.print(highBrake ? F("H") : F(""));
    Serial.print(F(" | RPM:"));
    Serial.print(dp.rpm);
    
    // Add command if available
    if (currentCommand.length() > 0) {
      Serial.print(F(" | Cmd:"));
      Serial.print(currentCommand);
      currentCommand = ""; // Clear the command after using it
    }
    
    Serial.println();
  }
}

// Replace recordDataPoint with streamDataPoint
void recordDataPoint() {
  streamDataPoint();
}

void runTestSequence() {
  runTest1_ThrottleTest();
  runTest2_SpeedModeTest();
  runTest3_DirectionTest();
  runTest4_CombinedTest();
  runTest5_BrakeTest();
  
  // Final report
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.println(F("TEST COMPLETE - All systems returned to safe state"));
    Serial.print(F("Final RPM reading: "));
    Serial.println(currentRpm);
    Serial.println(F("Test complete."));
    Serial.println(F("Press 'r' to reset and run again"));
  }
}

void clearTestData() {
  // Reset test statistics
  resetTestStats();
  // Reset current command and test name
  currentCommand = "";
}

void runTest1_ThrottleTest() {
  // Clear any previous test data
  clearTestData();
  
  // Set current test name
  currentTest = F("Throttle Test");
  
  // PART 1: Test throttle in forward direction, low speed mode
  printStatus("STARTING TEST SEQUENCE - PART 1: Throttle test (forward, LOW speed)", 0);
  
  // Release brake to begin
  setLowBrake(false);  // Disengage low brake (if relay circuit is used)
  setDirection(true);  // Forward
  setSpeedMode(1);     // LOW speed (not OFF)
  delay(1000);
  recordDataPoint();  // Record initial data point
  
  // First apply the minimum throttle to get the motor moving
  printStatus("Applying MIN_THROTTLE to start motor", MIN_THROTTLE);
  setThrottle(MIN_THROTTLE);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  // Display RPM info
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("At MIN_THROTTLE ("));
    Serial.print(MIN_THROTTLE);
    Serial.print(F("), RPM = "));
    Serial.print(currentRpm);
    Serial.print(F(", Hall = "));
    Serial.println(hallState, BIN);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  // Then gradually increase throttle
  for (uint8_t throttle = MIN_THROTTLE; throttle <= FULL_THROTTLE; throttle += THROTTLE_INCREMENT) {
    // Prevent overflow causing infinite loop when uint8_t wraps around
    if (throttle > FULL_THROTTLE || throttle < MIN_THROTTLE) {
      break;
    }

    setThrottle(throttle);
    delay(STEP_DELAY/2);
    recordDataPoint();
    
    // Display RPM halfway through the step
    updateHallReadings();
    if (SERIAL_ENABLED) {
      Serial.print(F("RPM update - Throttle: "));
      Serial.print(throttle);
      Serial.print(F(", RPM: "));
      Serial.println(currentRpm);
    }
    delay(STEP_DELAY/2);
    recordDataPoint();
    
    // For higher throttle values, wait a bit longer for motor stabilization
    if (throttle > MAX_THROTTLE) {
      delay(STEP_DELAY/2);
      recordDataPoint();
    }
  }
  
  // Make sure we test at exactly FULL_THROTTLE if the increments didn't hit it
  if ((MIN_THROTTLE + ((FULL_THROTTLE - MIN_THROTTLE) / THROTTLE_INCREMENT) * THROTTLE_INCREMENT) < FULL_THROTTLE) {
    setThrottle(FULL_THROTTLE);
    delay(STEP_DELAY/2);
    recordDataPoint();
    updateHallReadings();
    if (SERIAL_ENABLED) {
      Serial.print(F("FINAL RPM update - Throttle: "));
      Serial.print(FULL_THROTTLE);
      Serial.print(F(", RPM: "));
      Serial.println(currentRpm);
    }
    delay(STEP_DELAY/2);
    recordDataPoint();
  }
  
  // Return to zero
  setThrottle(0);
  delay(STEP_DELAY);
  recordDataPoint();
  
  // Print results for this test immediately
  if (SERIAL_ENABLED) {
    Serial.println(F("Outputting results for Test 1: Throttle Test"));
    if (JSON_OUTPUT) {
      Serial.println(F("{"));
      printTestSummary();
      Serial.println(F("}"));
    } else {
      printTestSummary();
    }
  }
}

void runTest2_SpeedModeTest() {
  clearTestData();
  currentTest = F("Speed Mode Test (LOW)");
  
  // PART 2: Test speed modes in forward direction
  printStatus("STARTING TEST SEQUENCE - PART 2: Speed mode test (forward)", 0);
  
  // Test each speed mode at MIN_THROTTLE and higher level
  uint8_t testThrottle = MIN_THROTTLE + 25;  // Higher than MIN for stability in HIGH mode
  
  // Test LOW speed mode
  setSpeedMode(1);
  delay(1000);
  recordDataPoint();
  
  setThrottle(MIN_THROTTLE);
  printStatus("Testing LOW speed mode at MIN_THROTTLE", MIN_THROTTLE);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("LOW mode, MIN_THROTTLE ("));
    Serial.print(MIN_THROTTLE);
    Serial.print(F("), RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  setThrottle(testThrottle);
  printStatus("Testing LOW speed mode at higher throttle", testThrottle);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("LOW mode, Higher throttle ("));
    Serial.print(testThrottle);
    Serial.print(F("), RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  setThrottle(0);
  delay(STEP_DELAY);
  recordDataPoint();
  
  // Print results for LOW speed mode
  if (SERIAL_ENABLED) {
    printTestSummary();
  }
  
  // Clear data for next part
  clearTestData();
  currentTest = F("Speed Mode Test (HIGH)");
  
  // Test HIGH speed mode
  setSpeedMode(2);
  delay(1000);
  recordDataPoint();
  
  setThrottle(MIN_THROTTLE);
  printStatus("Testing HIGH speed mode at MIN_THROTTLE (may be unstable)", MIN_THROTTLE);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("HIGH mode, MIN_THROTTLE ("));
    Serial.print(MIN_THROTTLE);
    Serial.print(F("), RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  setThrottle(testThrottle);
  printStatus("Testing HIGH speed mode at higher throttle", testThrottle);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("HIGH mode, Higher throttle ("));
    Serial.print(testThrottle);
    Serial.print(F("), RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  setThrottle(0);
  delay(STEP_DELAY);
  recordDataPoint();
  
  // Print results for HIGH speed mode
  if (SERIAL_ENABLED) {
    printTestSummary();
  }
}

void runTest3_DirectionTest() {
  clearTestData();
  currentTest = F("Direction Test");
  
  // PART 3: Test direction change (requires complete stop first)
  printStatus("STARTING TEST SEQUENCE - PART 3: Direction test", 0);
  
  // Make sure we're stopped
  setThrottle(0);
  delay(STEP_DELAY);
  recordDataPoint();
  
  // Test reverse at low speed
  setSpeedMode(1);  // LOW speed mode for safety
  setDirection(false);  // Set reverse
  delay(1000);
  recordDataPoint();
  
  // First apply the minimum throttle to get the motor moving in reverse
  printStatus("Applying MIN_THROTTLE to start reverse motion", MIN_THROTTLE);
  setThrottle(MIN_THROTTLE);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("REVERSE mode, MIN_THROTTLE ("));
    Serial.print(MIN_THROTTLE);
    Serial.print(F("), RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  // Apply throttle gradually up to full throttle in reverse
  for (uint8_t throttle = MIN_THROTTLE; throttle <= FULL_THROTTLE; throttle += THROTTLE_INCREMENT) {
    // Prevent overflow causing infinite loop when uint8_t wraps around
    if (throttle > FULL_THROTTLE || throttle < MIN_THROTTLE) {
      break;
    }

    setThrottle(throttle);
    delay(STEP_DELAY/2);
    recordDataPoint();
    
    updateHallReadings();
    if (SERIAL_ENABLED) {
      Serial.print(F("REVERSE update - Throttle: "));
      Serial.print(throttle);
      Serial.print(F(", RPM: "));
      Serial.println(currentRpm);
    }
    delay(STEP_DELAY/2);
    recordDataPoint();
    
    // For higher throttle values, wait a bit longer for motor stabilization
    if (throttle > MAX_THROTTLE) {
      delay(STEP_DELAY/2);
      recordDataPoint();
    }
  }
  
  // Return to zero and back to forward direction
  setThrottle(0);
  delay(STEP_DELAY);
  setDirection(true);
  recordDataPoint();
  
  // Print results
  if (SERIAL_ENABLED) {
    printTestSummary();
  }
}

void runTest4_CombinedTest() {
  clearTestData();
  currentTest = F("Combined Test");
  
  // PART 4: Combined test of all systems
  printStatus("STARTING TEST SEQUENCE - PART 4: Combined functionality test", 0);
  
  // Cycle through different modes with varying throttle
  for (uint8_t mode = 1; mode <= 2; mode++) {  // Skip mode 0 (OFF)
    // Set direction
    setDirection(mode % 2 == 0);  // Alternate forward/reverse
    
    // Set speed mode
    setSpeedMode(mode);
    delay(1000);
    recordDataPoint();
    
    // Set throttle to appropriate level (higher than MIN_THROTTLE)
    uint8_t modeThrottle = MIN_THROTTLE + (10 * mode);  // Different levels above MIN_THROTTLE
    setThrottle(modeThrottle);
    delay(STEP_DELAY/2);
    recordDataPoint();
    
    updateHallReadings();
    if (SERIAL_ENABLED) {
      Serial.print(F("COMBINED Mode "));
      Serial.print(mode);
      Serial.print(F(", Dir "));
      Serial.print(currentDirection ? F("FWD") : F("REV"));
      Serial.print(F(", Throttle "));
      Serial.print(modeThrottle);
      Serial.print(F(", RPM = "));
      Serial.println(currentRpm);
    }
    delay(STEP_DELAY/2);
    recordDataPoint();
    
    // Return to zero before next iteration
    setThrottle(0);
    delay(STEP_DELAY);
    recordDataPoint();
  }
  
  // Add a full throttle test
  setDirection(true);  // Forward
  setSpeedMode(2);     // HIGH speed mode
  delay(1000);
  recordDataPoint();
  
  // Test full throttle in HIGH speed mode
  printStatus("Testing FULL THROTTLE in HIGH speed mode", FULL_THROTTLE);
  setThrottle(FULL_THROTTLE);    // Full throttle
  delay(STEP_DELAY);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("HIGH mode, FULL THROTTLE, RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY);
  recordDataPoint();
  
  // Return to zero throttle
  setThrottle(0);
  delay(STEP_DELAY);
  recordDataPoint();
  
  // Print results
  if (SERIAL_ENABLED) {
    printTestSummary();
  }
}

void runTest5_BrakeTest() {
  clearTestData();
  currentTest = F("Brake Test");
  
  // PART 5: Test brakes
  printStatus("STARTING TEST SEQUENCE - PART 5: Testing brakes", 0);
  setDirection(true);  // Forward
  setSpeedMode(1);     // LOW speed
  recordDataPoint();
  
  // Apply enough throttle to get the motor moving
  setThrottle(MIN_THROTTLE + 10);
  printStatus("Motor running at throttle", MIN_THROTTLE + 10);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("Before brake test, RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  // Test low brake
  setLowBrake(true);
  printStatus("Engaging LOW BRAKE", 1);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("With LOW BRAKE engaged, RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  setLowBrake(false);
  printStatus("Disengaging LOW BRAKE", 0);
  delay(STEP_DELAY / 2);  // Short delay to let motor restart
  recordDataPoint();
  
  // Apply throttle again
  setThrottle(MIN_THROTTLE + 10);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("After LOW BRAKE release, RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  // Test high brake
  setHighBrake(true);
  printStatus("Engaging HIGH BRAKE", 1);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("With HIGH BRAKE engaged, RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  setHighBrake(false);
  printStatus("Disengaging HIGH BRAKE", 0);
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  updateHallReadings();
  if (SERIAL_ENABLED) {
    Serial.print(F("After HIGH BRAKE release, RPM = "));
    Serial.println(currentRpm);
  }
  delay(STEP_DELAY/2);
  recordDataPoint();
  
  // Final all-stop to ensure safety
  allStop();
  recordDataPoint();
  
  // Print results
  if (SERIAL_ENABLED) {
    printTestSummary();
  }
}

// Print a summary of test statistics (min/max/avg RPM for each test)
void printTestStatisticsSummary() {
  if (!SERIAL_ENABLED) return;
  
  Serial.println(F("=================================================="));
  Serial.println(F("            TEST STATISTICS SUMMARY               "));
  Serial.println(F("=================================================="));
  Serial.println(F("Test               | Min RPM | Max RPM | Avg RPM "));
  Serial.println(F("--------------------------------------------------"));
  
  unsigned int avgRpm = currentTestStats.countNonZero > 0 ? 
    currentTestStats.totalRpm / currentTestStats.countNonZero : 0;
  
  Serial.println(F("-------------------------------------------------------"));
  Serial.print(F("RPM Summary - Min: "));
  Serial.print(currentTestStats.countNonZero > 0 ? currentTestStats.minRpm : 0);
  Serial.print(F(" | Max: "));
  Serial.print(currentTestStats.maxRpm);
  Serial.print(F(" | Avg: "));
  Serial.println(avgRpm);
  Serial.println();
}

// Print summary of test statistics
void printTestSummary() {
  if (!SERIAL_ENABLED) return;
  
  // Calculate average
  unsigned int avgRpm = currentTestStats.countNonZero > 0 ? 
    currentTestStats.totalRpm / currentTestStats.countNonZero : 0;
  
  if (JSON_OUTPUT) {
    Serial.print(F("{\"test_summary\": {\"test\": \""));
    Serial.print(currentTest);
    Serial.print(F("\", \"min_rpm\": "));
    Serial.print(currentTestStats.countNonZero > 0 ? currentTestStats.minRpm : 0);
    Serial.print(F(", \"max_rpm\": "));
    Serial.print(currentTestStats.maxRpm);
    Serial.print(F(", \"avg_rpm\": "));
    Serial.print(avgRpm);
    Serial.println(F("}}"));
  }
  else {
    Serial.println(F("-------------------------------------------------------"));
    Serial.print(F("Test Summary - "));
    Serial.print(currentTest);
    Serial.println(F(":"));
    Serial.print(F("Min RPM: "));
    Serial.print(currentTestStats.countNonZero > 0 ? currentTestStats.minRpm : 0);
    Serial.print(F(" | Max RPM: "));
    Serial.print(currentTestStats.maxRpm);
    Serial.print(F(" | Avg RPM: "));
    Serial.println(avgRpm);
    Serial.println(F("-------------------------------------------------------"));
  }
}

// Update the reset functionality to work on both platforms
void resetDevice() {
#if defined(ESP8266)
  ESP.restart();  // Proper way to reset ESP8266
#elif defined(ESP32)
  ESP.restart();  // Proper way to reset ESP32
#else
  asm volatile ("jmp 0");  // Reset for AVR Arduino
#endif
}

// Replace updateTemperatures with the sensor-based approach
void updateTemperatures() {
  // Directly read from the sensors
  batteryTemp = batteryTempSensor->getTemperature();
  controllerTemp = controllerTempSensor->getTemperature();
  motorTemp = motorTempSensor->getTemperature();
}

// Update checkTemperatureSafety to use our sensor framework
void checkTemperatureSafety() {
  // Update temperature variables from sensors
  updateTemperatures();
  
  bool prevCritical = tempCriticalActive;
  
  // Check for critical temperatures
  tempCriticalActive = (batteryTemp > TEMP_BATTERY_CRITICAL ||
                       controllerTemp > TEMP_CONTROLLER_CRITICAL ||
                       motorTemp > TEMP_MOTOR_CRITICAL);
                       
  // Check for warning temperatures
  tempWarningActive = (batteryTemp > TEMP_BATTERY_WARNING ||
                      controllerTemp > TEMP_CONTROLLER_WARNING ||
                      motorTemp > TEMP_MOTOR_WARNING);
  
  // Take action if critical temperature detected
  if (tempCriticalActive && !prevCritical) {
    if (SERIAL_ENABLED) {
      Serial.println(F("CRITICAL TEMPERATURE DETECTED! Engaging emergency stop!"));
    }
    allStop();  // Stop motor immediately
  }
  // Reduce throttle for warning temperature
  else if (tempWarningActive && currentThrottle > 100) {
    if (SERIAL_ENABLED) {
      Serial.println(F("WARNING: High temperature detected! Reducing throttle."));
    }
    setThrottle(100);  // Limit to lower throttle
  }
} 