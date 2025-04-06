#include <Arduino.h>
#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "common.pb.h"
#include "motors.pb.h"

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
ProtobufCANInterface canInterface(NODE_ID);

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

// Global variables for temperature readings
float batteryTemp = 0.0;
float controllerTemp = 0.0;
float motorTemp = 0.0;
bool tempWarningActive = false;
bool tempCriticalActive = false;
unsigned long lastTempUpdate = 0;
unsigned long lastStatusUpdate = 0;

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
  if (!canInterface.begin(500E3)) {
#if DEBUG_MODE
    Serial.println(F("Failed to initialize CAN interface"));
#endif
  }
  
#if DEBUG_MODE
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

  // Initialize temperature sensors
  batteryTempSensor = new TemperatureSensor(
    2,                         // Location ID (BATTERY=2)
    TEMP_SENSOR_BATTERY,       // Pin
    2000,                      // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  controllerTempSensor = new TemperatureSensor(
    1,                         // Location ID (CONTROLLER=1)
    TEMP_SENSOR_CONTROLLER,    // Pin
    2000,                      // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );
  
  motorTempSensor = new TemperatureSensor(
    0,                         // Location ID (MOTOR=0)
    TEMP_SENSOR_MOTOR,         // Pin
    2000,                      // Update interval (2 seconds)
    SERIES_RESISTOR,
    THERMISTOR_NOMINAL,
    TEMPERATURE_NOMINAL,
    B_COEFFICIENT
  );

  // Initialize RPM sensor with the sensor framework
  motorRpmSensor = new RpmSensor(
    kart_motors_MotorComponentId_MAIN_DRIVE, // Use motor component ID
    100                                      // Update interval 100ms
  );
  
  // Register the sensors with the registry
  sensorRegistry.registerSensor(motorRpmSensor);
  sensorRegistry.registerSensor(batteryTempSensor);
  sensorRegistry.registerSensor(controllerTempSensor);
  sensorRegistry.registerSensor(motorTempSensor);
  
#if DEBUG_MODE
  Serial.println(F("Motor controller initialized"));
#endif
}

void loop() {
  // Process CAN messages
  canInterface.process();
  
  // Update hall sensor readings periodically
  updateHallReadings();
  
  // Process all sensors using the SensorRegistry
  sensorRegistry.process();
  
  // Parse serial commands in DEBUG_MODE
#if DEBUG_MODE == 1
  parseSerialCommands();
#endif
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
  pinMode(HALL_A_PIN, INPUT_PULLUP);
  pinMode(HALL_B_PIN, INPUT_PULLUP);
  pinMode(HALL_C_PIN, INPUT_PULLUP);
  
  // Attach interrupts for hall sensors - use RISING edge only to reduce noise
  // and prevent overcounting (CHANGE detection counts each transition twice)
  attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), hallSensorA_ISR, RISING);
  attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), hallSensorB_ISR, RISING);
  
  // Conditionally attach interrupt for hall sensor C based on platform
#if defined(ESP8266) || defined(ESP32)
  // ESP8266/ESP32 can use interrupts on all pins
  attachInterrupt(digitalPinToInterrupt(HALL_C_PIN), hallSensorC_ISR, RISING);
#endif
  
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
  if (currentBrakeMode != kart_motors_MotorBrakeValue_BRAKE_OFF) {
    analogWrite(THROTTLE_PIN, 0);
    currentThrottle = 0;
    return;
  }
  
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
  
#if DEBUG_MODE
  Serial.print(F("Speed mode set to: "));
  Serial.println(mode == 0 ? F("OFF") : mode == 1 ? F("LOW") : F("HIGH"));
#endif
}

void setLowBrake(bool engaged) {
  currentLowBrake = engaged;
  
  if (engaged) {
    currentBrakeMode = kart_motors_MotorBrakeValue_BRAKE_LOW;
  } else if (!currentHighBrake) {
    // Only set to OFF if high brake is also not engaged
    currentBrakeMode = kart_motors_MotorBrakeValue_BRAKE_OFF;
  }
  
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
  
#if DEBUG_MODE
  Serial.print(F("Low brake: "));
  Serial.println(engaged ? F("Engaged") : F("Disengaged"));
  Serial.print(F("Brake mode: "));
  Serial.println(currentBrakeMode);
#endif
}

void setHighBrake(bool engaged) {
  currentHighBrake = engaged;
  
  if (engaged) {
    currentBrakeMode = kart_motors_MotorBrakeValue_BRAKE_HIGH;
  } else if (!currentLowBrake) {
    // Only set to OFF if low brake is also not engaged
    currentBrakeMode = kart_motors_MotorBrakeValue_BRAKE_OFF;
  }
  
  digitalWrite(HIGH_BRAKE_PIN, engaged ? HIGH : LOW);
  
#if DEBUG_MODE
  Serial.print(F("High brake: "));
  Serial.println(engaged ? F("Engaged") : F("Disengaged"));
  Serial.print(F("Brake mode: "));
  Serial.println(currentBrakeMode);
#endif
}

void allStop() {
  // Emergency stop function - stop motor and engage brakes
  setThrottle(0);
  setLowBrake(true);
  setHighBrake(true);
  
#if DEBUG_MODE
  // Serial.println(F("EMERGENCY STOP - All systems halted"));
#endif
}

// Hall sensor interrupt handlers with debouncing
volatile unsigned long lastHallPulseTime[3] = {0, 0, 0};
const unsigned long DEBOUNCE_TIME = 1000; // 1ms debounce time (in microseconds)

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
    currentRpm = motorRpmSensor->getRPM();
    lastRpmUpdate = millis();
  }
}

void emergencyStop() {
  // Stop motor and engage low brake
  setThrottle(0);
  setLowBrake(true);
  currentStatus = kart_motors_MotorStatusValue_STATUS_OK; // Reset status since this is a commanded stop
  
#if DEBUG_MODE
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
  
#if DEBUG_MODE
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
  // Bit 0 = Low brake, Bit 1 = High brake
  bool lowBrake = (value & 0x01) != 0;
  bool highBrake = (value & 0x02) != 0;
  
  setLowBrake(lowBrake);
  setHighBrake(highBrake);
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
    }
    // Command format: "S:0" for OFF, "S:1" for LOW, "S:2" for HIGH
    else if (command.startsWith("S:")) {
      int speedMode = command.substring(2).toInt();
      if (speedMode >= 0 && speedMode <= 2) {
        setSpeedMode(speedMode);
        Serial.print(F("Speed mode set to "));
        Serial.println(speedMode);
      }
    }
    // Command format: "B:L" for low brake, "B:H" for high brake, "B:N" for no brake
    else if (command.startsWith("B:")) {
      char brakeCommand = command.charAt(2);
      if (brakeCommand == 'L' || brakeCommand == 'l') {
        setLowBrake(true);
        setHighBrake(false);
        Serial.println(F("LOW Brake ENGAGED"));
      }
      else if (brakeCommand == 'H' || brakeCommand == 'h') {
        setLowBrake(false);
        setHighBrake(true);
        Serial.println(F("HIGH Brake ENGAGED"));
      }
      else if (brakeCommand == 'N' || brakeCommand == 'n') {
        setLowBrake(false);
        setHighBrake(false);
        Serial.println(F("Brakes DISENGAGED"));
      }
    }
    // Emergency commands
    else if (command == "STOP") {
      emergencyStop();
      Serial.println(F("EMERGENCY STOP executed"));
    }
    else if (command == "SHUTDOWN") {
      emergencyShutdown();
      Serial.println(F("EMERGENCY SHUTDOWN executed"));
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
      Serial.println(batteryTempSensor ? batteryTempSensor->getTemperature() : 0.0);
      Serial.print(F("Controller temp: "));
      Serial.println(controllerTempSensor ? controllerTempSensor->getTemperature() : 0.0);
      Serial.print(F("Motor temp: "));
      Serial.println(motorTempSensor ? motorTempSensor->getTemperature() : 0.0);
      Serial.print(F("Temp warning: "));
      Serial.println(tempWarningActive ? F("ACTIVE") : F("INACTIVE"));
      Serial.print(F("Temp critical: "));
      Serial.println(tempCriticalActive ? F("ACTIVE") : F("INACTIVE"));
      Serial.print(F("Hall state: 0b"));
      Serial.println(hallState, BIN);
      Serial.print(F("Hall pulse count: "));
      Serial.println(hallPulseCount);
      Serial.print(F("Current status: "));
      Serial.println(currentStatus);
    }
    else {
      Serial.println(F("Unknown command. Type 'HELP' for commands."));
    }
  }
}
#endif