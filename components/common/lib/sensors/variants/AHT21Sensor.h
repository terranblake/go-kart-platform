#ifndef AHT21_SENSOR_H
#define AHT21_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include "../src/Sensor.h"
#include "common.pb.h" // Include common types like ComponentType, ValueType
#include "navigation.pb.h" // Assuming TEMP and HUMIDITY command IDs are here now

/**
 * AHT21Sensor - Measures temperature and humidity using AHT21 sensor.
 *
 * Communicates over I2C. Returns either temperature or humidity based on
 * the commandId provided during initialization.
 */
class AHT21Sensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type (e.g., NAVIGATION)
   * @param componentId Component ID for this sensor instance (e.g., ENVIRONMENT_PRIMARY)
   * @param commandId Command ID (e.g., TEMPERATURE_AMBIENT, HUMIDITY_RELATIVE) determines what read() returns
   * @param updateInterval Update interval in ms
   * @param wire TwoWire instance (e.g., &Wire)
   */
  AHT21Sensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId, // Determines if temp or humidity is returned
    uint16_t updateInterval = 2000, // AHT21 takes time to measure
    TwoWire *wire = &Wire
  ) : Sensor(componentType, componentId, commandId,
             // Determine ValueType based on actual command ID from protobuf
             (commandId == kart_navigation_NavigationCommandId_TEMPERATURE_AMBIENT) ? kart_common_ValueType_INT16 :
             (commandId == kart_navigation_NavigationCommandId_HUMIDITY_RELATIVE) ? kart_common_ValueType_UINT8 :
             kart_common_ValueType_UINT24, // Fallback - Using UINT24 as placeholder for FLOAT
             updateInterval),
      _wire(wire),
      _lastTempC(0.0f),
      _lastHumidity(0.0f) {
    // Determine which protobuf value field to use based on commandId
    // Assuming SensorValue union in Sensor.h doesn't have nanopb oneof tags
    // if (commandId == kart_navigation_NavigationCommandId_TEMPERATURE_AMBIENT) {
    //     _baseSensorValue.which_value = kart_common_SensorValue_int16_value_tag;
    // } else if (commandId == kart_navigation_NavigationCommandId_HUMIDITY_RELATIVE) {
    //      _baseSensorValue.which_value = kart_common_SensorValue_uint8_value_tag;
    // }
    // Consider adding error handling for unexpected commandId
  }

  /**
   * Initialize the AHT21 sensor
   */
  bool begin() override {
    if (!_aht.begin(_wire)) {
      #ifdef DEBUG_ENABLED // Use a common debug flag if available
        Serial.println("Failed to find AHTx0 sensor");
      #endif
      return false;
    }
    #ifdef DEBUG_ENABLED
      Serial.println("AHTx0 Found!");
    #endif
    return true;
  }

  /**
   * Read sensor data (temperature and humidity)
   * @return SensorValue containing either temp (tenths of °C as int16_t) or humidity (% as uint8_t)
   *         based on the commandId the sensor was initialized with.
   */
  SensorValue read() override {
    sensors_event_t humidity, temp;
    if (_aht.getEvent(&humidity, &temp)) { // Read both events
      _lastTempC = temp.temperature;
      _lastHumidity = humidity.relative_humidity;

      // Return value based on configured command ID
      if (_commandId == kart_navigation_NavigationCommandId_TEMPERATURE_AMBIENT) {
        // Convert to tenths of a degree C and store as int16_t
        _baseSensorValue.int16_value = (int16_t)(_lastTempC * 10.0f);
      } else if (_commandId == kart_navigation_NavigationCommandId_HUMIDITY_RELATIVE) {
        // Clamp humidity to 0-100 range and store as uint8_t
        uint8_t humidity_percent = (uint8_t)constrain(_lastHumidity, 0.0f, 100.0f);
         _baseSensorValue.uint8_value = humidity_percent;
      } else {
        // Handle unsupported command ID or default case
        // Maybe return an error value or default?
      }
    } else {
      #ifdef DEBUG_ENABLED
        Serial.println("Failed to read AHTx0 sensor");
      #endif
      // Return last known value or an error indicator?
      // For now, return the existing _baseSensorValue which holds the last successful read
    }
    return _baseSensorValue;
  }

  // Optional: Provide getters for the raw values if needed elsewhere
  float getLastTemperatureC() { return _lastTempC; }
  float getLastHumidity() { return _lastHumidity; }

private:
  Adafruit_AHTX0 _aht;
  TwoWire* _wire;
  float _lastTempC;
  float _lastHumidity;
};

#endif // AHT21_SENSOR_H 