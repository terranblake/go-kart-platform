#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"

/**
 * TemperatureSensor - Implementation for NTCLE100E3203JBD thermistor
 */
class TemperatureSensor : public Sensor {
public:
  /**
   * Constructor
   * @param id Sensor ID
   * @param pin Analog pin for thermistor
   * @param updateInterval Update interval in ms
   * @param seriesResistor Series resistor value (ohms)
   * @param thermistorNominal Nominal resistance at 25°C (ohms)
   * @param temperatureNominal Nominal temperature (°C)
   * @param bCoefficient Beta coefficient from datasheet
   */
  TemperatureSensor(
    uint8_t id, 
    uint8_t pin, 
    uint16_t updateInterval = 1000,
    uint32_t seriesResistor = 10000,
    uint32_t thermistorNominal = 10000,
    float temperatureNominal = 25.0,
    float bCoefficient = 3977.0
  ) : Sensor(id, updateInterval),
      _pin(pin),
      _seriesResistor(seriesResistor),
      _thermistorNominal(thermistorNominal),
      _temperatureNominal(temperatureNominal),
      _bCoefficient(bCoefficient) {}
  
  /**
   * Initialize the temperature sensor
   */
  bool begin() override {
    pinMode(_pin, INPUT);
    return true;
  }
  
  /**
   * Read temperature from thermistor
   * @return SensorValue containing temperature in °C
   */
  SensorValue read() override {
    int rawADC = analogRead(_pin);
    
    // Convert ADC value to temperature
    float temperature = calculateTemperature(rawADC);
    
    // Store in SensorValue
    SensorValue value;
    value.float_value = temperature;
    return value;
  }
  
  /**
   * Get value type (FLOAT16)
   */
  uint8_t getValueType() const override {
    return kart_common_ValueType_FLOAT16;
  }
  
  /**
   * Convert sensor value to int32_t for CAN transmission
   * We'll use a fixed-point representation (temp * 100)
   */
  int32_t toInt32() const {
    return static_cast<int32_t>(_lastReading * 100.0f);
  }
  
  /**
   * Get last temperature reading
   * @return Temperature in °C
   */
  float getTemperature() const {
    return _lastReading;
  }
  
private:
  uint8_t _pin;
  uint32_t _seriesResistor;
  uint32_t _thermistorNominal;
  float _temperatureNominal;
  float _bCoefficient;
  float _lastReading = 0.0f;
  
  /**
   * Calculate temperature using Steinhart-Hart equation
   * @param adcValue Raw ADC value
   * @return Temperature in °C
   */
  float calculateTemperature(int adcValue) {
    // Safety check for invalid readings
    if (adcValue <= 1 || adcValue >= 1022) {
      return -99.9f;
    }
    
    // Convert ADC reading to resistance
    float resistance = _seriesResistor / ((1023.0f / adcValue) - 1.0f);
    
    // Apply Steinhart-Hart equation
    float steinhart = resistance / _thermistorNominal;  // (R/Ro)
    steinhart = log(steinhart);                         // ln(R/Ro)
    steinhart /= _bCoefficient;                         // 1/B * ln(R/Ro)
    steinhart += 1.0f / (_temperatureNominal + 273.15f); // + (1/To)
    steinhart = 1.0f / steinhart;                        // Invert
    steinhart -= 273.15f;                                // Convert to °C
    
    // Sanity check
    if (steinhart < -40.0f || steinhart > 125.0f) {
      return -99.9f;
    }
    
    _lastReading = steinhart;
    return steinhart;
  }
};

#endif // TEMPERATURE_SENSOR_H 