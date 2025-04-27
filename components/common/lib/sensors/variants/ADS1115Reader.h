#ifndef ADS1115_READER_H
#define ADS1115_READER_H

#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include "../src/AnalogReader.h"

/**
 * Implementation of AnalogReader for the ADS1115 ADC.
 * Requires the Adafruit ADS1X15 library.
 * 
 * Note: Ensure Wire.begin() is called before using this class.
 */
class ADS1115Reader : public AnalogReader {
public:
  /**
   * Constructor.
   * @param ads Pointer to an already instantiated Adafruit_ADS1115 object.
   * @param defaultGain The gain setting to use initially. Defaults to +/-6.144V.
   */
  ADS1115Reader(Adafruit_ADS1115* ads, adsGain_t defaultGain = GAIN_ONE) 
    : _ads(ads), _currentGain(defaultGain) {}

  /**
   * Initializes the ADS1115 communication and sets the initial gain.
   * @return true if initialization was successful, false otherwise.
   */
  bool begin() override {
    if (!_ads) {
      return false; // Must provide a valid ADS1115 object pointer
    }
    // Adafruit_ADS1115::begin() initializes the sensor and Wire internally if needed.
    if (!_ads->begin()) { // Default address is often 0x48
      return false;
    }
    _ads->setGain(_currentGain); 
    return true;
  }

  /**
   * Reads the raw signed value from a specific ADS1115 single-ended channel.
   * @param channelIdentifier The ADS1115 channel number (0, 1, 2, or 3).
   * @return Raw signed ADC value (-32768 to 32767), or 0 if channel is invalid or read fails.
   */
  int16_t readChannel(uint8_t channelIdentifier) override {
    if (!_ads || channelIdentifier > 3) {
      return 0; // Invalid channel or object
    }
    // Reads single-ended value from the specified channel
    return _ads->readADC_SingleEnded(channelIdentifier);
  }

  /**
   * Returns the maximum positive resolution (15-bit for signed 16-bit).
   */
  uint16_t getResolution() override {
    // ADS1115 is 16-bit signed, max positive value is 2^15 - 1
    return 32767;
  }

  /**
   * Returns the effective reference voltage (full-scale range) in millivolts 
   * based on the currently configured gain.
   */
  uint16_t getReferenceVoltageMv() override {
    // Determine the voltage range based on the gain setting
    float fsr = 0.0f;
    switch (_currentGain) {
      case GAIN_TWOTHIRDS: // +/-6.144V
        fsr = 6144.0f;
        break;
      case GAIN_ONE:       // +/-4.096V
        fsr = 4096.0f;
        break;
      case GAIN_TWO:       // +/-2.048V
        fsr = 2048.0f;
        break;
      case GAIN_FOUR:      // +/-1.024V
        fsr = 1024.0f;
        break;
      case GAIN_EIGHT:     // +/-0.512V
        fsr = 512.0f;
        break;
      case GAIN_SIXTEEN:   // +/-0.256V
        fsr = 256.0f;
        break;
      default:
        fsr = 0.0f; // Should not happen
        break;
    }
    return (uint16_t)fsr;
  }

  /**
   * Sets the gain for the ADS1115.
   * This affects the full-scale voltage range and measurement resolution.
   * @param gain The desired gain setting (e.g., GAIN_ONE for +/-4.096V).
   */
  void setGain(adsGain_t gain) {
      if (!_ads) return;
      _ads->setGain(gain);
      _currentGain = gain;
  }

  /**
   * Gets the currently configured gain setting.
   * @return The current adsGain_t value.
   */
  adsGain_t getGain() const {
      return _currentGain;
  }

private:
  Adafruit_ADS1115* _ads; // Pointer to the ADS1115 object
  adsGain_t _currentGain; // Store the current gain setting
};

#endif // ADS1115_READER_H 