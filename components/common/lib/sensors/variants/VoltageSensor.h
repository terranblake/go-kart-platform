#ifndef VOLTAGE_SENSOR_H
#define VOLTAGE_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "../src/AnalogReader.h"
#include <cmath>

/**
 * VoltageSensor - Measures voltage using a scaled/isolated input via an AnalogReader.
 *
 * Assumes input comes from an isolation amplifier (like AMC1301 or ISO224)
 * which is fed by a voltage divider from the high voltage source.
 * Calculates the original high voltage based on divider ratio and amplifier gain.
 *
 * Output is scaled to millivolts (mV) and stored as uint16_t (assuming positive voltage).
 * Use int32_t if negative voltages or higher range needed, and adjust ValueType.
 */
class VoltageSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type for this sensor
   * @param componentId Component ID for this sensor
   * @param commandId Command ID for this sensor
   * @param reader Pointer to the AnalogReader instance to use.
   * @param channelId Channel identifier (pin/ADC channel) for the AnalogReader.
   * @param updateInterval Update interval in ms
   * @param r1_ohms Resistance of the top resistor in the voltage divider (connected to HV+)
   * @param r2_ohms Resistance of the bottom resistor in the voltage divider (across isolation amp input)
   * @param isolator_gain Gain of the isolation amplifier (V/V)
   * @param isolator_output_offset_mV Offset voltage of the isolator output (mV), relative to ADC ground. Some output 0V, others Vref/2.
   * @param adc_resolution The resolution of the ADC (e.g., 4096 for 12-bit, 1024 for 10-bit)
   * @param adc_vref_mV The reference voltage of the ADC in millivolts (e.g., 3300 for 3.3V)
   */
  VoltageSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    AnalogReader* reader,
    uint8_t channelId,
    uint16_t updateInterval = 1000,
    float r1_ohms = 19000000.0, // Example: 19MOhms
    float r2_ohms = 1000000.0,  // Example: 1MOhms (Ratio = 20:1)
    float isolator_gain = 1.0,    // Example: Gain of 1
    uint16_t isolator_output_offset_mV = 0, // Example: Output is 0V at 0V input
    uint16_t adc_resolution = 4096, // Default ESP32 12-bit
    uint16_t adc_vref_mV = 3300 // Default ESP32 3.3V
  ) : Sensor(componentType, componentId, commandId, kart_common_ValueType_UINT16, updateInterval),
      _reader(reader),
      _channelId(channelId),
      _r1_ohms(r1_ohms),
      _r2_ohms(r2_ohms),
      _isolator_gain(isolator_gain),
      _isolator_output_offset_mV(isolator_output_offset_mV),
      _adc_resolution(adc_resolution),
      _adc_vref_mV(adc_vref_mV) {}

  /**
   * Initialize the voltage sensor.
   * Pin/ADC setup is handled by the AnalogReader.
   */
  bool begin() override {
    if (!_reader) {
      return false; // Must have a valid reader
    }
    return true;
  }

  /**
   * Read voltage using the AnalogReader.
   * @return SensorValue containing voltage in millivolts (mV) as UINT16.
   */
  SensorValue read() override {
    if (!_reader) {
        _baseSensorValue.uint16_value = 0; // Error indicator
        return _baseSensorValue;
    }

    // Read the voltage at the ADC input using the reader
    float adc_voltage_mV = _reader->readVoltageMv(_channelId);

    if (std::isnan(adc_voltage_mV)) {
        // Handle read failure
        _baseSensorValue.uint16_value = 0; // Error indicator
        return _baseSensorValue;
    }

    // Account for isolator offset
    float isolator_output_mV = adc_voltage_mV - (float)_isolator_output_offset_mV;

    // Prevent division by zero if gain is zero (shouldn't happen but safety check)
    if (_isolator_gain == 0.0f) {
        _baseSensorValue.uint16_value = 0; // Error indicator
        return _baseSensorValue;
    }

    // Calculate the voltage at the isolator's input (across R2)
    // IsolatorInput (mV) = IsolatorOutput (mV) / Gain
    float r2_voltage_mV = isolator_output_mV / _isolator_gain;

    // Prevent division by zero if R2 is zero
    if (_r2_ohms == 0.0f) {
        _baseSensorValue.uint16_value = 0; // Error indicator
        return _baseSensorValue;
    }

    // Calculate the original high voltage
    // HV (mV) = R2_Voltage (mV) * (R1 + R2) / R2
    float hv_voltage_mV = r2_voltage_mV * (_r1_ohms + _r2_ohms) / _r2_ohms;

    // Clamp to valid range for uint16_t (0 - 65535 mV or 65.535 V)
    if (hv_voltage_mV < 0.0f) hv_voltage_mV = 0.0f;
    if (hv_voltage_mV > 65535.0f) hv_voltage_mV = 65535.0f;

    // Store in SensorValue as UINT16 (millivolts)
    _baseSensorValue.uint16_value = (uint16_t)hv_voltage_mV;
    return _baseSensorValue;
  }

private:
  AnalogReader* _reader;              // Pointer to the analog reader
  uint8_t _channelId;                 // Channel identifier for the reader
  float _r1_ohms;                     // Voltage divider R1 (Ohms)
  float _r2_ohms;                     // Voltage divider R2 (Ohms)
  float _isolator_gain;               // Gain of isolation amplifier
  uint16_t _isolator_output_offset_mV;// Offset voltage of isolator output (mV)
  uint16_t _adc_resolution;           // ADC resolution (e.g., 4096)
  uint16_t _adc_vref_mV;              // ADC reference voltage (mV)
};

#endif // VOLTAGE_SENSOR_H 