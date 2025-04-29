#ifndef MAPPED_ANALOG_SENSOR_H
#define MAPPED_ANALOG_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "../src/AnalogReader.h"
#include <cmath> // Include for std::isnan

/**
 * MappedAnalogSensor - Reads an analog value and maps it to a specified range.
 *
 * Useful for sensors like throttle or brake pedals where the raw ADC reading
 * needs to be translated into a standardized range (e.g., 0-1024).
 */
class MappedAnalogSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type (e.g., CONTROLS)
   * @param componentId Component ID (e.g., THROTTLE)
   * @param commandId Command ID (e.g., POSITION)
   * @param valueType Value type to report (e.g., UINT16 for 0-1024)
   * @param reader Pointer to the AnalogReader instance.
   * @param channelId Channel identifier for the AnalogReader.
   * @param updateInterval Update interval in ms.
   * @param minAdc Raw ADC value corresponding to the minimum output value.
   * @param maxAdc Raw ADC value corresponding to the maximum output value.
   * @param outMin The minimum value of the output range.
   * @param outMax The maximum value of the output range.
   */
  MappedAnalogSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    kart_common_ValueType valueType,
    AnalogReader* reader,
    uint8_t channelId,
    uint16_t updateInterval,
    uint16_t minAdc, 
    uint16_t maxAdc,
    int32_t outMin, 
    int32_t outMax
  ) : Sensor(componentType, componentId, commandId, valueType, updateInterval),
      _reader(reader),
      _channelId(channelId),
      _minAdc(minAdc),
      _maxAdc(maxAdc),
      _outMin(outMin),
      _outMax(outMax) 
  {
    // Ensure min/max ADC values are reasonable
    if (_minAdc >= _maxAdc) {
        // Handle error or swap values? For now, log if debug
        #ifdef DEBUG_MODE
          Serial.println("MappedAnalogSensor: Warning - minAdc >= maxAdc");
        #endif
        // Could default to a safe range or use assert
    }
  }

  /**
   * Initialize the sensor.
   * Pin/ADC setup is handled by the AnalogReader.
   */
  bool begin() override {
    return (_reader != nullptr);
  }

  /**
   * Read the analog value, map it, and return.
   * @return SensorValue containing the mapped value.
   */
  SensorValue read() override {
    if (!_reader) {
        _baseSensorValue.int16_value = _outMin; // Default to min on error
        _baseSensorValue.uint16_value = _outMin;
        return _baseSensorValue;
    }

    int rawValue = _reader->readChannel(_channelId);

    // Clamp raw value to the defined ADC range
    if (rawValue < _minAdc) rawValue = _minAdc;
    if (rawValue > _maxAdc) rawValue = _maxAdc;

    // Perform linear mapping
    // Ensure maxAdc > minAdc to prevent division by zero
    long mappedValue = _outMin;
    if (_maxAdc > _minAdc) {
         mappedValue = map(rawValue, _minAdc, _maxAdc, _outMin, _outMax);
    }

    // Store in the appropriate SensorValue field based on _valueType
    switch (_valueType) {
      case kart_common_ValueType_UINT8:
        _baseSensorValue.uint8_value = (uint8_t)constrain(mappedValue, 0, 255);
        break;
      case kart_common_ValueType_INT8:
         _baseSensorValue.int8_value = (int8_t)constrain(mappedValue, -128, 127);
         break;
      case kart_common_ValueType_UINT16:
         _baseSensorValue.uint16_value = (uint16_t)constrain(mappedValue, 0, 65535);
         break;
       case kart_common_ValueType_INT16:
          _baseSensorValue.int16_value = (int16_t)constrain(mappedValue, -32768, 32767);
          break;
      // Add other types if needed, though mapping usually targets int/uint
      default:
         // Default to UINT16 if type unknown/unsupported for mapping
         _baseSensorValue.uint16_value = (uint16_t)constrain(mappedValue, 0, 65535);
         break;
    }
    
    return _baseSensorValue;
  }

private:
  AnalogReader* _reader;
  uint8_t _channelId;
  uint16_t _minAdc;
  uint16_t _maxAdc;
  int32_t _outMin;
  int32_t _outMax;
};

#endif // MAPPED_ANALOG_SENSOR_H 