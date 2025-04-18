#ifndef CURRENT_SENSOR_H
#define CURRENT_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"

/**
 * CurrentSensor - Measures current using a Hall Effect sensor
 *
 * Assumes a linear Hall Effect sensor (like ACS712/723 or discrete + op-amp)
 * where output voltage is proportional to current. Designed for use with
 * a ferrite core around the conductor.
 *
 * Output is scaled to milliamps (mA) and stored as int16_t.
 */
class CurrentSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type for this sensor
   * @param componentId Component ID for this sensor
   * @param commandId Command ID for this sensor
   * @param pin Analog pin connected to the Hall sensor output
   * @param updateInterval Update interval in ms
   * @param sensitivity_mV_per_A Sensitivity of the sensor (mV output per Amp measured)
   * @param zero_current_voltage_mV The sensor's output voltage (in mV) when zero current is flowing (often Vcc/2)
   * @param adc_resolution The resolution of the ADC (e.g., 4096 for 12-bit, 1024 for 10-bit)
   * @param adc_vref_mV The reference voltage of the ADC in millivolts (e.g., 3300 for 3.3V)
   */
  CurrentSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    uint8_t pin,
    uint16_t updateInterval = 1000,
    float sensitivity_mV_per_A = 100.0, // Example: 100mV/A common for ACS712-20A
    uint16_t zero_current_voltage_mV = 2500, // Example: 2500mV for 5V sensor
    uint16_t adc_resolution = 4096, // Default ESP32 12-bit
    uint16_t adc_vref_mV = 3300 // Default ESP32 3.3V
  ) : Sensor(componentType, componentId, commandId, kart_common_ValueType_INT16, updateInterval),
      _pin(pin),
      _sensitivity_mV_per_A(sensitivity_mV_per_A),
      _zero_current_voltage_mV(zero_current_voltage_mV),
      _adc_resolution(adc_resolution),
      _adc_vref_mV(adc_vref_mV) {}

  /**
   * Initialize the current sensor
   */
  bool begin() override {
    pinMode(_pin, INPUT);
    // ESP32 specific ADC setup (attenuation) might be needed here
    // similar to TemperatureSensor if not using default attenuation.
    // For now, assume default configuration is sufficient.
    return true;
  }

  /**
   * Read current
   * @return SensorValue containing current in milliamps (mA)
   */
  SensorValue read() override {
    // Read ADC value
    int adcValue = analogRead(_pin);

    // Convert ADC value to millivolts
    float voltage_mV = (float)adcValue * ((float)_adc_vref_mV / (float)_adc_resolution);

    // Calculate current in Amps
    // Current (A) = (Measured Voltage (mV) - Zero Current Voltage (mV)) / Sensitivity (mV/A)
    float current_A = (voltage_mV - (float)_zero_current_voltage_mV) / _sensitivity_mV_per_A;

    // Convert current to milliamps (mA)
    int16_t current_mA = (int16_t)(current_A * 1000.0);

    // Store in SensorValue as INT16
    _baseSensorValue.int16_value = current_mA;
    return _baseSensorValue;
  }

private:
  uint8_t _pin;                         // Analog pin
  float _sensitivity_mV_per_A;        // Sensor sensitivity (mV/A)
  uint16_t _zero_current_voltage_mV;   // Voltage at zero current (mV)
  uint16_t _adc_resolution;             // ADC resolution (e.g., 4096)
  uint16_t _adc_vref_mV;                // ADC reference voltage (mV)
};

#endif // CURRENT_SENSOR_H 