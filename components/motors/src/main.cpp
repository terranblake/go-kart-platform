#include "../include/Config.h"
#include "ProtobufCANInterface.h"
#include "common.pb.h"
#include "motors.pb.h"
#include "batteries.pb.h"

// Add sensor framework includes for integration
#include "Sensor.h"
#include "SensorRegistry.h"
#include "TemperatureSensor.h"
#include "RpmSensor.h"

// Add platform-specific defines
#if defined(ESP8266) || defined(ESP32)
#define ICACHE_RAM_ATTR ICACHE_RAM_ATTR
#else
#define ICACHE_RAM_ATTR
#endif

// Global state variables
ProtobufCANInterface canInterface(NODE_ID, CAN_CS_PIN, CAN_INT_PIN);

// Global variables for motor state
uint8_t currentThrottle = 0;
uint8_t currentDirection = kart_motors_MotorDirectionValue_NEUTRAL;
uint8_t currentSpeedMode = kart_motors_MotorModeValue_OFF;
uint8_t currentBrakeMode = kart_motors_MotorBrakeValue_BRAKE_OFF;
uint8_t currentStatus = kart_motors_MotorStatusValue_STATUS_UNKNOWN;

// Hall sensor variables
volatile unsigned long hallPulseCount = 0;
volatile unsigned long lastHallTime = 0;
unsigned long lastRpmUpdate = 0;
unsigned int currentRpm = 0;
byte hallState = 0;

// Brake state variables - define these externally as they're declared in Config.h
bool currentLowBrake = false;
bool currentHighBrake = false;

// Sensor framework integration - add sensor objects
SensorRegistry sensorRegistry(canInterface, kart_common_ComponentType_MOTORS, NODE_ID);
RpmSensor* motorRpmSensor;
TemperatureSensor* batteryTempSensor;
TemperatureSensor* controllerTempSensor;
TemperatureSensor* motorTempSensor;

// Function prototypes for new functions
#if DEBUG_MODE == 1
void parseSerialCommands();
#endif

void setup() {
#if DEBUG_MODE
  Serial.begin(115200);
  Serial.println(F("Kunray MY1020 Motor Controller"));
#endif

  // Initialize hardware pins
  setupPins();
  
  // Initialize CAN interface
  if (!canInterface.begin(CAN_BAUDRATE)) {
// #if DEBUG_MODE
    Serial.println(F("Failed to initialize CAN interface"));
// #endif
  } else {
    Serial.println(F("CAN interface initialized"));
  }
    // Register command handlers
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, 
                               kart_motors_MotorCommandId_THROTTLE, 
                               handleThrottleCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, 
                               kart_motors_MotorCommandId_DIRECTION, 
                               handleDirectionCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, 
                               kart_motors_MotorCommandId_BRAKE, 
                               handleBrakeCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, 
                               kart_motors_MotorCommandId_MODE, 
                               handleModeCommand);
  
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, 
                               kart_motors_MotorCommandId_EMERGENCY, 
                               handleEmergencyCommand);
                               
  // Also handle broadcast commands
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_ALL, 
                               kart_motors_MotorCommandId_EMERGENCY, 
                               handleEmergencyCommand);

  // Initialize temperature sensors
  batteryTempSensor = new TemperatureSensor(
    kart_common_ComponentType_BATTERIES,
    kart_batteries_BatteryComponentId_MOTOR_LEFT_REAR,
    kart_batteries_BatteryCommandId_TEMPERATURE,
    TEMP_SENSOR_BATTERY,       // Pin
    2000,                      // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  // todo: how to support controller sensors?
  // controllerTempSensor = new TemperatureSensor(
  //   1,                         // Location ID (CONTROLLER=1)
  //   TEMP_SENSOR_CONTROLLER,    // Pin
  //   2000,                      // Update interval (2 seconds)
  //   SERIES_RESISTOR,
  //   THERMISTOR_NOMINAL,
  //   TEMPERATURE_NOMINAL,
  //   B_COEFFICIENT
  // );
  
  motorTempSensor = new TemperatureSensor(
    kart_common_ComponentType_MOTORS,
    kart_motors_MotorComponentId_MOTOR_LEFT_REAR,
    kart_motors_MotorCommandId_TEMPERATURE,
    TEMP_SENSOR_MOTOR,         // Pin
    2000,                      // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );

  // Initialize RPM sensor with the sensor framework
  motorRpmSensor = new RpmSensor(
    kart_motors_MotorComponentId_MOTOR_LEFT_REAR,
    1000
  );
  
  // Register the sensors with the registry
  sensorRegistry.registerSensor(motorRpmSensor);
  sensorRegistry.registerSensor(batteryTempSensor);
  // sensorRegistry.registerSensor(controllerTempSensor);
  sensorRegistry.registerSensor(motorTempSensor);
  
#if DEBUG_MODE
  Serial.println(F("Motor controller initialized"));
#endif
}

void loop() {
  // Process CAN messages
  canInterface.process();
  
  // Process all sensors using the SensorRegistry
  sensorRegistry.process();
  
  // Parse serial commands in DEBUG_MODE
#if DEBUG_MODE == 1
  parseSerialCommands();
#endif

  // Yield control to allow background tasks/scheduler to run
  delay(1);
}

void setupPins() {
  // Set up control pins
  pinMode(THROTTLE_PIN, OUTPUT);
  pinMode(DIRECTION_PIN, OUTPUT);
  pinMode(SPEED_MODE_PIN_1, OUTPUT);
  pinMode(SPEED_MODE_PIN_2, OUTPUT);
  pinMode(LOW_BRAKE_PIN, OUTPUT);
  pinMode(HIGH_BRAKE_PIN, OUTPUT);
  
  // Initialize to safe state
  setDirection(DEFAULT_DIRECTION);
  setMode(DEFAULT_MODE);
  setBrake(DEFAULT_BRAKE);
}

void setThrottle(uint8_t level) {
  // If low brake is engaged, don't allow motor to run
  if (currentBrakeMode != kart_motors_MotorBrakeValue_BRAKE_OFF) {
    analogWrite(THROTTLE_PIN, 0);
    currentThrottle = 0;
    return;
  }

  // level will be some value between 0 and 100; scale level between min and max
  level = map(level, 0, 100, MIN_THROTTLE, MAX_THROTTLE);
  
  currentThrottle = level;
  analogWrite(THROTTLE_PIN, level);
  
#if DEBUG_MODE
  Serial.print(F("Throttle set to: "));
  Serial.println(level);
#endif
}

void setDirection(kart_motors_MotorDirectionValue direction) {
  // To change direction safely, ensure motor is stopped first
  if (currentThrottle > 0 && direction != kart_motors_MotorDirectionValue_NEUTRAL) {
    // Motor is running, stop it first
    setThrottle(0);
    delay(500); // Brief delay to ensure motor stops
  } else if (direction == kart_motors_MotorDirectionValue_NEUTRAL) {
    // Neutral direction - stop motor
    // very low throttle to keep motor free and able to turn
    setThrottle(0);
  } else {
    // Normal operation
    currentDirection = direction;
    digitalWrite(DIRECTION_PIN, direction == kart_motors_MotorDirectionValue_FORWARD ? HIGH : LOW);
  }
  
#if DEBUG_MODE
  Serial.print(F("Direction set to: "));
  Serial.println(direction == kart_motors_MotorDirectionValue_FORWARD ? F("Forward") : F("Reverse"));
#endif
}

void setMode(kart_motors_MotorModeValue mode) {
  currentSpeedMode = mode;
  
  switch (mode) {
    case kart_motors_MotorModeValue_OFF: // OFF
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      break;
      
    case kart_motors_MotorModeValue_LOW: // LOW speed
      digitalWrite(SPEED_MODE_PIN_1, HIGH);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      break;
      
    case kart_motors_MotorModeValue_HIGH: // HIGH speed
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, HIGH);
      break;
      
    default: // Default to OFF
      digitalWrite(SPEED_MODE_PIN_1, LOW);
      digitalWrite(SPEED_MODE_PIN_2, LOW);
      break;
  }
  
#if DEBUG_MODE
  Serial.print(F("Speed mode set to: "));
  Serial.println(
    mode == kart_motors_MotorModeValue_OFF
      ? F("OFF")
      : mode == kart_motors_MotorModeValue_LOW
        ? F("LOW")
        : F("HIGH")
  );
#endif
}

void setBrake(kart_motors_MotorBrakeValue brake) {
  currentBrakeMode = brake;
  digitalWrite(HIGH_BRAKE_PIN, brake == kart_motors_MotorBrakeValue_BRAKE_ON ? HIGH : LOW);
  
#if DEBUG_MODE
  Serial.print(F("Brake mode: "));
  Serial.println(currentBrakeMode);
#endif
}

void allStop() {
  // Emergency stop function - stop motor and engage brakes
  setThrottle(0);
  setBrake(kart_motors_MotorBrakeValue_BRAKE_ON);
  
#if DEBUG_MODE
  // Serial.println(F("EMERGENCY STOP - All systems halted"));
#endif
}

void emergencyStop() {
  // Stop motor and engage low brake
  setThrottle(0);
  setBrake(kart_motors_MotorBrakeValue_BRAKE_ON);
  currentStatus = kart_motors_MotorStatusValue_STATUS_OK; // Reset status since this is a commanded stop
  
#if DEBUG_MODE
  Serial.println(F("EMERGENCY STOP"));
#endif
}

void emergencyShutdown() {
  // Full shutdown - stop motor and engage all brakes
  setThrottle(0);
  setBrake(kart_motors_MotorBrakeValue_BRAKE_ON);
  setMode(kart_motors_MotorModeValue_OFF);
  currentStatus = kart_motors_MotorStatusValue_STATUS_OK; // Reset status since this is a commanded shutdown
  
#if DEBUG_MODE
  Serial.println(F("EMERGENCY SHUTDOWN"));
#endif
}

// Command handlers
void handleThrottleCommand(kart_common_MessageType msg_type,
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
}

void handleDirectionCommand(kart_common_MessageType msg_type,
                           kart_common_ComponentType comp_type,
                           uint8_t component_id,
                           uint8_t command_id,
                           kart_common_ValueType value_type,
                           int32_t value) {
  
  // Set direction
  setDirection(static_cast<kart_motors_MotorDirectionValue>(value));
}

void handleBrakeCommand(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value) {
  setBrake(static_cast<kart_motors_MotorBrakeValue>(value));
}

void handleModeCommand(kart_common_MessageType msg_type,
                      kart_common_ComponentType comp_type,
                      uint8_t component_id,
                      uint8_t command_id,
                      kart_common_ValueType value_type,
                      int32_t value) {
  setMode(static_cast<kart_motors_MotorModeValue>(value));
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
      setMode(kart_motors_MotorModeValue_LOW); // LOW speed
      // Limit throttle to 30%
      if (currentThrottle > 75) {
        setThrottle(75);
      }
      break;
      
    case kart_motors_MotorEmergencyValue_NORMAL:
      // Return to normal operation
      setBrake(kart_motors_MotorBrakeValue_BRAKE_OFF);
      break;
  }
}

// Serial command processing for testing
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
      uint8_t throttleValue = map(throttlePercent, 0, 100, 0, 255);
      
      // Apply the throttle
      setThrottle(throttleValue);
      
      // Print confirmation
      Serial.print(F("Throttle set to "));
      Serial.print(throttlePercent);
      Serial.print(F("% ("));
      Serial.print(throttleValue);
      Serial.println(F(")"));
      
      // Send acknowledgement
      Serial.println(F("ACK: THROTTLE command processed"));
      canInterface.sendMessage(
        kart_common_MessageType_STATUS,
        kart_common_ComponentType_MOTORS, 
        kart_motors_MotorComponentId_MOTOR_LEFT_REAR,
        kart_motors_MotorCommandId_THROTTLE,
        kart_common_ValueType_UINT8,
        currentThrottle
      );
    }
    // Command format: "D:F" for forward, "D:R" for reverse
    else if (command.startsWith("D:")) {
      char direction = command.charAt(2);
      if (direction == 'F' || direction == 'f') {
        setDirection(kart_motors_MotorDirectionValue_FORWARD);
        Serial.println(F("Direction set to FORWARD"));
      } 
      else if (direction == 'R' || direction == 'r') {
        setDirection(kart_motors_MotorDirectionValue_REVERSE);
        Serial.println(F("Direction set to REVERSE"));
      }
      else if (direction == 'N' || direction == 'n') {
        setDirection(kart_motors_MotorDirectionValue_NEUTRAL);
        Serial.println(F("Direction set to NEUTRAL"));
      }
      
      // Send acknowledgement
      Serial.println(F("ACK: DIRECTION command processed"));
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, kart_motors_MotorCommandId_DIRECTION, 
                               kart_common_ValueType_UINT8, currentDirection);
    }
    // Command format: "S:0" for OFF, "S:1" for LOW, "S:2" for HIGH
    else if (command.startsWith("S:")) {
      int speedMode = command.substring(2).toInt();
      if (speedMode >= 0 && speedMode <= 2) {
        setMode(static_cast<kart_motors_MotorModeValue>(speedMode));
        Serial.print(F("Speed mode set to "));
        Serial.println(speedMode);
        
        // Send acknowledgement
        Serial.println(F("ACK: SPEED_MODE command processed"));
        canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                                 kart_motors_MotorComponentId_MOTOR_LEFT_REAR, kart_motors_MotorCommandId_MODE, 
                                 kart_common_ValueType_UINT8, currentSpeedMode);
      }
    }
    // Command format: "B:L" for low brake, "B:H" for high brake, "B:N" for no brake
    else if (command.startsWith("B:")) {
      char brakeCommand = command.charAt(2);
      if (brakeCommand == 'L' || brakeCommand == 'l') {
        setBrake(kart_motors_MotorBrakeValue_BRAKE_ON);
        Serial.println(F("LOW Brake ENGAGED"));
      }
      else if (brakeCommand == 'H' || brakeCommand == 'h') {
        setBrake(kart_motors_MotorBrakeValue_BRAKE_ON);
        Serial.println(F("HIGH Brake ENGAGED"));
      }
      else if (brakeCommand == 'N' || brakeCommand == 'n') {
        setBrake(kart_motors_MotorBrakeValue_BRAKE_OFF);
        Serial.println(F("Brakes DISENGAGED"));
      }
      
      // Send acknowledgement
      Serial.println(F("ACK: BRAKE command processed"));
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, kart_motors_MotorCommandId_BRAKE, 
                               kart_common_ValueType_UINT8, currentBrakeMode);
    }
    // Emergency commands
    else if (command == "STOP") {
      emergencyStop();
      Serial.println(F("EMERGENCY STOP executed"));
      
      // Send acknowledgement
      Serial.println(F("ACK: EMERGENCY_STOP command processed"));
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, kart_motors_MotorCommandId_EMERGENCY, 
                               kart_common_ValueType_UINT8, kart_motors_MotorEmergencyValue_STOP);
    }
    else if (command == "SHUTDOWN") {
      emergencyShutdown();
      Serial.println(F("EMERGENCY SHUTDOWN executed"));
      
      // Send acknowledgement
      Serial.println(F("ACK: EMERGENCY_SHUTDOWN command processed"));
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, kart_motors_MotorCommandId_EMERGENCY, 
                               kart_common_ValueType_UINT8, kart_motors_MotorEmergencyValue_SHUTDOWN);
    }
    // Help command
    else if (command == "HELP") {
      Serial.println(F("\n--- DEBUG COMMANDS ---"));
      Serial.println(F("T:value - Set throttle (0-100%)"));
      Serial.println(F("D:F     - Set direction FORWARD"));
      Serial.println(F("D:R     - Set direction REVERSE"));
      Serial.println(F("D:N     - Set direction NEUTRAL"));
      Serial.println(F("S:0     - Speed mode OFF"));
      Serial.println(F("S:1     - Speed mode LOW"));
      Serial.println(F("S:2     - Speed mode HIGH"));
      Serial.println(F("B:L     - Engage LOW brake"));
      Serial.println(F("B:H     - Engage HIGH brake"));
      Serial.println(F("B:N     - Disengage brakes"));
      Serial.println(F("STOP    - Emergency stop"));
      Serial.println(F("SHUTDOWN- Emergency shutdown"));
      Serial.println(F("STATUS  - Show system status"));
      Serial.println(F("HELP    - Show this help"));
      Serial.println(F("---------------------\n"));
      
      // Send acknowledgement for HELP command
      Serial.println(F("ACK: HELP command processed"));
    }
    else if (command == "STATUS") {
      Serial.println(F("STATUS"));
      Serial.print(F("RPM: "));
      Serial.println(currentRpm);
      Serial.print(F("Throttle: "));
      Serial.println(currentThrottle);
      Serial.print(F("Direction: "));
      Serial.println(currentDirection);
      Serial.print(F("Brake mode: "));
      Serial.println(currentBrakeMode);
      Serial.print(F("Speed mode: "));
      Serial.println(currentSpeedMode);
      Serial.print(F("Low brake: "));
      Serial.println(currentLowBrake);
      Serial.print(F("High brake: "));
      Serial.println(currentHighBrake);
      Serial.print(F("Battery temp: "));
      // Get values from sensor framework
      Serial.println(batteryTempSensor ? batteryTempSensor->getValue() : 0.0);
      Serial.print(F("Controller temp: "));
      Serial.println(controllerTempSensor ? controllerTempSensor->getValue() : 0.0);
      Serial.print(F("Motor temp: "));
      Serial.println(motorTempSensor ? motorTempSensor->getValue() : 0.0);
      Serial.print(F("Hall state: 0b"));
      Serial.println(hallState, BIN);
      Serial.print(F("Hall pulse count: "));
      Serial.println(hallPulseCount);
      Serial.print(F("Current status: "));
      Serial.println(currentStatus);
      
      // Send acknowledgement for STATUS command
      Serial.println(F("ACK: STATUS command processed"));
    }
    else {
      Serial.println(F("Unknown command. Type 'HELP' for commands."));
      Serial.println(F("ACK: UNKNOWN command rejected"));
    }
  }
}
#endif