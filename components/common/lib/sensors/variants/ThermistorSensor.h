#ifndef THERMISTOR_SENSOR_H
#define THERMISTOR_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "driver/adc.h" // Include ESP32 ADC driver

/**
 * ThermistorSensor - Measures temperature using NTC thermistors
 * 
 * Designed for NTCLE100E3xxxx thermistors with a series resistor
 * Uses the Steinhart-Hart equation for temperature conversion
 */
class ThermistorSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type for this sensor
   * @param componentId Component ID for this sensor
   * @param commandId Command ID for this sensor
   * @param pin Analog pin connected to the voltage divider
   * @param updateInterval Update interval in ms
   * @param seriesResistor Value of the series resistor (ohms)
   * @param thermistorNominal Resistance at nominal temperature (ohms)
   * @param temperatureNominal Nominal temperature (°C)
   * @param bCoefficient B coefficient from thermistor datasheet
   */
  ThermistorSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    uint8_t pin,
    uint16_t updateInterval = 1000,
    uint32_t seriesResistor = 10000,
    uint32_t thermistorNominal = 10000,
    float temperatureNominal = 25.0,
    float bCoefficient = 3977.0 // Default B value, check datasheet for NTCLE100E3103JBD (likely 3977)
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

    // Configure ADC attenuation for ESP32
#if defined(ESP32) || defined(PLATFORM_ESP32)
    adc1_config_width(ADC_WIDTH_BIT_12); // Explicitly set 12-bit width

    // Determine ADC1 channel based on the pin numbers from Config.h
    adc1_channel_t channel = ADC1_CHANNEL_MAX; // Default invalid channel
    if (_pin == 36) {         // GPIO 36 (TEMP_SENSOR_CONTROLLER) is ADC1_CHANNEL_0
        channel = ADC1_CHANNEL_0;
    } else if (_pin == 39) {  // GPIO 39 (TEMP_SENSOR_BATTERY) is ADC1_CHANNEL_3
        channel = ADC1_CHANNEL_3;
    } else if (_pin == 34) {  // GPIO 34 (TEMP_SENSOR_MOTOR) is ADC1_CHANNEL_6
        channel = ADC1_CHANNEL_6;
    }
    // Add mappings for other ADC1 pins if necessary

    if (channel != ADC1_CHANNEL_MAX) {
        // Use ADC_ATTEN_DB_12 for 0-3.3V range (11 is deprecated)
        esp_err_t err = adc1_config_channel_atten(channel, ADC_ATTEN_DB_12);
        if (err != ESP_OK) {
            Serial.printf("Failed to set ADC1 attenuation for pin %d, channel %d\n", _pin, channel);
        }
    } else {
        Serial.printf("Pin %d is not a configured ADC1 pin for attenuation setting\n", _pin);
        // Handle error or check ADC2 if applicable
    }
#endif
    return true;
  }
  
  /**
   * Read temperature
   * @return SensorValue containing temperature
   */
  SensorValue read() override {
    // Read temperature
    float lastTemp = readTemperature();
    
    // Store in SensorValue (as UINT16 in tenths of a degree)
    _baseSensorValue.uint16_value = (uint16_t)(lastTemp * 10.0);
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
    if (adcValue >= 4095) adcValue = 4094; // Use 12-bit max for ESP32 check
    if (adcValue <= 0) adcValue = 1;
    
    // Calculate resistance of thermistor
#if defined(ESP8266) || defined(ESP32) || defined(PLATFORM_ESP32)
    // ESP ADC is 3.3V with 12-bit resolution (0-4095)
    float resistance = _seriesResistor / ((4095.0 / adcValue) - 1.0);
#else
    // Arduino ADC is typically 5V with 10-bit resolution (0-1023)
    float resistance = _seriesResistor / ((1023.0 / adcValue) - 1.0);
#endif
    
    // Calculate temperature using Steinhart-Hart equation (simplified B parameter equation)
    // Ensure log() handles potential negative resistance values gracefully if ADC readings are strange
    if (resistance <= 0) {
      // Handle error: resistance calculation failed or ADC reading implies short circuit?
      // Return a known error value or the limit. Using -55.0 for now.
      return -55.0;
    }

    float steinhart;
    steinhart = resistance / _thermistorNominal;           // (R/Ro)
    steinhart = log(steinhart);                           // ln(R/Ro)
    steinhart /= _bCoefficient;                           // 1/B * ln(R/Ro)
    steinhart += 1.0 / (_temperatureNominal + 273.15);    // + (1/To)

    // Check for potential division by zero if steinhart calculation leads to zero
    if (steinhart == 0) {
       return -55.0; // Or another error indicator
    }

    steinhart = 1.0 / steinhart;                          // Invert
    steinhart -= 273.15;                                  // Convert to °C

    // Apply limits
    if (steinhart < -55.0) steinhart = -55.0;
    if (steinhart > 125.0) steinhart = 125.0; // Adjust upper limit if needed

    return steinhart;
  }
};

#endif // THERMISTOR_SENSOR_H 