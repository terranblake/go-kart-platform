#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "../src/AnalogReader.h"
#include <cmath>

/**
 * CurrentSensor - Measures current using a Hall Effect sensor via an AnalogReader.
 *
 * Assumes a linear Hall Effect sensor (like ACS712/723 or discrete + op-amp)
 * where output voltage is proportional to current. 
 * Output is scaled to milliamps (mA) and stored as int16_t.
 */
class CurrentSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type for this sensor
   * @param componentId Component ID for this sensor
   * @param commandId Command ID for this sensor
   * @param reader Pointer to the AnalogReader instance to use.
   * @param channelId Channel identifier (pin/ADC channel) for the AnalogReader.
   * @param updateInterval Update interval in ms
   * @param sensitivity_mV_per_A Sensitivity of the sensor (mV output per Amp measured)
   * @param zero_current_voltage_mV The sensor's output voltage (in mV) when zero current is flowing
   */
  CurrentSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    AnalogReader* reader,
    uint8_t channelId,
    uint16_t updateInterval = 1000,
    float sensitivity_mV_per_A = 100.0, // Example: 100mV/A common for ACS712-20A
    uint16_t zero_current_voltage_mV = 2500 // Example: 2500mV for 5V sensor w/ 3.3V ADC?
  ) : Sensor(componentType, componentId, commandId, kart_common_ValueType_INT16, updateInterval),
      _reader(reader),
      _channelId(channelId),
      _sensitivity_mV_per_A(sensitivity_mV_per_A),
      _zero_current_voltage_mV(zero_current_voltage_mV) {}

  /**
   * Initialize the current sensor.
   * Pin/ADC setup is handled by the AnalogReader.
   */
  bool begin() override {
    if (!_reader) {
      return false; // Must have a valid reader
    }
    return true;
  }

  /**
   * Read current using the AnalogReader.
   * @return SensorValue containing current in milliamps (mA) as INT16.
   */
  SensorValue read() override {
    if (!_reader) {
        _baseSensorValue.int16_value = 0; // Error indicator
        return _baseSensorValue;
    }

    float voltage_mV = _reader->readVoltageMv(_channelId);

    if (std::isnan(voltage_mV)) {
        _baseSensorValue.int16_value = 0; // Error indicator
        return _baseSensorValue;
    }

    if (_sensitivity_mV_per_A == 0.0f) {
        _baseSensorValue.int16_value = 0; // Error indicator
        return _baseSensorValue;
    }

    float current_A = (voltage_mV - (float)_zero_current_voltage_mV) / _sensitivity_mV_per_A;

    float current_mA_float = current_A * 1000.0f;

    if (current_mA_float > 32767.0f) current_mA_float = 32767.0f;
    if (current_mA_float < -32768.0f) current_mA_float = -32768.0f;

    _baseSensorValue.int16_value = (int16_t)current_mA_float;
    return _baseSensorValue;
  }

private:
  AnalogReader* _reader;
  uint8_t _channelId;
  float _sensitivity_mV_per_A;
  uint16_t _zero_current_voltage_mV;
};

#endif // CURRENT_SENSOR_H 