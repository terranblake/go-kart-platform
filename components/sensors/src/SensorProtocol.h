/**
 * SensorProtocol.h - Sensor protocol definitions
 * 
 * Contains constants and enumerations for the sensor component protocol.
 */

#ifndef SENSOR_PROTOCOL_H
#define SENSOR_PROTOCOL_H

#include <Arduino.h>
#include "../../common/lib/CrossPlatformCAN/include/common.pb.h"

namespace SensorProtocol {

// *** SENSOR COMPONENT IDS ***
// Used in component_id field when component_type is SENSORS
enum SensorComponentId {
  TEMPERATURE = 0,          // Temperature sensors
  RPM = 1,                  // RPM sensors
  VOLTAGE = 2,              // Voltage sensors
  CURRENT = 3,              // Current sensors
  PRESSURE = 4,             // Pressure sensors
  POSITION = 5,             // Position sensors
  ACCELERATION = 6,         // Accelerometer
  GYROSCOPE = 7,            // Gyroscope
  SAFETY = 8,               // Safety-critical sensors
  ALL = 255                 // All sensors (broadcast)
};

// *** SENSOR COMMAND IDS ***
// Used in command_id field when component_type is SENSORS
enum SensorCommandId {
  READ = 0,                 // Read current sensor value
  CONFIGURE = 1,            // Configure sensor parameters
  CALIBRATE = 2,            // Calibrate sensor
  INTERVAL = 3,             // Set update interval
  THRESHOLD = 4,            // Set alert threshold
  RESET = 5,                // Reset sensor
  STATUS = 6,               // Get sensor status
  ALERT = 7,                // Alert notification (threshold exceeded)
  ENABLE = 8                // Enable/disable sensor
};

// *** SENSOR STATUS VALUES ***
// Used in value field when command_id is STATUS
enum SensorStatusValue {
  OK = 0,                   // Normal operation
  ERROR_CONNECTION = 1,     // Connection error
  ERROR_TIMEOUT = 2,        // Timeout error
  ERROR_RANGE = 3,          // Out of range error
  ERROR_CALIBRATION = 4,    // Calibration error
  DISABLED = 5,             // Sensor is disabled
  INITIALIZING = 6,         // Sensor is initializing
  ERROR_GENERAL = 7         // General error
};

// *** TEMPERATURE SENSOR TYPES ***
// Used in value field for temperature-specific commands
enum TemperatureSensorType {
  MOTOR = 0,                // Motor temperature
  CONTROLLER = 1,           // Controller temperature
  BATTERY = 2,              // Battery temperature
  AMBIENT = 3,              // Ambient temperature
  COOLANT = 4               // Coolant temperature
};

// *** RPM SENSOR SOURCES ***
// Used in value field for RPM-specific commands
enum RpmSensorSource {
  HALL_EFFECT = 0,          // Hall effect sensor
  OPTICAL = 1,              // Optical sensor
  CALCULATED = 2            // Calculated from other values
};

// *** CALIBRATION COMMANDS ***
// Used in value field when command_id is CALIBRATE
enum SensorCalibrateCommand {
  CAL_START = 0,            // Start calibration
  CAL_FINISH = 1,           // Finish calibration
  CAL_RESET = 2,            // Reset to default calibration
  CAL_ZERO = 3,             // Set current reading as zero
  CAL_REFERENCE = 4         // Set reference point
};

// *** THRESHOLD TYPES ***
// Used in value field when command_id is THRESHOLD
enum SensorThresholdType {
  THRESHOLD_LOW = 0,        // Low threshold
  THRESHOLD_HIGH = 1,       // High threshold
  THRESHOLD_RATE = 2,       // Rate of change threshold
  THRESHOLD_DELTA = 3       // Delta threshold
};

// *** HELPER FUNCTIONS ***

/**
 * Send a sensor reading over the CAN bus
 * 
 * @param can The CAN interface to use
 * @param sensor_type The type of sensor (SensorComponentId)
 * @param sensor_id The ID of the specific sensor instance
 * @param value The sensor reading
 * @param value_type The type of the value (kart_common_ValueType)
 * @return true if the message was sent successfully
 */
inline bool sendSensorReading(
    ProtobufCANInterface& can,
    uint8_t sensor_type,
    uint8_t sensor_id,
    int32_t value,
    kart_common_ValueType value_type) {
  
  return can.sendMessage(
    kart_common_MessageType_STATUS,    // Message type: STATUS
    kart_common_ComponentType_SENSORS,  // Component type: SENSORS
    sensor_type,                       // Component ID: sensor type (TEMPERATURE, RPM, etc)
    sensor_id,                         // Command ID: specific sensor instance ID
    value_type,                        // Value type: depends on sensor (UINT16, INT16, etc)
    value                              // Actual sensor reading
  );
}

/**
 * Send a sensor alert over the CAN bus
 * 
 * @param can The CAN interface to use
 * @param sensor_type The type of sensor (SensorComponentId)
 * @param sensor_id The ID of the specific sensor instance
 * @param alert_type The type of alert (usually related to thresholds)
 * @return true if the message was sent successfully
 */
inline bool sendSensorAlert(
    ProtobufCANInterface& can,
    uint8_t sensor_type,
    uint8_t sensor_id,
    uint8_t alert_type) {
  
  return can.sendMessage(
    kart_common_MessageType_STATUS,    // Message type: STATUS
    kart_common_ComponentType_SENSORS,  // Component type: SENSORS
    sensor_type,                       // Component ID: sensor type
    SensorCommandId::ALERT,            // Command ID: ALERT
    kart_common_ValueType_UINT8,       // Value type: UINT8
    alert_type                         // Alert type value
  );
}

/**
 * Send a sensor status over the CAN bus
 * 
 * @param can The CAN interface to use
 * @param sensor_type The type of sensor (SensorComponentId)
 * @param sensor_id The ID of the specific sensor instance
 * @param status The status of the sensor (SensorStatusValue)
 * @return true if the message was sent successfully
 */
inline bool sendSensorStatus(
    ProtobufCANInterface& can,
    uint8_t sensor_type,
    uint8_t sensor_id,
    uint8_t status) {
  
  return can.sendMessage(
    kart_common_MessageType_STATUS,    // Message type: STATUS
    kart_common_ComponentType_SENSORS,  // Component type: SENSORS
    sensor_type,                       // Component ID: sensor type
    SensorCommandId::STATUS,           // Command ID: STATUS
    kart_common_ValueType_UINT8,       // Value type: UINT8
    status                             // Status value
  );
}

} // namespace SensorProtocol

#endif // SENSOR_PROTOCOL_H 