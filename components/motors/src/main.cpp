#include "../include/Config.h"
#include "ProtobufCANInterface.h"
#include "common.pb.h"
#include "motors.pb.h"
#include "batteries.pb.h"

// Add sensor framework includes for integration
#include "Sensor.h"
#include "SensorRegistry.h"
// Reverted to relative path
#include "ThermistorSensor.h"
#include "KunrayHallRpmSensor.h"
#include "DifferentialCurrentSensor.h"

// Include ADC Reader framework
#include "AnalogReader.h"       // Base interface
#include "ADS1115Reader.h"    // Specific implementation for motors component

// Include ADS1115 library
#include <Adafruit_ADS1X15.h> // Required for ADS1115Reader
// ** NOTE: Add "Adafruit ADS1X15" to lib_deps in platformio.ini **

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

// ADC Reader Instantiation (Moved to global scope)
Adafruit_ADS1115 ads;         // Create ADS1115 object (use default I2C address 0x48)
// ADS1115Reader adsReader(&ads); // Create the reader wrapper, passing the object

// Sensor framework integration - add sensor objects
SensorRegistry sensorRegistry(canInterface, kart_common_ComponentType_MOTORS, NODE_ID);
KunrayHallRpmSensor* motorRpmSensor;
ThermistorSensor* batteryTempSensor;
ThermistorSensor* controllerTempSensor;
ThermistorSensor* motorTempSensor;
DifferentialCurrentSensor* shuntCurrentSensor = nullptr;

// Function prototypes for new functions
#if DEBUG_MODE == 1
void parseSerialCommands();
#endif

void setup() {
#if DEBUG_MODE
  Serial.begin(115200);
  Serial.println(F("Kunray MY1020 Motor Controller (with ADS1115)"));
#endif

  // Initialize I2C for ADS1115 before initializing the reader
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // Ensure I2C pins are configured

  // Initialize ADS1115 Reader
  // Set gain appropriate for the thermistor voltage divider output.
  // Assuming 3.3V logic and 10k/10k divider, max voltage is ~1.65V.
  // GAIN_TWO (+/-2.048V) provides good resolution.
  if (!adsReader.begin()) { // This calls ads.begin() and sets gain
    #if DEBUG_MODE
      Serial.println(F("Failed to initialize ADS1115 Reader"));
      // Consider halting or indicating error
    #endif
  } else {
     adsReader.setGain(GAIN_TWO); // Reverted gain for thermistors (~1.7V range -> +/- 2.048V FS)
     #if DEBUG_MODE
      Serial.println(F("ADS1115 Reader initialized with Gain +/-2.048V (GAIN_TWO)"));
     #endif
  }

  // Initialize hardware pins (excluding thermistor analog pins)
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

  // Initialize temperature sensors using ADS1115 Reader
  // batteryTempSensor = new ThermistorSensor(
  //   kart_common_ComponentType_BATTERIES,
  //   kart_batteries_BatteryComponentId_MOTOR_LEFT_REAR,
  //   kart_batteries_BatteryCommandId_TEMPERATURE,
  //   (AnalogReader*)&adsReader,   // Explicitly cast to AnalogReader*
  //   1,                         // Use ADS1115 Channel 1 for Battery Temp
  //   1000,                      // Update interval (ms)
  //   SERIES_RESISTOR,
  //   THERMISTOR_NOMINAL,
  //   TEMPERATURE_NOMINAL,
  //   B_COEFFICIENT,
  //   kart_common_ValueType_INT16, // Using INT16
  //   3300                       // Divider supply voltage (mV)
  // );
  
  // todo: how to support controller sensors? Needs ADC channel assignment
  // controllerTempSensor = new ThermistorSensor(
  //   ...
  //   (AnalogReader*)&adsReader,   // Explicitly cast to AnalogReader*
  //   ?,                         // Assign ADS1115 Channel (e.g., 1)
  //   ...,
  //   kart_common_ValueType_INT16,
  //   3300                       // Divider supply voltage (mV)
  // );
  
  // motorTempSensor = new ThermistorSensor(
  //   kart_common_ComponentType_MOTORS,
  //   kart_motors_MotorComponentId_MOTOR_LEFT_REAR,
  //   kart_motors_MotorCommandId_TEMPERATURE,
  //   (AnalogReader*)&adsReader,   // Explicitly cast to AnalogReader*
  //   0,                         // Use ADS1115 Channel 0 for Motor Temp
  //   1000,                      // Update interval (ms)
  //   SERIES_RESISTOR,
  //   THERMISTOR_NOMINAL,
  //   TEMPERATURE_NOMINAL,
  //   B_COEFFICIENT,
  //   kart_common_ValueType_INT16, // Using INT16
  //   3300                       // Divider supply voltage (mV)
  // );

  // --- Initialize Current Sensor using ADS1115 Reader --- 
  // shuntCurrentSensor = new DifferentialCurrentSensor(
  //   kart_common_ComponentType_BATTERIES, // Logically belongs to Batteries
  //   0, // Assuming Component ID 0 for main battery pack current
  //   kart_batteries_BatteryCommandId_CURRENT, // Command ID for Current
  //   (AnalogReader*)&adsReader, // Pass the ADS1115 reader instance
  //   SHUNT_ADS_CH_P,            // Positive ADS channel from Config.h
  //   SHUNT_ADS_CH_N,            // Negative ADS channel from Config.h
  //   CURRENT_UPDATE_INTERVAL,   // Update interval from Config.h
  //   SHUNT_RESISTANCE_MOHM    // Shunt resistance from Config.h
  // );

  // Initialize RPM sensor with the sensor framework
  motorRpmSensor = new KunrayHallRpmSensor(
    kart_motors_MotorComponentId_MOTOR_LEFT_REAR,
    500
  );
  
  // Register the sensors with the registry
  // begin() is called inside registerSensor (which now attaches the single RPM interrupt)
  // sensorRegistry.registerSensor(motorRpmSensor);
  // sensorRegistry.registerSensor(batteryTempSensor);
  // // sensorRegistry.registerSensor(controllerTempSensor); // Register if implemented
  // sensorRegistry.registerSensor(motorTempSensor);
  // sensorRegistry.registerSensor(shuntCurrentSensor);
  
#if DEBUG_MODE
  Serial.println(F("Motor controller initialized"));
#endif
}

void loop() {
  // Process CAN messages
  canInterface.process(); // <--- Re-enabled

  // Process all sensors using the SensorRegistry
  sensorRegistry.process(); // <--- Re-enabled (But I2C sensors are still commented out in setup)

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
    String commandUpper = command; // Keep original case for value parsing
    commandUpper.toUpperCase();
    
    // Command format: "T:value" for throttle where value is 0-100
    if (commandUpper.startsWith("T:")) {
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
    else if (commandUpper.startsWith("D:")) {
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
    else if (commandUpper.startsWith("S:")) {
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
    else if (commandUpper.startsWith("B:")) {
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
    else if (commandUpper == "STOP") {
      emergencyStop();
      Serial.println(F("EMERGENCY STOP executed"));
      
      // Send acknowledgement
      Serial.println(F("ACK: EMERGENCY_STOP command processed"));
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, kart_motors_MotorCommandId_EMERGENCY, 
                               kart_common_ValueType_UINT8, kart_motors_MotorEmergencyValue_STOP);
    }
    else if (commandUpper == "SHUTDOWN") {
      emergencyShutdown();
      Serial.println(F("EMERGENCY SHUTDOWN executed"));
      
      // Send acknowledgement
      Serial.println(F("ACK: EMERGENCY_SHUTDOWN command processed"));
      canInterface.sendMessage(kart_common_MessageType_STATUS, kart_common_ComponentType_MOTORS, 
                               kart_motors_MotorComponentId_MOTOR_LEFT_REAR, kart_motors_MotorCommandId_EMERGENCY, 
                               kart_common_ValueType_UINT8, kart_motors_MotorEmergencyValue_SHUTDOWN);
    }
    // Help command
    else if (commandUpper == "HELP") {
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
    else if (commandUpper == "STATUS") {
      Serial.println(F("--- Motor Status ---"));
      Serial.print(F("RPM (from Sensor): "));
      Serial.println(motorRpmSensor ? motorRpmSensor->getValue() : -1); // Use getValue()
      Serial.print(F("Throttle: "));
      Serial.println(currentThrottle);
      Serial.print(F("Direction: "));
      Serial.println(currentDirection);
      Serial.print(F("Brake mode: "));
      Serial.println(currentBrakeMode);
      Serial.print(F("Speed mode: "));
      Serial.println(currentSpeedMode);
      // Low/High brake pins reflect the state set by setBrake()
      Serial.print(F("High Brake Pin State: "));
      Serial.println(digitalRead(HIGH_BRAKE_PIN));


      // Get temperature values from sensors (already processed by registry.process)
      // getValue() returns the scaled int32_t value (tenths of degrees for thermistors)
      // Declare variables here
      int32_t batTempTenths = batteryTempSensor ? batteryTempSensor->getValue() : -9999;
      int32_t motTempTenths = motorTempSensor ? motorTempSensor->getValue() : -9999;
      int32_t current_mA = shuntCurrentSensor ? shuntCurrentSensor->getValue() : -32768;

      Serial.print(F("Battery Temp (Sensor): "));
      // Corrected printf format string
      if (batTempTenths != -9999) Serial.printf("%.1f C\n", (float)batTempTenths / 10.0f); else Serial.println("N/A");

      Serial.print(F("Motor Temp (Sensor): "));
      // Corrected printf format string
       if (motTempTenths != -9999) Serial.printf("%.1f C\n", (float)motTempTenths / 10.0f); else Serial.println("N/A");

      Serial.print(F("Shunt Current (Sensor): "));
      if (current_mA != -32768) Serial.printf("%d mA\n", current_mA); else Serial.println("N/A");

      // Hall state/count are internal to Kunray sensor now
      // Serial.print(F("Hall pulse count: "));
      // Serial.println(hallPulseCount);
      Serial.print(F("Current status: "));
      Serial.println(currentStatus);
       Serial.println(F("--------------------"));


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