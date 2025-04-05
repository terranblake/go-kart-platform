#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "ProtobufCANInterface.h"

// Forward declaration
class SensorRegistry;

/**
 * SensorValue - Union to store different value types
 */
union SensorValue {
  uint32_t uint_value;
  int32_t int_value;
  float float_value;
  
  // Convert to int32_t for CAN transmission
  int32_t toInt32() const {
    // By default, return uint_value as int32_t
    // For float, we would need to convert it to fixed-point
    return int_value;
  }
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
    _id(id), _updateInterval(updateInterval) {}
  
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
   * @param componentId Component ID
   * @param currentTime Current time in ms
   * @return true if sensor was read and transmitted
   */
  bool process(ProtobufCANInterface& can, uint8_t componentType, uint8_t componentId, uint32_t currentTime) {
    if (currentTime - _lastUpdateTime >= _updateInterval) {
      SensorValue value = read();
      _lastUpdateTime = currentTime;
      
      // Send via CAN
      can.sendMessage(
        kart_common_MessageType_STATUS,           // Status message
        (kart_common_ComponentType)componentType, // Component type
        componentId,                              // Component ID
        _id,                                      // Sensor ID as command ID
        (kart_common_ValueType)getValueType(),    // Value type
        value.toInt32()                           // Value
      );
      
      return true;
    }
    return false;
  }
  
protected:
  uint8_t _id;                 // Sensor ID within component
  uint16_t _updateInterval;    // Update interval in ms
  uint32_t _lastUpdateTime = 0; // Last update time
};

#endif // SENSOR_H 