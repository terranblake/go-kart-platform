#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"

/**
 * TemperatureSensor - Measures temperature using NTC thermistors
 * 
 * Designed for NTCLE100E3203JBD thermistors with a series resistor
 * Uses the Steinhart-Hart equation for temperature conversion
 */
class TemperatureSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type for this sensor (from TemperatureSensorLocation enum)
   * @param componentId Component ID for this sensor (from TemperatureSensorLocation enum)
   * @param commandId Command ID for this sensor (from TemperatureSensorLocation enum)
   * @param pin Analog pin connected to the voltage divider
   * @param updateInterval Update interval in ms
   * @param seriesResistor Value of the series resistor (ohms)
   * @param thermistorNominal Resistance at nominal temperature (ohms)
   * @param temperatureNominal Nominal temperature (°C)
   * @param bCoefficient B coefficient from thermistor datasheet
   */
  TemperatureSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    uint8_t pin,
    uint16_t updateInterval = 1000,
    uint32_t seriesResistor = 10000,
    uint32_t thermistorNominal = 10000,
    float temperatureNominal = 25.0,
    float bCoefficient = 3950.0
  ) : Sensor(componentType, componentId, commandId, kart_common_ValueType_UINT16, updateInterval),
      _pin(pin),
      _seriesResistor(seriesResistor),
      _thermistorNominal(thermistorNominal),
      _temperatureNominal(temperatureNominal),
      _bCoefficient(bCoefficient) {}
  
  /**
   * Initialize the temperature sensor
   */
  bool begin() override {
    // Set up analog pin
    pinMode(_pin, INPUT);
    return true;
  }
  
  /**
   * Read temperature
   * @return SensorValue containing temperature
   */
  SensorValue read() override {
    // Read temperature
    float lastTemp = readTemperature();
    
    // Store in SensorValue (as INT16 in tenths of a degree)
    _baseSensorValue.int16_value = (int16_t)(lastTemp * 10.0);
    return _baseSensorValue;
  }
  
private:
  uint8_t _pin;                // Analog pin
  uint32_t _seriesResistor;    // Series resistor value (ohms)
  uint32_t _thermistorNominal; // Resistance at nominal temperature (ohms)
  float _temperatureNominal;   // Nominal temperature (°C)
  float _bCoefficient;         // Beta coefficient from datasheet
  /**
   * Read the current temperature
   * @return Temperature in °C
   */
  float readTemperature() {
    // Read analog value
    int adcValue = analogRead(_pin);
    
    // Prevent division by zero or invalid readings
    if (adcValue >= 1023) adcValue = 1022;
    if (adcValue <= 0) adcValue = 1;
    
    // Calculate resistance of thermistor
#if defined(ESP8266) || defined(ESP32)
    // ESP ADC is 3.3V with 10-bit resolution (0-1023)
    float resistance = _seriesResistor / ((1023.0 / adcValue) - 1.0);
#else
    // Arduino ADC is typically 5V with 10-bit resolution (0-1023)
    float resistance = _seriesResistor / ((1023.0 / adcValue) - 1.0);
#endif
    
    // Calculate temperature using Steinhart-Hart equation (simplified B parameter equation)
    float steinhart;
    steinhart = resistance / _thermistorNominal;           // (R/Ro)
    steinhart = log(steinhart);                           // ln(R/Ro)
    steinhart /= _bCoefficient;                           // 1/B * ln(R/Ro)
    steinhart += 1.0 / (_temperatureNominal + 273.15);    // + (1/To)
    steinhart = 1.0 / steinhart;                          // Invert
    steinhart -= 273.15;                                  // Convert to °C
    
    // Apply limits
    if (steinhart < -55.0) steinhart = -55.0;
    if (steinhart > 125.0) steinhart = 125.0;
    
    return steinhart;
  }
};

#endif // TEMPERATURE_SENSOR_H 