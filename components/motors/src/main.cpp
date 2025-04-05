#include <Arduino.h>
#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "common.pb.h"
#include "motors.pb.h"

// Global state variables
ProtobufCANInterface canInterface(NODE_ID);

// Global variables for motor state
uint8_t currentThrottle = 0;
uint8_t currentDirection = kart_motors_MotorDirectionValue_NEUTRAL;
uint8_t currentSpeedMode = kart_motors_MotorModeValue_OFF;
uint8_t currentBrakeMode = kart_motors_MotorBrakeModeValue_OFF;
uint8_t currentStatus = kart_motors_MotorStatusValue_STATUS_UNKNOWN;

// Hall sensor variables
volatile unsigned long hallPulseCount = 0;
volatile unsigned long lastHallTime = 0;
unsigned long lastRpmUpdate = 0;
unsigned int currentRpm = 0;
byte hallState = 0;

// Status reporting
unsigned long lastStatusUpdate = 0;

void setup() {
#if DEBUG_ENABLED
  Serial.begin(115200);
  Serial.println(F("Kunray MY1020 Motor Controller"));
#endif

  // Initialize hardware pins
  setupPins();
  
  // Initialize CAN interface
  if (!canInterface.begin(CAN_SPEED)) {
#if DEBUG_ENABLED
    Serial.println(F("Failed to initialize CAN interface"));
#endif
    while (1) {
      // Blink LED to indicate error
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
    }
  }
  
#if DEBUG_ENABLED
  Serial.println(F("CAN interface initialized"));
#endif

  // Register command handlers
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MAIN_DRIVE, 
                               kart_motors_MotorCommandId_SPEED, 
                               handleSpeedCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MAIN_DRIVE, 
                               kart_motors_MotorCommandId_DIRECTION, 
                               handleDirectionCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MAIN_DRIVE, 
                               kart_motors_MotorCommandId_BRAKE, 
                               handleBrakeCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MAIN_DRIVE, 
                               kart_motors_MotorCommandId_MODE, 
                               handleModeCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MAIN_DRIVE, 
                               kart_motors_MotorCommandId_EMERGENCY, 
                               handleEmergencyCommand);
                               
  // Also handle broadcast commands
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_ALL, 
                               kart_motors_MotorCommandId_EMERGENCY, 
                               handleEmergencyCommand);

#if DEBUG_ENABLED
  Serial.println(F("Motor controller initialized"));
#endif
}

void loop() {
  // Process CAN messages
  canInterface.process();
  
  // Update hall sensor readings periodically
  updateHallReadings();
  
  // Send status updates periodically
  if (millis() - lastStatusUpdate > STATUS_INTERVAL) {
    sendStatusUpdate();
    lastStatusUpdate = millis();
  }
}

void setupPins() {
  // Set up control pins
  pinMode(THROTTLE_PIN, OUTPUT);
  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(SPEED_MODE_PIN_1, OUTPUT);
  pinMode(SPEED_MODE_PIN_2, OUTPUT);
  pinMode(LOW_BRAKE_PIN, OUTPUT);
  pinMode(HIGH_BRAKE_PIN, OUTPUT);
  
  // Set up hall sensor pins
  pinMode(HALL_SENSOR_1, INPUT_PULLUP);
  pinMode(HALL_SENSOR_2, INPUT_PULLUP);
  pinMode(HALL_SENSOR_3, INPUT_PULLUP);
  
  // Set up built-in LED
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Attach interrupts for hall sensors - use RISING edge only to reduce noise
  // and prevent overcounting (CHANGE detection counts each transition twice)
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_1), hallSensorA_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_2), hallSensorB_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_3), hallSensorC_ISR, RISING);
  
  // Initialize to safe state
  digitalWrite(DIRECTION_PIN, HIGH);     // Forward
  digitalWrite(SPEED_MODE_PIN_1, LOW);   // Speed mode OFF
  digitalWrite(SPEED_MODE_PIN_2, LOW);   // Speed mode OFF
  setLowBrake(false);                    // Low brake disengaged (normal operation)
  digitalWrite(HIGH_BRAKE_PIN, LOW);     // High brake disengaged
  analogWrite(THROTTLE_PIN, 0);          // Zero throttle
}

void setThrottle(uint8_t level) {
  // If low brake is engaged, don't allow motor to run
  if (currentLowBrake) {
    analogWrite(THROTTLE_PIN, 0);
    currentThrottle = 0;
    return;
  }
  
  currentThrottle = level;
  analogWrite(THROTTLE_PIN, level);
  
#if DEBUG_ENABLED
  Serial.print(F("Throttle set to: "));
  Serial.println(level);
#endif
}

void setDirection(bool forward) {
  // To change direction safely, ensure motor is stopped first
  if (currentThrottle > 0) {
    // Motor is running, stop it first
    setThrottle(0);
    delay(500); // Brief delay to ensure motor stops
  }
  
  currentDirection = forward;
  digitalWrite(DIRECTION_PIN, forward ? HIGH : LOW);
  
#if DEBUG_ENABLED
  Serial.print(F("Direction set to: "));
  Serial.println(forward ? F("Forward") : F("Reverse"));
#endif
}

void setSpeedMode(uint8_t mode) {
  // Validate mode (0=OFF, 1=LOW, 2=HIGH)
  if (mode > 2) {
    mode = 0; // Default to OFF for invalid values
  }
  
  currentSpeedMode = mode;
  
  switch (mode) {
    case 0: // OFF
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      break;
      
    case 1: // LOW speed
      digitalWrite(SPEED_MODE_PIN_1, HIGH);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      break;
      
    case 2: // HIGH speed
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, HIGH);
      break;
      
    default: // Default to OFF
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      break;
  }
  
#if DEBUG_ENABLED
  Serial.print(F("Speed mode set to: "));
  Serial.println(mode == 0 ? F("OFF") : mode == 1 ? F("LOW") : F("HIGH"));
#endif
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
  
  // If brake is engaged, ensure throttle is zero
  if (engaged && currentThrottle > 0) {
    setThrottle(0);
  }
  
#if DEBUG_ENABLED
  Serial.print(F("Low brake: "));
  Serial.println(engaged ? F("Engaged") : F("Disengaged"));
#endif
}

void setHighBrake(bool engaged) {
  currentHighBrake = engaged;
  digitalWrite(HIGH_BRAKE_PIN, engaged ? HIGH : LOW);
  
#if DEBUG_ENABLED
  Serial.print(F("High brake: "));
  Serial.println(engaged ? F("Engaged") : F("Disengaged"));
#endif
}

void allStop() {
  // Emergency stop function - stop motor and engage brakes
  setThrottle(0);
  setLowBrake(true);
  setHighBrake(true);
  
#if DEBUG_ENABLED
  Serial.println(F("EMERGENCY STOP - All systems halted"));
#endif
}

// Hall sensor interrupt handlers with debouncing
volatile unsigned long lastHallPulseTime[3] = {0, 0, 0};
const unsigned long DEBOUNCE_TIME = 1000; // 1ms debounce time (in microseconds)

void hallSensorA_ISR() {
  unsigned long currentTime = micros();
  // Apply debounce to avoid noise
  if (currentTime - lastHallPulseTime[0] > DEBOUNCE_TIME) {
    hallPulseCount++;
    lastHallTime = currentTime;
    lastHallPulseTime[0] = currentTime;
  }
}

void hallSensorB_ISR() {
  unsigned long currentTime = micros();
  // Apply debounce to avoid noise
  if (currentTime - lastHallPulseTime[1] > DEBOUNCE_TIME) {
    hallPulseCount++;
    lastHallTime = currentTime;
    lastHallPulseTime[1] = currentTime;
  }
}

void hallSensorC_ISR() {
  unsigned long currentTime = micros();
  // Apply debounce to avoid noise
  if (currentTime - lastHallPulseTime[2] > DEBOUNCE_TIME) {
    hallPulseCount++;
    lastHallTime = currentTime;
    lastHallPulseTime[2] = currentTime;
  }
}

// Update all hall sensor readings and calculate RPM
void updateHallReadings() {
  // Read current hall sensor state (binary pattern of all 3 sensors)
  hallState = 0;
  if (digitalRead(HALL_SENSOR_1)) hallState |= 0b001;
  if (digitalRead(HALL_SENSOR_2)) hallState |= 0b010;
  if (digitalRead(HALL_SENSOR_3)) hallState |= 0b100;
  
  // Update RPM calculation periodically
  if (millis() - lastRpmUpdate > RPM_UPDATE_INTERVAL) {
    currentRpm = calculateRPM();
    lastRpmUpdate = millis();
  }
}

// Improved RPM calculation without maximum capping
unsigned int calculateRPM() {
  static unsigned long prevCount = 0;
  static unsigned long prevTime = 0;
  unsigned long currentTime = millis();
  
  // Calculate time elapsed since last update
  unsigned long timeElapsed = currentTime - prevTime;
  if (timeElapsed < 100) return currentRpm; // Don't update too frequently
  
  // Calculate pulses per minute
  unsigned long countDiff = 0;
  
  // Handle possible overflow of hallPulseCount
  if (hallPulseCount >= prevCount) {
    countDiff = hallPulseCount - prevCount;
  } else {
    // Counter has overflowed, handle gracefully
    countDiff = (0xFFFFFFFF - prevCount) + hallPulseCount + 1;
  }
  
  unsigned int rpm = 0;
  
  // Calculate RPM with additional validation
  if (timeElapsed > 0 && countDiff > 0) {
    // For a BLDC with 3 hall sensors, one revolution typically creates 6 pulses (using RISING edge only)
    // The divisor is 6 because we're counting each phase change once (RISING only, not CHANGE)
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

void sendStatusUpdate() {
  // Update hall sensor readings to get latest RPM
  updateHallReadings();
  
  // Send motor status
  canInterface.sendMessage(
    kart_common_MessageType_STATUS,
    kart_common_ComponentType_MOTORS,
    kart_motors_MotorComponentId_MAIN_DRIVE,
    kart_motors_MotorCommandId_STATUS,
    kart_common_ValueType_UINT8,
    currentStatus
  );
  
  // Send motor speed (RPM)
  canInterface.sendMessage(
    kart_common_MessageType_STATUS,
    kart_common_ComponentType_MOTORS,
    kart_motors_MotorComponentId_MAIN_DRIVE,
    kart_motors_MotorCommandId_SPEED,
    kart_common_ValueType_UINT16,
    currentRpm
  );
  
  // Send direction
  canInterface.sendMessage(
    kart_common_MessageType_STATUS,
    kart_common_ComponentType_MOTORS,
    kart_motors_MotorComponentId_MAIN_DRIVE,
    kart_motors_MotorCommandId_DIRECTION,
    kart_common_ValueType_UINT8,
    currentDirection
  );
  
#if DEBUG_ENABLED
  Serial.print(F("Status update: RPM="));
  Serial.print(currentRpm);
  Serial.print(F(", Throttle="));
  Serial.print(currentThrottle);
  Serial.print(F(", Direction="));
  Serial.print(currentDirection ? F("FWD") : F("REV"));
  Serial.print(F(", Mode="));
  Serial.print(currentSpeedMode == 0 ? F("OFF") : currentSpeedMode == 1 ? F("LOW") : F("HIGH"));
  Serial.print(F(", Brake="));
  
  if (currentLowBrake) Serial.print(F("Low "));
  if (currentHighBrake) Serial.print(F("High"));
  if (!currentLowBrake && !currentHighBrake) Serial.print(F("None"));
  
  Serial.println();
#endif
}

void emergencyStop() {
  // Stop motor and engage low brake
  setThrottle(0);
  setLowBrake(true);
  currentStatus = kart_motors_MotorStatusValue_STATUS_OK; // Reset status since this is a commanded stop
  
#if DEBUG_ENABLED
  Serial.println(F("EMERGENCY STOP"));
#endif
}

void emergencyShutdown() {
  // Full shutdown - stop motor and engage all brakes
  setThrottle(0);
  setLowBrake(true);
  setHighBrake(true);
  setSpeedMode(0); // Set speed mode to OFF
  currentStatus = kart_motors_MotorStatusValue_STATUS_OK; // Reset status since this is a commanded shutdown
  
#if DEBUG_ENABLED
  Serial.println(F("EMERGENCY SHUTDOWN"));
#endif
}

// Command handlers
void handleSpeedCommand(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value) {
  // Validate input
  if (value < 0) value = 0;
  if (value > 255) value = 255;
  
  // Set throttle
  setThrottle(value);
  
  // Send acknowledgment
  canInterface.sendMessage(
    kart_common_MessageType_ACK,
    kart_common_ComponentType_MOTORS,
    component_id,
    command_id,
    kart_common_ValueType_UINT8,
    value
  );
}

void handleDirectionCommand(kart_common_MessageType msg_type,
                           kart_common_ComponentType comp_type,
                           uint8_t component_id,
                           uint8_t command_id,
                           kart_common_ValueType value_type,
                           int32_t value) {
  // Validate value
  bool isForward;
  
  if (value == kart_motors_MotorDirectionValue_FORWARD) {
    isForward = true;
  } else if (value == kart_motors_MotorDirectionValue_REVERSE) {
    isForward = false;
  } else if (value == kart_motors_MotorDirectionValue_NEUTRAL) {
    // For neutral, we stop the motor but don't change direction
    setThrottle(0);
    
    // Send acknowledgment
    canInterface.sendMessage(
      kart_common_MessageType_ACK,
      kart_common_ComponentType_MOTORS,
      component_id,
      command_id,
      kart_common_ValueType_UINT8,
      value
    );
    
    return;
  } else {
    // Invalid value, ignore or return current state
    canInterface.sendMessage(
      kart_common_MessageType_ERROR,
      kart_common_ComponentType_MOTORS,
      component_id,
      command_id,
      kart_common_ValueType_UINT8,
      currentDirection ? kart_motors_MotorDirectionValue_FORWARD : kart_motors_MotorDirectionValue_REVERSE
    );
    
    return;
  }
  
  // Set direction
  setDirection(isForward);
  
  // Send acknowledgment
  canInterface.sendMessage(
    kart_common_MessageType_ACK,
    kart_common_ComponentType_MOTORS,
    component_id,
    command_id,
    kart_common_ValueType_UINT8,
    isForward ? kart_motors_MotorDirectionValue_FORWARD : kart_motors_MotorDirectionValue_REVERSE
  );
}

void handleBrakeCommand(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value) {
  // Bit 0 = Low brake, Bit 1 = High brake
  bool lowBrake = (value & 0x01) != 0;
  bool highBrake = (value & 0x02) != 0;
  
  setLowBrake(lowBrake);
  setHighBrake(highBrake);
  
  // Send acknowledgment
  canInterface.sendMessage(
    kart_common_MessageType_ACK,
    kart_common_ComponentType_MOTORS,
    component_id,
    command_id,
    kart_common_ValueType_UINT8,
    (lowBrake ? 1 : 0) | (highBrake ? 2 : 0)
  );
}

void handleModeCommand(kart_common_MessageType msg_type,
                      kart_common_ComponentType comp_type,
                      uint8_t component_id,
                      uint8_t command_id,
                      kart_common_ValueType value_type,
                      int32_t value) {
  // Map from protocol speed modes to controller speed modes
  uint8_t speedMode;
  
  switch (value) {
    case kart_motors_MotorModeValue_LOW:
      speedMode = 1; // LOW speed
      break;
      
    case kart_motors_MotorModeValue_MEDIUM:
    case kart_motors_MotorModeValue_HIGH:
    case kart_motors_MotorModeValue_SPORT:
      speedMode = 2; // HIGH speed
      break;
      
    case kart_motors_MotorModeValue_ECO:
      speedMode = 1; // LOW speed for ECO mode
      break;
      
    default:
      speedMode = 0; // OFF for any other value
      break;
  }
  
  setSpeedMode(speedMode);
  
  // Send acknowledgment
  canInterface.sendMessage(
    kart_common_MessageType_ACK,
    kart_common_ComponentType_MOTORS,
    component_id,
    command_id,
    kart_common_ValueType_UINT8,
    value
  );
}

void handleEmergencyCommand(kart_common_MessageType msg_type,
                           kart_common_ComponentType comp_type,
                           uint8_t component_id,
                           uint8_t command_id,
                           kart_common_ValueType value_type,
                           int32_t value) {
  switch (value) {
    case kart_motors_MotorEmergencyValue_STOP:
      emergencyStop();
      break;
      
    case kart_motors_MotorEmergencyValue_SHUTDOWN:
      emergencyShutdown();
      break;
      
    case kart_motors_MotorEmergencyValue_LIMP_HOME:
      // Limit speed and set to LOW mode
      setSpeedMode(1); // LOW speed
      // Limit throttle to 30%
      if (currentThrottle > 75) {
        setThrottle(75);
      }
      break;
      
    case kart_motors_MotorEmergencyValue_NORMAL:
      // Return to normal operation
      setLowBrake(false);
      setHighBrake(false);
      break;
  }
  
  // Send acknowledgment
  canInterface.sendMessage(
    kart_common_MessageType_ACK,
    kart_common_ComponentType_MOTORS,
    component_id,
    command_id,
    kart_common_ValueType_UINT8,
    value
  );
}

void handleStatusCommand(kart_common_MessageType msg_type,
                        kart_common_ComponentType comp_type,
                        uint8_t component_id,
                        uint8_t command_id,
                        kart_common_ValueType value_type,
                        int32_t value) {
  // Send full status update immediately
  sendStatusUpdate();
  
  // Send acknowledgment
  canInterface.sendMessage(
    kart_common_MessageType_ACK,
    kart_common_ComponentType_MOTORS,
    component_id,
    command_id,
    kart_common_ValueType_UINT8,
    1
  );
}
