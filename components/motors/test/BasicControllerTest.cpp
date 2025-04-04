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

// PIN DEFINITIONS - must match your actual wiring
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
    
    Serial.println(F("Kunray MY1020 Basic Controller Test with Hall Sensor Monitoring"));
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
  
  setupPins();
  
  // Start with everything stopped/safe
  allStop();
  
  if (SERIAL_ENABLED) {
    Serial.println(F("Beginning test sequence in 3 seconds..."));
    Serial.println(F("Throttle | RPM | Hall State"));
    Serial.println();
  }
  delay(3000);
  
  // Record test start time
  testStartTime = millis();
  
  // Run the test sequence
  runTestSequence();
}

void loop() {
  // Main test has completed, just update hall sensor readings periodically
  updateHallReadings();
  
  // Reset if the serial monitored started after the test completed
  if (SERIAL_ENABLED && Serial.available()) {
    char c = Serial.read();
    if (c == 'r' || c == 'R') {
      Serial.println(F("Resetting and running test again..."));
      delay(1000);
      // Reset Arduino
      asm volatile ("jmp 0");
    } else if (c == 'j' || c == 'J') {
      // Output JSON format regardless of current setting
      Serial.println(F("Outputting results in JSON format..."));
      Serial.println(F("{\"message\": \"Results streamed in real-time\"}"));
    }
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
  
  // Attach interrupts for hall sensors
  attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), hallSensorA_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), hallSensorB_ISR, CHANGE);
  
  // Initialize to safe state
  digitalWrite(DIRECTION_PIN, HIGH);     // Forward
  digitalWrite(SPEED_MODE_1_PIN, LOW);   // Speed mode OFF
  digitalWrite(SPEED_MODE_2_PIN, LOW);   // Speed mode OFF
  setLowBrake(false);                    // Low brake disengaged (normal operation)
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
  if (SERIAL_ENABLED) {
    updateHallReadings();
    Serial.print(action);
    Serial.print(": ");
    Serial.print(value);
    Serial.print(F(" | RPM: "));
    Serial.print(currentRpm);
    Serial.print(F(" | Hall: "));
    Serial.println(hallState, BIN);
  }
  
  // Store the command for the next data point
  currentCommand = action;
}

// Hall sensor interrupt handlers
void hallSensorA_ISR() {
  hallPulseCount++;
  lastHallTime = micros();
}

void hallSensorB_ISR() {
  hallPulseCount++;
  lastHallTime = micros();
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

// Calculate RPM based on hall sensor pulses
unsigned int calculateRPM() {
  static unsigned long prevCount = 0;
  static unsigned long prevTime = 0;
  unsigned long currentTime = millis();
  
  // Calculate time elapsed since last update
  unsigned long timeElapsed = currentTime - prevTime;
  if (timeElapsed < 100) return currentRpm; // Don't update too frequently
  
  // Calculate pulses per minute
  unsigned long countDiff = hallPulseCount - prevCount;
  unsigned int rpm = 0;
  
  if (timeElapsed > 0 && countDiff > 0) {
    // Convert pulse count to RPM
    // For a typical BLDC with 3 hall sensors, one revolution creates 6 state changes
    // Multiply by 60000 to convert to minutes, divide by timeElapsed to get per-minute rate
    rpm = (countDiff * 60000) / (timeElapsed * 6);
  } else if (timeElapsed > 1000 && countDiff == 0) {
    // If no pulses for over 1 second, RPM is zero
    rpm = 0;
  }
  
  // Store current values for next calculation
  prevCount = hallPulseCount;
  prevTime = currentTime;
  
  return rpm;
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