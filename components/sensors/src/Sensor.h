#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "../../common/lib/CrossPlatformCAN/ProtobufCANInterface.h"

// Sensor value union to support different types
union SensorValue {
  bool    bool_value;
  int8_t  int8_value;
  uint8_t uint8_value;
  int16_t int16_value;
  uint16_t uint16_value;
  int32_t int24_value;   // 24-bit value stored in 32-bit
  uint32_t uint24_value; // 24-bit value stored in 32-bit
  float   float_value;
};

/**
 * Base Sensor class
 * Abstract class for all sensor types
 */
class Sensor {
public:
  /**
   * Constructor
   * @param locationId Location ID for this sensor (used as component_id)
   * @param updateInterval Update interval in milliseconds
   */
  Sensor(uint8_t locationId, uint16_t updateInterval = 1000) :
    _locationId(locationId),
    _updateInterval(updateInterval),
    _lastUpdateTime(0),
    _enabled(true) {}
  
  /**
   * Virtual destructor
   */
  virtual ~Sensor() {}
  
  /**
   * Initialize the sensor
   * @return true if initialization was successful
   */
  virtual bool begin() = 0;
  
  /**
   * Read the sensor value
   * @return SensorValue union containing the read value
   */
  virtual SensorValue read() = 0;
  
  /**
   * Get the value type as defined in CrossPlatformCAN's ValueType enum
   */
  virtual uint8_t getValueType() const = 0;
  
  /**
   * Get the sensor command ID (sensor type from sensors.proto SensorCommandId)
   * This is the type of sensor (TEMPERATURE, RPM, etc.)
   */
  virtual uint8_t getSensorCommandId() const = 0;
  
  /**
   * Process sensor (read and transmit if needed)
   * Should be called regularly in the main loop
   * @param canInterface The CAN interface to transmit readings
   * @param forceSend Force sending the reading even if not due
   * @return true if reading was taken and sent
   */
  bool process(ProtobufCANInterface& canInterface, bool forceSend = false) {
    if (!_enabled) return false;
    
    unsigned long currentTime = millis();
    if (forceSend || (currentTime - _lastUpdateTime >= _updateInterval)) {
      // Read sensor
      SensorValue value = read();
      
      // Send over CAN
      bool result = canInterface.sendMessage(
        kart_common_MessageType_STATUS,        // Message type: STATUS
        kart_common_ComponentType_SENSORS,     // Component type: SENSORS 
        _locationId,                           // Component ID: sensor location
        getSensorCommandId(),                  // Command ID: sensor type
        (kart_common_ValueType)getValueType(), // Value type
        getValue(value)                        // Value
      );
      
      _lastUpdateTime = currentTime;
      return result;
    }
    
    return false;
  }
  
  /**
   * Enable or disable the sensor
   * @param enable true to enable, false to disable
   */
  void setEnabled(bool enable) {
    _enabled = enable;
  }
  
  /**
   * Check if the sensor is enabled
   * @return true if enabled
   */
  bool isEnabled() const {
    return _enabled;
  }
  
  /**
   * Get the sensor location ID
   * @return The location ID
   */
  uint8_t getLocationId() const {
    return _locationId;
  }
  
  /**
   * Set the update interval
   * @param interval Update interval in milliseconds
   */
  void setUpdateInterval(uint16_t interval) {
    _updateInterval = interval;
  }
  
  /**
   * Get the update interval
   * @return Update interval in milliseconds
   */
  uint16_t getUpdateInterval() const {
    return _updateInterval;
  }
  
protected:
  uint8_t _locationId;               // Sensor location ID (component_id)
  uint16_t _updateInterval;          // Update interval in ms
  unsigned long _lastUpdateTime;     // Last update timestamp
  bool _enabled;                     // Enabled flag
  
  /**
   * Extract the proper value from SensorValue based on type
   * @param value SensorValue union
   * @return Value as int32_t for CAN transmission
   */
  int32_t getValue(const SensorValue& value) const {
    switch (getValueType()) {
      case kart_common_ValueType_BOOLEAN:
        return value.bool_value ? 1 : 0;
      case kart_common_ValueType_INT8:
        return value.int8_value;
      case kart_common_ValueType_UINT8:
        return value.uint8_value;
      case kart_common_ValueType_INT16:
        return value.int16_value;
      case kart_common_ValueType_UINT16:
        return value.uint16_value;
      case kart_common_ValueType_INT24:
        return value.int24_value;
      case kart_common_ValueType_UINT24:
        return value.uint24_value;
      default:
        return 0;
    }
  }
};

#endif // SENSOR_H 