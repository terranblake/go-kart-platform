#ifndef INTERNAL_ADC_READER_H
#define INTERNAL_ADC_READER_H

#include <Arduino.h>
#include "AnalogReader.h"

#if defined(ESP32) || defined(PLATFORM_ESP32)
#include "driver/adc.h"

/**
 * Implementation of AnalogReader for the ESP32's internal ADC.
 */
class InternalADCReader : public AnalogReader {
public:
  InternalADCReader(uint16_t vrefMv = 3300) : _vrefMv(vrefMv) {}

  /**
   * Configures global ADC settings like width.
   * Per-channel attenuation should be configured separately using configureAttenuation().
   */
  bool begin() override {
    // Set default width, can be overridden if needed
    adc1_config_width(ADC_WIDTH_BIT_12);
    // Note: ADC2 width config (adc2_config_channel_atten) is done per-read if using WiFi
    return true;
  }

  /**
   * Reads the raw value from a specific GPIO pin connected to ADC1 or ADC2.
   * @param gpioPin The GPIO pin number.
   * @return Raw ADC value (0-4095 for 12-bit), or -1 if pin is invalid.
   */
  int16_t readChannel(uint8_t gpioPin) override {
    // analogRead handles ADC1/ADC2 mapping based on pin
    int value = analogRead(gpioPin);
    // analogRead returns uint16_t, but ESP32 max is 4095
    // We return int16_t to match the interface, value will always be positive here.
    return (int16_t)value; 
  }

  /**
   * Returns the resolution (max value) for the ESP32 ADC (12-bit).
   */
  uint16_t getResolution() override {
    // ESP32 ADC is 12-bit
    return 4095;
  }

  /**
   * Returns the configured reference voltage.
   */
  uint16_t getReferenceVoltageMv() override {
    // This might need calibration per board
    return _vrefMv;
  }

  /**
   * Configures the attenuation for a specific ADC1 channel (GPIO pin).
   * Call this after begin() for each pin you intend to read.
   * @param channel The ADC1 channel (e.g., ADC1_CHANNEL_0).
   * @param atten The desired attenuation (e.g., ADC_ATTEN_DB_11).
   * @return true if configuration was successful.
   */
  bool configureAttenuation(adc1_channel_t channel, adc_atten_t atten) {
    if (channel >= ADC1_CHANNEL_MAX) return false;
    esp_err_t err = adc1_config_channel_atten(channel, atten);
    if (err != ESP_OK) {
#if DEBUG_MODE // Or use a dedicated logging macro
        Serial.printf("InternalADCReader: Failed to set ADC1 attenuation for channel %d\n", channel);
#endif
        return false;
    }
    return true;
  }

  // Note: ADC2 attenuation needs to be set before each read if WiFi is active.
  // The standard analogRead() might handle this, but direct adc2_get_raw requires manual setup.
  // For simplicity, this class relies on analogRead()'s behavior.

private:
  uint16_t _vrefMv; // Reference voltage in millivolts (needs calibration)
};

#else // Handle non-ESP32 platforms (optional basic implementation)

// Basic Arduino analogRead wrapper if not on ESP32
class InternalADCReader : public AnalogReader {
public:
   InternalADCReader(uint16_t vrefMv = 5000) : _vrefMv(vrefMv) {}

  bool begin() override {
    // No specific global setup needed for standard Arduino
    return true;
  }

  int16_t readChannel(uint8_t pin) override {
    // Standard Arduino analog pins are typically A0, A1, ...
    // The pin number passed might need mapping depending on board
    // Assuming 'pin' directly maps to the analog pin number used by analogRead
    return (int16_t)analogRead(pin);
  }

  uint16_t getResolution() override {
    // Standard Arduino UNO/Mega is 10-bit
    return 1023;
  }

  uint16_t getReferenceVoltageMv() override {
    return _vrefMv; // Default 5V, override in constructor if needed
  }

private:
    uint16_t _vrefMv;
};

#endif // ESP32 check

#endif // INTERNAL_ADC_READER_H 