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
   * @param componentId Component ID for this sensor (used as component_id)
   * @param updateInterval Update interval in milliseconds
   */
  Sensor(kart_common_ComponentType componentType, uint8_t componentId, uint8_t commandId, kart_common_ValueType valueType, uint16_t updateInterval = 1000) :
    _componentType(static_cast<uint8_t>(componentType)),
    _componentId(componentId),
    _commandId(commandId),
    _valueType(valueType),
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

  todo: make begin a generic pin/interrupt setup function 
        for all sensors based on some list of pins/configs provided
   */
  virtual bool begin() = 0;
  
  virtual SensorValue read() = 0;
  
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
      // Read sensor directly into the base sensor value member
      _baseSensorValue = read();
      
      // Send over CAN
      bool result = canInterface.sendMessage(
        kart_common_MessageType_STATUS,
        static_cast<kart_common_ComponentType>(_componentType),
        _componentId,
        _commandId,
        _valueType, // Value type
        getValue()             // Value
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
   * Get the sensor component ID
   * @return The component ID
   */
  uint8_t getComponentId() const {
    return _componentId;
  }

  uint8_t getCommandId() const {
    return _commandId;
  }

  kart_common_ValueType getValueType() const {
    return _valueType;
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

  /**
   * Set the value of the sensor
   * @param value SensorValue union
   */
  void setValue(const SensorValue& value) {
    switch (getValueType()) {
      case kart_common_ValueType_BOOLEAN:
        _baseSensorValue.bool_value = value.bool_value;
        break;
      case kart_common_ValueType_INT8:
        _baseSensorValue.int8_value = value.int8_value;
        break;
      case kart_common_ValueType_UINT8:
        _baseSensorValue.uint8_value = value.uint8_value;
        break;
      case kart_common_ValueType_INT16:
        _baseSensorValue.int16_value = value.int16_value;
        break;
      case kart_common_ValueType_UINT16:
        _baseSensorValue.uint16_value = value.uint16_value;
        break;
      case kart_common_ValueType_UINT24:
        _baseSensorValue.uint24_value = value.uint24_value;
        break;
      default:
        // todo: handle error
        break;
    }
  }

  /**
   * Extract the proper value from SensorValue based on type
   * @param value SensorValue union
   * @return Value as int32_t for CAN transmission
   */
  int32_t getValue() const {
    switch (getValueType()) {
      case kart_common_ValueType_BOOLEAN:
        return _baseSensorValue.bool_value ? 1 : 0;
      case kart_common_ValueType_INT8:
        return _baseSensorValue.int8_value;
      case kart_common_ValueType_UINT8:
        return _baseSensorValue.uint8_value;
      case kart_common_ValueType_INT16:
        return _baseSensorValue.int16_value;
      case kart_common_ValueType_UINT16:
        return _baseSensorValue.uint16_value;
      case kart_common_ValueType_UINT24:
        return _baseSensorValue.uint24_value;
      default:
        return 0;
    }
  }
  
protected:
  uint8_t _componentType;             // Sensor component type (component_type)
  uint8_t _componentId;               // Sensor component ID (component_id)
  uint8_t _commandId;                 // Sensor command ID (command_id)
  kart_common_ValueType _valueType;   // Sensor value type (value_type)
  uint16_t _updateInterval;          // Update interval in ms
  unsigned long _lastUpdateTime;     // Last update timestamp
  bool _enabled;                     // Enabled flag
  SensorValue _baseSensorValue;      // Reusable sensor value for base class
};

#endif // SENSOR_H 