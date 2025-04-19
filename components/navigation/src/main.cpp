#include "../include/Config.h"

#include <Arduino.h>
#include <Wire.h>
#include "ProtobufCANInterface.h"
#include "Sensor.h"
#include "SensorRegistry.h"
#include "ATGM336HSensor.h"

// Include Protobuf definitions
#include "common.pb.h"
#include "navigation.pb.h"

// Include Sensor Variants
#include "AHT21Sensor.h"
#include "GY521Sensor.h"

// Global CAN Interface
ProtobufCANInterface canInterface(NODE_ID, CAN_CS_PIN, CAN_INT_PIN);

// Global Sensor Registry
SensorRegistry sensorRegistry(canInterface, kart_common_ComponentType_NAVIGATION, NODE_ID);

// Sensor object pointers
// Environment Sensor (AHT21)
AHT21Sensor* tempAmbientSensor = nullptr;
AHT21Sensor* humiditySensor = nullptr;

// IMU Sensor (GY-521 / MPU-6050)
GY521Sensor* accelXSensor = nullptr;
GY521Sensor* accelYSensor = nullptr;
GY521Sensor* accelZSensor = nullptr;
GY521Sensor* gyroXSensor = nullptr;
GY521Sensor* gyroYSensor = nullptr;
GY521Sensor* gyroZSensor = nullptr;
GY521Sensor* imuTempSensor = nullptr;

// GPS Sensor (ATGM336H)
TinyGPSPlus sharedGps; // Create the single shared TinyGPSPlus object
ATGM336HSensor* latitudeSensor = nullptr;
ATGM336HSensor* longitudeSensor = nullptr;
ATGM336HSensor* altitudeSensor = nullptr;
ATGM336HSensor* speedSensor = nullptr;
ATGM336HSensor* courseSensor = nullptr;
ATGM336HSensor* satellitesSensor = nullptr;
ATGM336HSensor* hdopSensor = nullptr;
ATGM336HSensor* gpsFixSensor = nullptr; // This one will also handle processSerial()

void setup() {
#if DEBUG_MODE
  Serial.begin(115200);
  while (!Serial) { delay(10); } // Wait for Serial connection
  Serial.println(F("Navigation Component Initializing..."));
#endif

  // Initialize hardware pins (I2C, potentially others if needed)
  setupPins();

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

  // Initialize Sensors
  setupSensors();

  // Register command handlers for configuration commands
#if DEBUG_MODE
    Serial.println(F("Registering Command Handlers..."));
#endif
  // --- IMU Config Handlers ---
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_IMU_PRIMARY, 
                               kart_navigation_NavigationCommandId_ACCELEROMETER_RANGE, 
                               handleImuConfigCommand);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_IMU_PRIMARY, 
                               kart_navigation_NavigationCommandId_GYROSCOPE_RANGE, 
                               handleImuConfigCommand); 
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_IMU_PRIMARY, 
                               kart_navigation_NavigationCommandId_FILTER_BANDWIDTH, 
                               handleImuConfigCommand);
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_IMU_PRIMARY, 
                               kart_navigation_NavigationCommandId_TRIGGER_CALIBRATION, 
                               handleImuConfigCommand);

  // --- GPS Config Handlers --- 
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_GPS_PRIMARY, 
                               kart_navigation_NavigationCommandId_GPS_UPDATE_RATE, 
                               handleGpsConfigCommand);                           
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_GPS_PRIMARY, 
                               kart_navigation_NavigationCommandId_GNSS_CONSTELLATION, 
                               handleGpsConfigCommand);   
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_GPS_PRIMARY, 
                               kart_navigation_NavigationCommandId_NMEA_OUTPUT_CONFIG, 
                               handleGpsConfigCommand);   
  canInterface.registerHandler(kart_common_MessageType_COMMAND, 
                               kart_common_ComponentType_NAVIGATION, 
                               kart_navigation_NavigationComponentId_GPS_PRIMARY, 
                               kart_navigation_NavigationCommandId_STATIC_NAVIGATION_MODE, 
                               handleGpsConfigCommand);                                                            

#if DEBUG_MODE
  Serial.println(F("Navigation Component Setup Complete"));
#endif
}

void loop() {
  // Process incoming/outgoing CAN messages
  canInterface.process();

  // Process GPS Serial Data (call on one instance)
  if (gpsFixSensor) { // Ensure GPS sensor is initialized
      gpsFixSensor->processSerial();
  }

  // Process all registered sensors (updates values and sends CAN messages if needed)
  sensorRegistry.process();

  // Parse serial commands in DEBUG_MODE
#if DEBUG_MODE == 1
  parseSerialCommands();
#endif

  // Yield control briefly
  delay(1);
}

void setupPins() {
  // Initialize I2C
  // Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN); // Wire.begin() is often called by library .begin()
  // If specific pin setup is needed beyond library defaults, add here.
#if DEBUG_MODE
    Serial.println(F("Pin setup complete (I2C pins managed by libraries)"));
#endif
}

void setupSensors() {
#if DEBUG_MODE
  Serial.println(F("Initializing Sensors..."));
#endif

  // --- Initialize AHT21 Sensor Instances --- (Using default I2C &Wire)
  tempAmbientSensor = new AHT21Sensor(
    kart_common_ComponentType_NAVIGATION,
    kart_navigation_NavigationComponentId_ENVIRONMENT_PRIMARY,
    kart_navigation_NavigationCommandId_TEMPERATURE_AMBIENT,
    1000 // Update interval ms
  );
  humiditySensor = new AHT21Sensor(
    kart_common_ComponentType_NAVIGATION,
    kart_navigation_NavigationComponentId_ENVIRONMENT_PRIMARY,
    kart_navigation_NavigationCommandId_HUMIDITY_RELATIVE,
    1000
  );
  // Register AHT21
  sensorRegistry.registerSensor(tempAmbientSensor);
  sensorRegistry.registerSensor(humiditySensor);

  // --- Initialize GY521 Sensor Instances --- (Using default I2C &Wire)
  accelXSensor = new GY521Sensor(
    kart_common_ComponentType_NAVIGATION,
    kart_navigation_NavigationComponentId_IMU_PRIMARY,
    kart_navigation_NavigationCommandId_ACCEL_X,
    100 // Update interval ms
  );
  accelYSensor = new GY521Sensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_ACCEL_Y, 100);
  accelZSensor = new GY521Sensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_ACCEL_Z, 100);
  gyroXSensor = new GY521Sensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_GYRO_X, 100);
  gyroYSensor = new GY521Sensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_GYRO_Y, 100);
  gyroZSensor = new GY521Sensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_GYRO_Z, 100);
  imuTempSensor = new GY521Sensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_IMU_TEMPERATURE, 1000);
  // Register GY521
  sensorRegistry.registerSensor(accelXSensor);
  sensorRegistry.registerSensor(accelYSensor);
  sensorRegistry.registerSensor(accelZSensor);
  sensorRegistry.registerSensor(gyroXSensor);
  sensorRegistry.registerSensor(gyroYSensor);
  sensorRegistry.registerSensor(gyroZSensor);
  sensorRegistry.registerSensor(imuTempSensor);

  // --- Initialize ATGM336H Sensor Instances --- (Using sharedGps)
  latitudeSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_LATITUDE, &Serial2, &sharedGps, 1000);
  longitudeSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_LONGITUDE, &Serial2, &sharedGps, 1000);
  altitudeSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_ALTITUDE, &Serial2, &sharedGps, 1000);
  speedSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_SPEED, &Serial2, &sharedGps, 1000);
  courseSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_COURSE, &Serial2, &sharedGps, 1000);
  satellitesSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_SATELLITES, &Serial2, &sharedGps, 1000);
  hdopSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_HDOP, &Serial2, &sharedGps, 1000);
  gpsFixSensor = new ATGM336HSensor(kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_GPS_FIX_STATUS, &Serial2, &sharedGps, 1000);
  // Register ATGM336H
  sensorRegistry.registerSensor(latitudeSensor);
  sensorRegistry.registerSensor(longitudeSensor);
  sensorRegistry.registerSensor(altitudeSensor);
  sensorRegistry.registerSensor(speedSensor);
  sensorRegistry.registerSensor(courseSensor);
  sensorRegistry.registerSensor(satellitesSensor);
  sensorRegistry.registerSensor(hdopSensor);
  sensorRegistry.registerSensor(gpsFixSensor);

#if DEBUG_MODE
  Serial.println(F("All sensors initialized and registered."));
#endif
}

// --- Command Handler Implementations ---

/**
 * Handles configuration commands targeting the IMU.
 */
void handleImuConfigCommand(kart_common_MessageType msg_type,
                           kart_common_ComponentType comp_type,
                           uint8_t component_id,
                           uint8_t command_id,
                           kart_common_ValueType value_type,
                           int32_t value) {
    bool success = false;
    // Use any initialized GY521 sensor pointer, as they share the hardware
    if (!accelXSensor) { 
      #if DEBUG_MODE
        Serial.println("IMU Config Handler: Sensor not initialized!");
      #endif
      // NACK will be handled elsewhere
      return; 
    }

    #if DEBUG_MODE
      Serial.printf("IMU Config Received: CompID=%d, CmdID=%d, Val=%ld\n", component_id, command_id, (long int)value);
    #endif

    switch (command_id) {
        case kart_navigation_NavigationCommandId_ACCELEROMETER_RANGE:
            success = accelXSensor->setAccelerometerRange((uint8_t)value);
            break;
        case kart_navigation_NavigationCommandId_GYROSCOPE_RANGE:
             success = accelXSensor->setGyroscopeRange((uint8_t)value);
            break;
        case kart_navigation_NavigationCommandId_FILTER_BANDWIDTH:
             success = accelXSensor->setFilterBandwidth((uint8_t)value);
            break;
        case kart_navigation_NavigationCommandId_TRIGGER_CALIBRATION:
             success = accelXSensor->triggerCalibration(); // Value might be ignored
            break;
        default:
            #if DEBUG_MODE
              Serial.printf("IMU Config Handler: Unknown Command ID: %d\n", command_id);
            #endif
            success = false;
            break;
    }

    // ACK/NACK sending is assumed to be handled by ProtobufCANInterface or calling code
    // if (success) {
    //   canInterface.sendAck(comp_type, component_id, command_id);
    // } else {
    //   canInterface.sendNack(comp_type, component_id, command_id);
    // }
}

/**
 * Handles configuration commands targeting the GPS.
 */
void handleGpsConfigCommand(kart_common_MessageType msg_type,
                          kart_common_ComponentType comp_type,
                          uint8_t component_id,
                          uint8_t command_id,
                          kart_common_ValueType value_type,
                          int32_t value) {
    bool success = false;
    // Use any initialized ATGM336H sensor pointer
    if (!gpsFixSensor) { 
      #if DEBUG_MODE
        Serial.println("GPS Config Handler: Sensor not initialized!");
      #endif
       // NACK will be handled elsewhere
      return; 
    }

    #if DEBUG_MODE
      Serial.printf("GPS Config Received: CompID=%d, CmdID=%d, Val=%ld\n", component_id, command_id, (long int)value);
    #endif

     switch (command_id) {
        case kart_navigation_NavigationCommandId_GPS_UPDATE_RATE:
            success = gpsFixSensor->setUpdateRate((uint8_t)value);
            break;
        case kart_navigation_NavigationCommandId_GNSS_CONSTELLATION:
            success = gpsFixSensor->setGnssConstellation((uint8_t)value);
            break;
        case kart_navigation_NavigationCommandId_NMEA_OUTPUT_CONFIG:
            success = gpsFixSensor->setNmeaOutput((uint8_t)value);
            break;
        case kart_navigation_NavigationCommandId_STATIC_NAVIGATION_MODE:
            success = gpsFixSensor->setStaticNavigation((uint8_t)value);
            break;
        default:
             #if DEBUG_MODE
              Serial.printf("GPS Config Handler: Unknown Command ID: %d\n", command_id);
            #endif
            success = false;
            break;
    }

    // ACK/NACK sending is assumed to be handled by ProtobufCANInterface or calling code
    // if (success) {
    //   canInterface.sendAck(comp_type, component_id, command_id);
    // } else {
    //   canInterface.sendNack(comp_type, component_id, command_id);
    // }
}

#if DEBUG_MODE == 1
// Serial Command Parsing for Debugging
void parseSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    String commandUpper = command; // Keep original case for value parsing
    commandUpper.toUpperCase();

    // STATUS command
    if (commandUpper == "STATUS") {
      Serial.println(F("--- Navigation Status ---"));
      // Manually trigger reads and print for debug
      if (tempAmbientSensor) Serial.printf("Ambient Temp: %.1f C\n", tempAmbientSensor->getLastTemperatureC());
      if (humiditySensor) Serial.printf("Humidity: %.1f %%\n", humiditySensor->getLastHumidity());
      if (accelXSensor) Serial.printf("Accel X: %.2f m/s^2\n", accelXSensor->getLastAccelX());
      if (accelYSensor) Serial.printf("Accel Y: %.2f m/s^2\n", accelYSensor->getLastAccelY());
      if (accelZSensor) Serial.printf("Accel Z: %.2f m/s^2\n", accelZSensor->getLastAccelZ());
      if (gyroXSensor) Serial.printf("Gyro X: %.2f rad/s\n", gyroXSensor->getLastGyroX());
      if (gyroYSensor) Serial.printf("Gyro Y: %.2f rad/s\n", gyroYSensor->getLastGyroY());
      if (gyroZSensor) Serial.printf("Gyro Z: %.2f rad/s\n", gyroZSensor->getLastGyroZ());
      if (imuTempSensor) Serial.printf("IMU Temp: %.1f C\n", imuTempSensor->getLastChipTempC());
      if (latitudeSensor) {
          Serial.printf("Latitude: %f deg\n", sharedGps.location.lat());
          Serial.printf("Longitude: %f deg\n", sharedGps.location.lng());
          Serial.printf("Altitude: %.1f m\n", sharedGps.altitude.meters());
          Serial.printf("Speed: %.1f km/h\n", sharedGps.speed.kmph());
          Serial.printf("Course: %.1f deg\n", sharedGps.course.deg());
          Serial.printf("Satellites: %d\n", sharedGps.satellites.value());
          Serial.printf("HDOP: %.2f\n", (float)sharedGps.hdop.value() / 100.0);
          Serial.printf("Fix Valid: %s\n", sharedGps.location.isValid() ? "Yes" : "No");
          Serial.printf("Fix Age: %u ms\n", sharedGps.location.age());
      } else {
          Serial.println("GPS Sensor not initialized for status.");
      }
      Serial.println(F("-------------------------"));
      Serial.println(F("ACK: STATUS command processed"));
    }
    // HELP command
    else if (commandUpper == "HELP") {
      Serial.println(F("--- Navigation Debug Commands ---"));
      Serial.println(F("STATUS - Show current sensor readings."));
      Serial.println(F("HELP   - Show this help message."));
      Serial.println(F("--- IMU Config ---"));
      Serial.println(F("AR:val - Set Accel Range (0=2G, 1=4G, 2=8G, 3=16G)"));
      Serial.println(F("GR:val - Set Gyro Range (0=250, 1=500, 2=1000, 3=2000 deg/s)"));
      Serial.println(F("FB:val - Set Filter BW (0=260, 1=184, 2=94, 3=44, 4=21, 5=10, 6=5 Hz)"));
      Serial.println(F("CAL    - Trigger IMU Calibration (Placeholder)"));
      Serial.println(F("--- GPS Config ---"));
      Serial.println(F("UR:val - Set GPS Update Rate (1, 5, 10 Hz)"));
      Serial.println(F("GC:val - Set GNSS Constellation (0=GPS, 1=GPS+GLO, ... - Placeholder)"));
      Serial.println(F("NO:val - Set NMEA Output (0=Default, 1=Essential, ... - Placeholder)"));
      Serial.println(F("SN:val - Set Static Nav Mode (0=Off, 1=On)"));
      Serial.println(F("-----------------------------"));
      Serial.println(F("ACK: HELP command processed"));
    }
    // --- IMU Config Commands ---
    else if (commandUpper.startsWith("AR:")) {
      int val = command.substring(3).toInt();
      if (val >= 0 && val <= 3) { // Validate against AccelerometerRangeValue
        handleImuConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_ACCELEROMETER_RANGE, kart_common_ValueType_UINT8, val);
        Serial.printf("ACK: ACCEL_RANGE set to %d\n", val);
      } else {
        Serial.println(F("NACK: Invalid Accel Range value (0-3)."));
      }
    }
    else if (commandUpper.startsWith("GR:")) {
       int val = command.substring(3).toInt();
      if (val >= 0 && val <= 3) { // Validate against GyroscopeRangeValue
        handleImuConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_GYROSCOPE_RANGE, kart_common_ValueType_UINT8, val);
        Serial.printf("ACK: GYRO_RANGE set to %d\n", val);
      } else {
        Serial.println(F("NACK: Invalid Gyro Range value (0-3)."));
      }
    }
    else if (commandUpper.startsWith("FB:")) {
      int val = command.substring(3).toInt();
      if (val >= 0 && val <= 6) { // Validate against FilterBandwidthValue
        handleImuConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_FILTER_BANDWIDTH, kart_common_ValueType_UINT8, val);
        Serial.printf("ACK: FILTER_BANDWIDTH set to %d\n", val);
      } else {
        Serial.println(F("NACK: Invalid Filter BW value (0-6)."));
      }
    }
    else if (commandUpper == "CAL") {
       handleImuConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_IMU_PRIMARY, kart_navigation_NavigationCommandId_TRIGGER_CALIBRATION, kart_common_ValueType_UINT8, 0); // Value ignored
       Serial.println(F("ACK: TRIGGER_CALIBRATION command sent (Placeholder)"));
    }
    // --- GPS Config Commands ---
    else if (commandUpper.startsWith("UR:")) {
      int val = command.substring(3).toInt();
       if (val == 1 || val == 5 || val == 10) { // Validate against GpsUpdateRateValue
         handleGpsConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_GPS_UPDATE_RATE, kart_common_ValueType_UINT8, val);
         Serial.printf("ACK: GPS_UPDATE_RATE set to %d Hz\n", val);
       } else {
         Serial.println(F("NACK: Invalid GPS Update Rate value (1, 5, 10)."));
       }
    }
    else if (commandUpper.startsWith("GC:")) { // Placeholder
      int val = command.substring(3).toInt();
      handleGpsConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_GNSS_CONSTELLATION, kart_common_ValueType_UINT8, val);
      Serial.printf("ACK: GNSS_CONSTELLATION set to %d (Placeholder)\n", val);
    }
     else if (commandUpper.startsWith("NO:")) { // Placeholder
      int val = command.substring(3).toInt();
      handleGpsConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_NMEA_OUTPUT_CONFIG, kart_common_ValueType_UINT8, val);
      Serial.printf("ACK: NMEA_OUTPUT_CONFIG set to %d (Placeholder)\n", val);
    }
    else if (commandUpper.startsWith("SN:")) {
       int val = command.substring(3).toInt();
       if (val == 0 || val == 1) { // Validate against StaticNavigationValue
         handleGpsConfigCommand(kart_common_MessageType_COMMAND, kart_common_ComponentType_NAVIGATION, kart_navigation_NavigationComponentId_GPS_PRIMARY, kart_navigation_NavigationCommandId_STATIC_NAVIGATION_MODE, kart_common_ValueType_UINT8, val);
         Serial.printf("ACK: STATIC_NAVIGATION_MODE set to %d\n", val);
       } else {
          Serial.println(F("NACK: Invalid Static Nav value (0 or 1)."));
       }
    }
    // Unknown command
    else if (command.length() > 0) {
      Serial.print(F("Unknown command: "));
      Serial.println(command);
      Serial.println(F("ACK: UNKNOWN command rejected"));
    }
  }
}
#endif // DEBUG_MODE
