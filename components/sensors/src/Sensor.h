#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "../../common/lib/CrossPlatformCAN/ProtobufCANInterface.h"
#include "SensorProtocol.h"

// Forward declaration
class SensorRegistry;

/**
 * SensorValue - Union to store different value types
 */
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
 * Sensor - Base class for all sensors
 */
class Sensor {
public:
  /**
   * Constructor
   * @param id Unique sensor ID within a component
   * @param updateInterval Time in ms between sensor readings
   */
  Sensor(uint8_t id, uint16_t updateInterval = 1000) : 
    _id(id), _updateInterval(updateInterval), _lastUpdateTime(0), _enabled(true) {}
  
  /**
   * Destructor
   */
  virtual ~Sensor() {}
    
  /**
   * Initialize the sensor
   * @return true if initialization was successful
   */
  virtual bool begin() = 0;
  
  /**
   * Read the sensor value
   * @return SensorValue containing the reading
   */
  virtual SensorValue read() = 0;
  
  /**
   * Get the value type for CAN protocol
   * @return Value type from kart_common_ValueType enum
   */
  virtual uint8_t getValueType() const = 0;
  
  /**
   * Get sensor ID
   * @return Sensor ID
   */
  uint8_t getId() const { return _id; }
  
  /**
   * Get update interval
   * @return Update interval in ms
   */
  uint16_t getUpdateInterval() const { return _updateInterval; }
  
  /**
   * Set update interval
   * @param updateInterval New interval in ms
   */
  void setUpdateInterval(uint16_t updateInterval) { _updateInterval = updateInterval; }
  
  /**
   * Process the sensor and transmit if needed
   * @param can CAN interface to use for transmission
   * @param componentType Component type from kart_common_ComponentType
   * @param forceSend Force sending the reading even if not due
   * @return true if sensor was read and transmitted
   */
  bool process(ProtobufCANInterface& can, kart_common_ComponentType componentType, bool forceSend = false) {
    if (!_enabled) return false;
    
    unsigned long currentTime = millis();
    if (forceSend || (currentTime - _lastUpdateTime >= _updateInterval)) {
      SensorValue value = read();
      _lastUpdateTime = currentTime;
      
      // Send via CAN
      bool result = can.sendMessage(
        kart_common_MessageType_STATUS,
        componentType,
        getSensorType(),
        _id,
        (kart_common_ValueType)getValueType(),
        getValue(value)
      );
      
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
   * Get the sensor component type (from SensorProtocol::SensorComponentId)
   * Default implementation returns 0 (TEMPERATURE)
   */
  virtual uint8_t getSensorType() const {
    return SensorProtocol::SensorComponentId::TEMPERATURE;
  }
  
protected:
  uint8_t _id;                 // Sensor ID within component
  uint16_t _updateInterval;    // Update interval in ms
  unsigned long _lastUpdateTime;   // Last update timestamp
  bool _enabled;                   // Enabled flag
  
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