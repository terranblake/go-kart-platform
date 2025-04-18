#ifndef VOLTAGE_SENSOR_H
#define VOLTAGE_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"

/**
 * VoltageSensor - Measures voltage using a scaled and isolated input.
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
   * @param pin Analog pin connected to the isolation amplifier output
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
    uint8_t pin,
    uint16_t updateInterval = 1000,
    float r1_ohms = 19000000.0, // Example: 19MOhms
    float r2_ohms = 1000000.0,  // Example: 1MOhms (Ratio = 20:1)
    float isolator_gain = 1.0,    // Example: Gain of 1
    uint16_t isolator_output_offset_mV = 0, // Example: Output is 0V at 0V input
    uint16_t adc_resolution = 4096, // Default ESP32 12-bit
    uint16_t adc_vref_mV = 3300 // Default ESP32 3.3V
  ) : Sensor(componentType, componentId, commandId, kart_common_ValueType_UINT16, updateInterval),
      _pin(pin),
      _r1_ohms(r1_ohms),
      _r2_ohms(r2_ohms),
      _isolator_gain(isolator_gain),
      _isolator_output_offset_mV(isolator_output_offset_mV),
      _adc_resolution(adc_resolution),
      _adc_vref_mV(adc_vref_mV) {}

  /**
   * Initialize the voltage sensor
   */
  bool begin() override {
    pinMode(_pin, INPUT);
    // ESP32 specific ADC setup (attenuation) might be needed here
    // similar to TemperatureSensor if not using default attenuation.
    // For now, assume default configuration is sufficient.
    return true;
  }

  /**
   * Read voltage
   * @return SensorValue containing voltage in millivolts (mV)
   */
  SensorValue read() override {
    // Read ADC value
    int adcValue = analogRead(_pin);

    // Convert ADC value to millivolts at the ADC input
    float adc_voltage_mV = (float)adcValue * ((float)_adc_vref_mV / (float)_adc_resolution);

    // Account for isolator offset
    float isolator_output_mV = adc_voltage_mV - (float)_isolator_output_offset_mV;

    // Calculate the voltage at the isolator's input (across R2)
    // IsolatorInput (mV) = IsolatorOutput (mV) / Gain
    float r2_voltage_mV = isolator_output_mV / _isolator_gain;

    // Calculate the original high voltage
    // HV (mV) = R2_Voltage (mV) * (R1 + R2) / R2
    float hv_voltage_mV = r2_voltage_mV * (_r1_ohms + _r2_ohms) / _r2_ohms;

    // Clamp to valid range for uint16_t (0 - 65535 mV or 65.535 V)
    // Adjust if using a different type or expecting higher voltages.
    if (hv_voltage_mV < 0) hv_voltage_mV = 0;
    if (hv_voltage_mV > 65535.0) hv_voltage_mV = 65535.0;

    // Store in SensorValue as UINT16 (millivolts)
    _baseSensorValue.uint16_value = (uint16_t)hv_voltage_mV;
    return _baseSensorValue;
  }

private:
  uint8_t _pin;                       // Analog pin
  float _r1_ohms;                     // Voltage divider R1 (Ohms)
  float _r2_ohms;                     // Voltage divider R2 (Ohms)
  float _isolator_gain;               // Gain of isolation amplifier
  uint16_t _isolator_output_offset_mV;// Offset voltage of isolator output (mV)
  uint16_t _adc_resolution;           // ADC resolution (e.g., 4096)
  uint16_t _adc_vref_mV;              // ADC reference voltage (mV)
};

#endif // VOLTAGE_SENSOR_H 