#ifndef ANALOG_READER_H
#define ANALOG_READER_H

#include <stdint.h>

/**
 * Abstract interface for reading analog values.
 * This allows sensors to be agnostic to the underlying ADC hardware
 * (e.g., internal MCU ADC, external ADS1115).
 */
class AnalogReader {
public:
  virtual ~AnalogReader() {}

  /**
   * Initialize the analog reader hardware/library.
   * @return true if initialization was successful, false otherwise.
   */
  virtual bool begin() = 0;

  /**
   * Reads the raw value from a specific analog channel.
   * @param channelIdentifier Identifier for the channel (e.g., GPIO pin for internal ADC, 0-3 for ADS1115).
   * @return Raw ADC value (e.g., 0-4095 for 12-bit, -32768 to 32767 for signed 16-bit).
   */
  virtual int16_t readChannel(uint8_t channelIdentifier) = 0;

  /**
   * Gets the maximum positive raw value the ADC can produce in its current configuration.
   * This is used for scaling calculations.
   * @return Maximum positive ADC value (e.g., 4095 for ESP32 12-bit, 32767 for ADS1115 signed 16-bit).
   */
  virtual uint16_t getResolution() = 0;

  /**
   * Gets the effective reference voltage in millivolts for the current ADC configuration.
   * For ADCs with gain settings (like ADS1115), this should reflect the
   * voltage corresponding to the full-scale reading at the current gain.
   * @return Reference voltage in mV.
   */
  virtual uint16_t getReferenceVoltageMv() = 0;

  /**
   * Helper method to read the voltage from a channel in millivolts.
   * Uses readChannel(), getResolution(), and getReferenceVoltageMv().
   * @param channelIdentifier Identifier for the channel.
   * @return Voltage in millivolts, or NAN if reading fails or scaling factors are zero.
   */
  virtual float readVoltageMv(uint8_t channelIdentifier) {
    int16_t rawValue = readChannel(channelIdentifier);
    uint16_t resolution = getResolution();
    uint16_t vref_mV = getReferenceVoltageMv();

    // Basic check for valid scaling factors
    if (resolution == 0 || vref_mV == 0) {
        // Consider logging an error or returning a specific error code if NAN isn't suitable
        return NAN; 
    }

    // Perform scaling
    // Note: We cast rawValue to float *before* multiplication to avoid integer overflow
    // if rawValue * vref_mV exceeds the limits of int32_t, though unlikely with mV.
    float voltage_mV = (float)rawValue * ((float)vref_mV / (float)resolution);

    return voltage_mV;
  }
};

#endif // ANALOG_READER_H 