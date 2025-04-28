#ifndef THERMISTOR_SENSOR_H
#define THERMISTOR_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "../src/AnalogReader.h" // Include the new interface
#include <cmath> // Include for log()

// Remove ESP32 ADC driver include, no longer needed here
// #include "driver/adc.h" 

/**
 * ThermistorSensor - Measures temperature using NTC thermistors via an AnalogReader.
 * 
 * Designed for NTCLE100E3xxxx thermistors with a series resistor.
 * Uses the Steinhart-Hart equation for temperature conversion.
 * Reads voltage using the provided AnalogReader instance.
 */
class ThermistorSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type for this sensor
   * @param componentId Component ID for this sensor
   * @param commandId Command ID for this sensor
   * @param reader Pointer to the AnalogReader instance to use for reading voltage.
   * @param channelId Channel identifier (e.g., pin or ADC channel number) for the AnalogReader.
   * @param updateInterval Update interval in ms
   * @param seriesResistor Value of the series resistor (ohms)
   * @param thermistorNominal Resistance at nominal temperature (ohms)
   * @param temperatureNominal Nominal temperature (°C)
   * @param bCoefficient B coefficient from thermistor datasheet
   * @param valueType The type of value this sensor reports (should be INT16 for temp)
   * @param dividerSupplyVoltageMv The supply voltage (in mV) powering the thermistor voltage divider (e.g., 3300).
   */
  ThermistorSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    AnalogReader* reader,         // Changed from pin
    uint8_t channelId,          // Added channel ID
    uint16_t updateInterval = 1000,
    uint32_t seriesResistor = 10000,
    uint32_t thermistorNominal = 10000,
    float temperatureNominal = 25.0,
    float bCoefficient = 3977.0, // Default B value for NTCLE100E3103JBD
    kart_common_ValueType valueType = kart_common_ValueType_INT16, // Default to INT16
    uint16_t dividerSupplyVoltageMv = 3300 // Default to 3.3V
  ) : Sensor(componentType, componentId, commandId, valueType, updateInterval), // Pass valueType to base
      _reader(reader),             // Store reader pointer
      _channelId(channelId),       // Store channel ID
      _seriesResistor(seriesResistor),
      _thermistorNominal(thermistorNominal),
      _temperatureNominal(temperatureNominal),
      _bCoefficient(bCoefficient),
      _dividerSupplyVoltageMv(dividerSupplyVoltageMv) {}
  
  /**
   * Initialize the temperature sensor.
   * Sensor-specific pin setup (like ADC attenuation) is now handled
   * by the AnalogReader's begin() or configuration methods.
   */
  bool begin() override {
    // Pin mode and ADC configuration are no longer done here.
    // Ensure the passed AnalogReader's begin() method is called elsewhere.
    if (!_reader) {
      return false; // Must have a valid reader
    }
    // No sensor-specific hardware setup needed in this `begin`
    return true;
  }
  
  /**
   * Read temperature using the configured AnalogReader.
   * @return SensorValue containing temperature in tenths of a degree Celsius (INT16).
   */
  SensorValue read() override {
    if (!_reader) {
        // Handle error: No reader available
        _baseSensorValue.int16_value = 0; // Or some error indicator
        return _baseSensorValue;
    }
    
    // Read temperature using the helper method
    float temperatureC = readTemperature();
    
    // Store in SensorValue (as INT16 in tenths of a degree)
    // Clamp to valid range for int16_t (e.g., -32768 to 32767)
    // However, practical thermistor range is smaller, e.g., -550 to 1250 tenths C
    int16_t tempTenths = (int16_t)(temperatureC * 10.0f);
    if (tempTenths < -550) tempTenths = -550; // Clamp to practical min
    if (tempTenths > 1250) tempTenths = 1250; // Clamp to practical max

    // Store directly into the int16_value field
    _baseSensorValue.int16_value = tempTenths; 
    return _baseSensorValue;
  }
  
private:
  AnalogReader* _reader;       // Pointer to the analog reader implementation
  uint8_t _channelId;          // Channel identifier for the reader (pin or ADC channel)
  // uint8_t _pin;             // Removed pin
  uint32_t _seriesResistor;    // Series resistor value (ohms)
  uint32_t _thermistorNominal; // Resistance at nominal temperature (ohms)
  float _temperatureNominal;   // Nominal temperature (°C)
  float _bCoefficient;         // Beta coefficient from datasheet
  uint16_t _dividerSupplyVoltageMv; // Supply voltage for the thermistor divider

  /**
   * Read the current temperature using the AnalogReader.
   * @return Temperature in °C, or a specific error value (like NAN or -273.15) on failure.
   */
  float readTemperature() {
    // Read voltage from the divider using the AnalogReader
    float voltage_mV = _reader->readVoltageMv(_channelId);
    
    if (std::isnan(voltage_mV)) {
        return -273.15f; // Return absolute zero or NAN
    }

    // Use the known divider supply voltage for calculation
    uint16_t vSupply_mV = _dividerSupplyVoltageMv;

    // Calculate thermistor resistance from the voltage divider formula:
    // Vout = Vsupply * Rtherm / (Rseries + Rtherm)
    // Rtherm = Rseries * Vout / (Vsupply - Vout)
    
    // Prevent division by zero if Vout equals Vsupply
    if (voltage_mV >= vSupply_mV) {
        // This implies Rtherm is very high or infinite (open circuit?)
        return 125.0f; // Return max temp limit
    }
    // Prevent issues if Vout is zero or negative (short circuit?)
    if (voltage_mV <= 0) {
        return -55.0f; // Return min temp limit
    }

    float resistance = (float)_seriesResistor * voltage_mV / ((float)vSupply_mV - voltage_mV);

    // Calculate temperature using Steinhart-Hart equation (simplified B parameter equation)
    if (resistance <= 0) {
      // Handle error: resistance calculation failed
      return -273.15f; // Return absolute zero or NAN
    }

    float steinhart;
    steinhart = resistance / _thermistorNominal;           // (R/Ro)
    steinhart = logf(steinhart);                          // ln(R/Ro) - use logf for float
    steinhart /= _bCoefficient;                           // 1/B * ln(R/Ro)
    steinhart += 1.0f / (_temperatureNominal + 273.15f);   // + (1/To)

    if (steinhart == 0) {
       return -273.15f; // Error: Division by zero incoming
    }

    steinhart = 1.0f / steinhart;                          // Invert -> Absolute temp in Kelvin
    steinhart -= 273.15f;                                  // Convert to °C

    // Apply sensible limits based on typical thermistor range
    if (steinhart < -55.0f) steinhart = -55.0f;
    if (steinhart > 125.0f) steinhart = 125.0f; // Adjust upper limit if needed

    return steinhart;
  }
};

#endif // THERMISTOR_SENSOR_H 