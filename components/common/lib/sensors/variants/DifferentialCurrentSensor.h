#ifndef DIFFERENTIAL_CURRENT_SENSOR_H
#define DIFFERENTIAL_CURRENT_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "../src/AnalogReader.h" // Include AnalogReader interface
#include "common.pb.h"
#include "batteries.pb.h" // Include batteries proto for CURRENT command ID

#include <cmath> // Include for isnan

/**
 * DifferentialCurrentSensor - Measures current using a shunt resistor via an AnalogReader.
 *
 * Reads the voltage difference between two ADC channels connected across a shunt resistor.
 * Calculates current based on Ohm's law (I = V/R).
 * Designed to work with AnalogReader implementations like ADS1115Reader.
 * Output is scaled to milliamps (mA) and stored as int16_t.
 */
class DifferentialCurrentSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type (e.g., BATTERIES)
   * @param componentId Component ID (e.g., BATTERY_MAIN)
   * @param commandId Command ID (should be CURRENT)
   * @param reader Pointer to the AnalogReader instance (e.g., ADS1115Reader).
   * @param channelPositive Channel identifier for the positive side of the shunt.
   * @param channelNegative Channel identifier for the negative side of the shunt.
   * @param updateInterval Update interval in ms.
   * @param shuntResistanceMilliOhm Resistance of the shunt in mOhms.
   */
  DifferentialCurrentSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId, // Should be kart_batteries_BatteryCommandId_CURRENT
    AnalogReader* reader,
    uint8_t channelPositive,
    uint8_t channelNegative,
    uint16_t updateInterval,
    float shuntResistanceMilliOhm
  ) : Sensor(componentType, componentId, commandId, kart_common_ValueType_INT16, updateInterval),
      _reader(reader),
      _ch_p(channelPositive),
      _ch_n(channelNegative),
      _shuntResistance_mOhm(shuntResistanceMilliOhm)
  {
      if (_shuntResistance_mOhm <= 0.0f) {
          #ifdef DEBUG_MODE
            Serial.println("DifferentialCurrentSensor: Error - Shunt resistance must be positive.");
          #endif
          _shuntResistance_mOhm = 1.0f; // Default to prevent division by zero
      }
       if (!_reader) {
          #ifdef DEBUG_MODE
            Serial.println("DifferentialCurrentSensor: Error - AnalogReader pointer is null.");
          #endif
      }
  }

  /**
   * Initialize the sensor.
   * ADC initialization is handled by the provided AnalogReader.
   */
  bool begin() override {
    // Rely on the external AnalogReader being initialized elsewhere
    return (_reader != nullptr);
  }

  /**
   * Read differential voltage via AnalogReader and calculate current.
   * @return SensorValue containing current in milliamps (mA) as INT16.
   */
  SensorValue read() override {
    if (!_reader) {
        _baseSensorValue.int16_value = 0; // Indicate error
        return _baseSensorValue;
    }

    // Read voltage from both channels in millivolts
    float voltage_p_mV = _reader->readVoltageMv(_ch_p);
    float voltage_n_mV = _reader->readVoltageMv(_ch_n);

    // Check for potential NaN results from reader
    if (std::isnan(voltage_p_mV) || std::isnan(voltage_n_mV)) {
         #ifdef DEBUG_MODE
           Serial.println("DifferentialCurrentSensor: Warning - NaN reading from AnalogReader.");
         #endif
        _baseSensorValue.int16_value = 0; // Indicate error
        return _baseSensorValue;
    }

    // Calculate differential voltage
    float diff_mV = voltage_p_mV - voltage_n_mV;

    // Calculate current using Ohm's Law (I = V / R)
    // V is in mV, R is in mOhm, so I = mV / mOhm = A
    float current_A = diff_mV / _shuntResistance_mOhm;

    float current_mA_float = current_A * 1000.0f;

    // Clamp to INT16 range
    if (current_mA_float > 32767.0f) current_mA_float = 32767.0f;
    if (current_mA_float < -32768.0f) current_mA_float = -32768.0f;

    #if DEBUG_MODE
      // Optional detailed debug print
      // Serial.printf("DiffCurr: Vp=%.2fmV Vn=%.2fmV Vdiff=%.2fmV | I=%.2fA | mA=%d\n", 
      //               voltage_p_mV, voltage_n_mV, diff_mV, current_A, (int16_t)current_mA_float);
    #endif

    _baseSensorValue.int16_value = (int16_t)current_mA_float;
    return _baseSensorValue;
  }

private:
  AnalogReader* _reader;
  uint8_t _ch_p;
  uint8_t _ch_n;
  float _shuntResistance_mOhm;
};

#endif // DIFFERENTIAL_CURRENT_SENSOR_H 