#include <Arduino.h>
#include <SPI.h>
#include "Config.h"
#include "common.pb.h"
#include "motors.pb.h"
#include "ProtobufCANInterface.h"

#ifdef DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

// Create CAN interface
ProtobufCANInterface canInterface(NODE_ID);

// Motor state variables
uint8_t currentSpeed = DEFAULT_SPEED;
kart_motors_MotorDirectionValue currentDirection = DEFAULT_DIRECTION;
kart_motors_MotorModeValue currentMode = DEFAULT_MODE;
bool motorEnabled = false;
bool lowBrakeActive = false;
bool highBrakeActive = false;
bool cruiseControlActive = false;
bool eLockEngaged = true;  // Safety feature - start with lock engaged
bool hardBootActive = false;

// Hall sensor readings for diagnostic and feedback
volatile int hallSensor1Value = 0;
volatile int hallSensor2Value = 0;
volatile int hallSensor3Value = 0;
uint16_t motorRPM = 0;

// Timestamp for periodic status updates
unsigned long lastStatusTime = 0;
unsigned long lastHallSensorReadTime = 0;
unsigned long lastRPMCalcTime = 0;
volatile unsigned long hallPulseCount = 0;

// Function prototypes
void handleSpeedCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                      uint8_t component_id, uint8_t command_id, 
                      kart_common_ValueType value_type, int32_t value);

void handleDirectionCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                          uint8_t component_id, uint8_t command_id, 
                          kart_common_ValueType value_type, int32_t value);

void handleBrakeCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                       uint8_t component_id, uint8_t command_id, 
                       kart_common_ValueType value_type, int32_t value);

void handleModeCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                      uint8_t component_id, uint8_t command_id, 
                      kart_common_ValueType value_type, int32_t value);

void handleEnableCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                        uint8_t component_id, uint8_t command_id, 
                        kart_common_ValueType value_type, int32_t value);

void handleEmergencyCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                           uint8_t component_id, uint8_t command_id, 
                           kart_common_ValueType value_type, int32_t value);

void handleCruiseCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                        uint8_t component_id, uint8_t command_id, 
                        kart_common_ValueType value_type, int32_t value);

void handleELockCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                        uint8_t component_id, uint8_t command_id, 
                        kart_common_ValueType value_type, int32_t value);

void hallSensorISR();
void applyMotorSettings();
void sendStatusUpdate();
void readHallSensors();
void calculateRPM();
void handleHardBoot();

void setup() {
  #ifdef DEBUG_ENABLED
    Serial.begin(115200);
    DEBUG_PRINTLN("Kunray MY1020 Motor Controller Initializing...");
  #endif
  
  // Initialize motor control pins
  pinMode(THROTTLE_PIN, OUTPUT);
  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(LOW_BRAKE_PIN, OUTPUT);
  pinMode(HIGH_BRAKE_PIN, OUTPUT);
  pinMode(SPEED_MODE_PIN_1, OUTPUT);
  pinMode(SPEED_MODE_PIN_2, OUTPUT);
  pinMode(E_LOCK_PIN, OUTPUT);
  pinMode(HARD_BOOT_PIN, OUTPUT);
  pinMode(CRUISE_PIN, OUTPUT);
  
  // Initialize Hall sensor pins as inputs
  pinMode(HALL_SENSOR_1, INPUT);
  pinMode(HALL_SENSOR_2, INPUT);
  pinMode(HALL_SENSOR_3, INPUT);
  
  // Attach interrupt to one of the hall sensors for RPM measurement
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_3), hallSensorISR, RISING);
  
  // Initialize outputs to safe state
  analogWrite(THROTTLE_PIN, 0);           // Zero throttle
  digitalWrite(DIRECTION_PIN, LOW);       // Forward direction
  digitalWrite(LOW_BRAKE_PIN, HIGH);      // Low brake engaged
  digitalWrite(HIGH_BRAKE_PIN, HIGH);     // High brake engaged
  digitalWrite(SPEED_MODE_PIN_1, LOW);    // Low speed mode
  digitalWrite(SPEED_MODE_PIN_2, LOW);    // Low speed mode
  digitalWrite(E_LOCK_PIN, HIGH);         // E-Lock engaged (safety)
  digitalWrite(HARD_BOOT_PIN, LOW);       // Hard boot inactive
  digitalWrite(CRUISE_PIN, LOW);          // Cruise control inactive
  
  // Initialize CAN interface
  if (!canInterface.begin(CAN_SPEED)) {
    DEBUG_PRINTLN("Failed to initialize CAN interface");
    while (1) {
      // Blink LED or some indication that CAN init failed
      delay(500);
    }
  }
  
  DEBUG_PRINTLN("CAN interface initialized");
  
  // Register command handlers - handles both COMMAND and STATUS message types
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_SPEED,
                             handleSpeedCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_DIRECTION,
                             handleDirectionCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_BRAKE,
                             handleBrakeCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_MODE,
                             handleModeCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_ENABLE,
                             handleEnableCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_EMERGENCY,
                             handleEmergencyCommand);
  
  // Register handlers for ALL motor components (broadcast commands)
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_ALL, kart_motors_MotorCommandId_EMERGENCY,
                             handleEmergencyCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_ALL, kart_motors_MotorCommandId_ENABLE,
                             handleEnableCommand);
  
  // Register specific handlers for cruise control and E-Lock (safety key)
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, 
                             kart_motors_MotorCommandId_CALIBRATE, // Using CALIBRATE command for cruise
                             handleCruiseCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, kart_common_ComponentType_MOTORS, 
                             kart_motors_MotorComponentId_MAIN_DRIVE, 
                             kart_motors_MotorCommandId_STATUS, // Using STATUS command for E-Lock
                             handleELockCommand);
  
  DEBUG_PRINTLN("Kunray MY1020 Motor Controller Ready");
  
  // Send initial status update
  sendStatusUpdate();
}

void loop() {
  // Process incoming CAN messages
  canInterface.process();
  
  // Read Hall sensor values periodically
  unsigned long currentTime = millis();
  if (currentTime - lastHallSensorReadTime >= 50) { // Read every 50ms
    readHallSensors();
    lastHallSensorReadTime = currentTime;
  }
  
  // Calculate RPM periodically
  if (currentTime - lastRPMCalcTime >= 500) { // Calculate every 500ms
    calculateRPM();
    lastRPMCalcTime = currentTime;
  }
  
  // Send periodic status updates
  if (currentTime - lastStatusTime >= STATUS_INTERVAL) {
    sendStatusUpdate();
    lastStatusTime = currentTime;
  }
  
  // Small delay to prevent CPU hogging
  delay(10);
}

// Interrupt Service Routine for Hall Sensor pulses
void hallSensorISR() {
  hallPulseCount++;
}

// Read Hall sensor values
void readHallSensors() {
  hallSensor1Value = digitalRead(HALL_SENSOR_1);
  hallSensor2Value = digitalRead(HALL_SENSOR_2);
  hallSensor3Value = digitalRead(HALL_SENSOR_3);
  
  // Diagnostic output if needed
  #ifdef DEBUG_ENABLED
    if (motorEnabled && currentSpeed > 50) {
      DEBUG_PRINT("Hall sensors: ");
      DEBUG_PRINT(hallSensor1Value);
      DEBUG_PRINT(hallSensor2Value);
      DEBUG_PRINT(hallSensor3Value);
      DEBUG_PRINT(" RPM: ");
      DEBUG_PRINTLN(motorRPM);
    }
  #endif
}

// Calculate motor RPM based on Hall sensor pulses
void calculateRPM() {
  // 6 pulses per revolution with 3 hall sensors (2 pulses per sensor)
  // RPM = (pulseCount * 60) / (0.5 * 6)
  noInterrupts();
  uint32_t pulses = hallPulseCount;
  hallPulseCount = 0;
  interrupts();
  
  // Calculate RPM
  motorRPM = (pulses * 20); // (pulses * 60) / (0.5 * 6) = pulses * 20
}

// Handle Hard Boot functionality
void handleHardBoot() {
  DEBUG_PRINTLN("Performing hard boot sequence");
  
  // Save current state
  uint8_t savedSpeed = currentSpeed;
  bool savedEnabled = motorEnabled;
  
  // Disable motor
  motorEnabled = false;
  currentSpeed = 0;
  applyMotorSettings();
  
  // Pulse the hard boot pin
  digitalWrite(HARD_BOOT_PIN, HIGH);
  delay(100);
  digitalWrite(HARD_BOOT_PIN, LOW);
  delay(500); // Wait for controller to reset
  
  // Restore previous state if it was enabled
  if (savedEnabled) {
    motorEnabled = true;
    // Gradually ramp up speed
    for (uint8_t i = 0; i <= savedSpeed; i += 5) {
      currentSpeed = i;
      applyMotorSettings();
      delay(50);
    }
  }
  
  DEBUG_PRINTLN("Hard boot sequence completed");
}

// Command handlers
void handleSpeedCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                      uint8_t component_id, uint8_t command_id, 
                      kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received speed command: ");
  DEBUG_PRINTLN(value);
  
  // Ensure value is within valid range
  if (value < MOTOR_MIN_SPEED) value = MOTOR_MIN_SPEED;
  if (value > MOTOR_MAX_SPEED) value = MOTOR_MAX_SPEED;
  
  currentSpeed = value;
  applyMotorSettings();
  
  // Send status update to confirm the change
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_SPEED, 
                         kart_common_ValueType_UINT8, currentSpeed);
}

void handleDirectionCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                          uint8_t component_id, uint8_t command_id, 
                          kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received direction command: ");
  DEBUG_PRINTLN(value);
  
  // Validate direction value
  if (value != kart_motors_MotorDirectionValue_FORWARD && 
      value != kart_motors_MotorDirectionValue_REVERSE &&
      value != kart_motors_MotorDirectionValue_NEUTRAL) {
    DEBUG_PRINTLN("Invalid direction value");
    return;
  }
  
  // Safety check - slow down before changing direction
  if (currentDirection != value && currentSpeed > 20) {
    // First slow down the motor
    uint8_t oldSpeed = currentSpeed;
    currentSpeed = 0;
    applyMotorSettings();
    delay(500); // Allow motor to slow down
    
    // Change direction
    currentDirection = (kart_motors_MotorDirectionValue)value;
    applyMotorSettings();
    delay(200); // Allow direction change to take effect
    
    // Restore speed gradually
    for (uint8_t i = 0; i <= oldSpeed; i += 5) {
      currentSpeed = i;
      applyMotorSettings();
      delay(50);
    }
  } else {
    // Motor is already slow enough, change direction directly
    currentDirection = (kart_motors_MotorDirectionValue)value;
    applyMotorSettings();
  }
  
  // Send status update to confirm the change
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_DIRECTION, 
                         kart_common_ValueType_UINT8, currentDirection);
}

void handleBrakeCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                       uint8_t component_id, uint8_t command_id, 
                       kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received brake command: ");
  DEBUG_PRINTLN(value);
  
  // Value determines brake level: 0=none, 1=low, 2=high, 3=both
  switch (value) {
    case 0: // No brakes
      lowBrakeActive = false;
      highBrakeActive = false;
      break;
    case 1: // Low brake only
      lowBrakeActive = true;
      highBrakeActive = false;
      break;
    case 2: // High brake only
      lowBrakeActive = false;
      highBrakeActive = true;
      break;
    case 3: // Both brakes
      lowBrakeActive = true;
      highBrakeActive = true;
      break;
    default:
      DEBUG_PRINTLN("Invalid brake value");
      return;
  }
  
  applyMotorSettings();
  
  // Send status update to confirm the change - sending the brake level as value
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_BRAKE, 
                         kart_common_ValueType_UINT8, value);
}

void handleModeCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                      uint8_t component_id, uint8_t command_id, 
                      kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received mode command: ");
  DEBUG_PRINTLN(value);
  
  // Validate mode value
  if (value < kart_motors_MotorModeValue_LOW || value > kart_motors_MotorModeValue_CUSTOM) {
    DEBUG_PRINTLN("Invalid mode value");
    return;
  }
  
  currentMode = (kart_motors_MotorModeValue)value;
  applyMotorSettings();
  
  // Send status update to confirm the change
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_MODE, 
                         kart_common_ValueType_UINT8, currentMode);
}

void handleEnableCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                        uint8_t component_id, uint8_t command_id, 
                        kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received enable command: ");
  DEBUG_PRINTLN(value);
  
  motorEnabled = (value != 0);
  
  // Safety check - if enabling, make sure E-Lock is disengaged
  if (motorEnabled && eLockEngaged) {
    DEBUG_PRINTLN("Cannot enable motor while E-Lock is engaged");
    motorEnabled = false;
  }
  
  applyMotorSettings();
  
  // Send status update to confirm the change
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_ENABLE, 
                         kart_common_ValueType_BOOLEAN, motorEnabled);
}

void handleEmergencyCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                           uint8_t component_id, uint8_t command_id, 
                           kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received emergency command: ");
  DEBUG_PRINTLN(value);
  
  if (value == kart_motors_MotorEmergencyValue_STOP || 
      value == kart_motors_MotorEmergencyValue_SHUTDOWN) {
    // Emergency stop - immediately disable motor and engage both brakes
    motorEnabled = false;
    lowBrakeActive = true;
    highBrakeActive = true;
    currentSpeed = 0;
    applyMotorSettings();
    
    // For full shutdown, also engage E-Lock
    if (value == kart_motors_MotorEmergencyValue_SHUTDOWN) {
      eLockEngaged = true;
      digitalWrite(E_LOCK_PIN, HIGH);
      DEBUG_PRINTLN("EMERGENCY SHUTDOWN ACTIVATED");
    } else {
      DEBUG_PRINTLN("EMERGENCY STOP ACTIVATED");
    }
  } else if (value == kart_motors_MotorEmergencyValue_LIMP_HOME) {
    // Limp home mode - reduce speed to safe level
    if (currentSpeed > 50) {
      currentSpeed = 50; // Limit to safe speed
      applyMotorSettings();
      DEBUG_PRINTLN("LIMP HOME MODE ACTIVATED");
    }
  }
  
  // Send status update to confirm the change
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_EMERGENCY, 
                         kart_common_ValueType_UINT8, value);
}

void handleCruiseCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                        uint8_t component_id, uint8_t command_id, 
                        kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received cruise control command: ");
  DEBUG_PRINTLN(value);
  
  // Cruise can only be activated when moving above a certain speed
  if (value != 0 && currentSpeed > 50 && motorEnabled) {
    cruiseControlActive = true;
    digitalWrite(CRUISE_PIN, HIGH);
    DEBUG_PRINTLN("Cruise control activated");
  } else {
    cruiseControlActive = false;
    digitalWrite(CRUISE_PIN, LOW);
    DEBUG_PRINTLN("Cruise control deactivated");
  }
  
  // Send status update to confirm the change
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_CALIBRATE, 
                         kart_common_ValueType_BOOLEAN, cruiseControlActive);
}

void handleELockCommand(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                        uint8_t component_id, uint8_t command_id, 
                        kart_common_ValueType value_type, int32_t value) {
  if (msg_type != kart_common_MessageType_COMMAND) return;
  
  DEBUG_PRINT("Received E-Lock command: ");
  DEBUG_PRINTLN(value);
  
  // E-Lock is a safety feature: 0=unlocked, 1=locked
  eLockEngaged = (value != 0);
  
  if (eLockEngaged) {
    // When engaging lock, must disable motor first
    motorEnabled = false;
    currentSpeed = 0;
    applyMotorSettings();
    digitalWrite(E_LOCK_PIN, HIGH);
    DEBUG_PRINTLN("E-Lock engaged - motor locked");
  } else {
    digitalWrite(E_LOCK_PIN, LOW);
    DEBUG_PRINTLN("E-Lock disengaged - motor unlocked");
  }
  
  // Send status update to confirm the change
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_STATUS, 
                         kart_common_ValueType_BOOLEAN, eLockEngaged);
}

// Apply current motor settings to hardware
void applyMotorSettings() {
  // If brake is active or motor is disabled, stop the motor
  if (lowBrakeActive || highBrakeActive || !motorEnabled || eLockEngaged) {
    analogWrite(THROTTLE_PIN, 0);
    
    // Apply brakes if requested
    digitalWrite(LOW_BRAKE_PIN, lowBrakeActive ? HIGH : LOW);
    digitalWrite(HIGH_BRAKE_PIN, highBrakeActive ? HIGH : LOW);
    
    DEBUG_PRINTLN("Motor stopped, brakes: Low=" + String(lowBrakeActive ? "ON" : "OFF") + 
                  ", High=" + String(highBrakeActive ? "ON" : "OFF"));
    return;
  }
  
  // Set direction pin - only when motor is enabled and lock is disengaged
  if (currentDirection == kart_motors_MotorDirectionValue_FORWARD) {
    digitalWrite(DIRECTION_PIN, HIGH); // Forward
    DEBUG_PRINTLN("Direction: Forward");
  } else if (currentDirection == kart_motors_MotorDirectionValue_REVERSE) {
    digitalWrite(DIRECTION_PIN, LOW);  // Reverse
    DEBUG_PRINTLN("Direction: Reverse");
  } else {
    // Neutral - stop motor but don't engage brake
    analogWrite(THROTTLE_PIN, 0);
    digitalWrite(LOW_BRAKE_PIN, LOW);
    digitalWrite(HIGH_BRAKE_PIN, LOW);
    DEBUG_PRINTLN("Direction: Neutral");
    return;
  }
  
  // Set speed mode pins based on mode
  switch (currentMode) {
    case kart_motors_MotorModeValue_LOW:
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      DEBUG_PRINTLN("Speed mode: LOW");
      break;
    case kart_motors_MotorModeValue_MEDIUM:
      digitalWrite(SPEED_MODE_PIN_1, HIGH);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      DEBUG_PRINTLN("Speed mode: MEDIUM");
      break;
    case kart_motors_MotorModeValue_HIGH:
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, HIGH);
      DEBUG_PRINTLN("Speed mode: HIGH");
      break;
    case kart_motors_MotorModeValue_SPORT:
      digitalWrite(SPEED_MODE_PIN_1, HIGH);
      digitalWrite(SPEED_MODE_PIN_2, HIGH);
      DEBUG_PRINTLN("Speed mode: SPORT");
      break;
    default:
      // Default to low speed for other modes
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      DEBUG_PRINTLN("Speed mode: DEFAULT (LOW)");
      break;
  }
  
  // Disengage brakes
  digitalWrite(LOW_BRAKE_PIN, LOW);
  digitalWrite(HIGH_BRAKE_PIN, LOW);
  
  // Apply speed
  analogWrite(THROTTLE_PIN, currentSpeed);
  DEBUG_PRINT("Speed set to: ");
  DEBUG_PRINTLN(currentSpeed);
}

// Send status update
void sendStatusUpdate() {
  // Report current speed
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_SPEED, 
                         kart_common_ValueType_UINT8, currentSpeed);
  
  // Report current direction
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_DIRECTION, 
                         kart_common_ValueType_UINT8, currentDirection);
  
  // Report current mode
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_MODE, 
                         kart_common_ValueType_UINT8, currentMode);
  
  // Report brake status - combined value: 0=none, 1=low, 2=high, 3=both
  uint8_t brakeStatus = (lowBrakeActive ? 1 : 0) + (highBrakeActive ? 2 : 0);
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_BRAKE, 
                         kart_common_ValueType_UINT8, brakeStatus);
  
  // Report enable status
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_ENABLE, 
                         kart_common_ValueType_BOOLEAN, motorEnabled);
  
  // Report E-Lock status
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_STATUS, 
                         kart_common_ValueType_BOOLEAN, eLockEngaged);
  
  // Report cruise control status
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_CALIBRATE, 
                         kart_common_ValueType_BOOLEAN, cruiseControlActive);
  
  // Report RPM (motor speed feedback)
  canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                         kart_motors_MotorComponentId_MAIN_DRIVE, kart_motors_MotorCommandId_TORQUE, 
                         kart_common_ValueType_UINT16, motorRPM);
  
  DEBUG_PRINTLN("Status update sent");
}
