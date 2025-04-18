#ifndef ATGM336H_SENSOR_H
#define ATGM336H_SENSOR_H

#include <Arduino.h>
#include <TinyGPS++.h>
#include "../src/Sensor.h"
#include "common.pb.h" // Include common types like ComponentType, ValueType
#include "navigation.pb.h" // Assuming GPS command IDs are defined here

// Removed placeholder definitions for GPS related command IDs
// They should now be defined in navigation.pb.h
// Example: kart_navigation_NavigationCommandId_LATITUDE

/**
 * ATGM336HSensor - Reads GPS/BDS data from ATGM336H module.
 *
 * Communicates over UART (Serial). Parses NMEA sentences using TinyGPS++.
 * Returns a specific value (Lat, Lon, Alt, Speed, Course, Sats, HDOP, Fix)
 * based on the commandId provided during initialization.
 *
 * Note: For robust performance, ensure processSerial() or equivalent
 *       is called frequently to feed data to TinyGPS++.
 *       The current implementation reads serial within read(), which might
 *       miss data if the updateInterval is too long.
 */
class ATGM336HSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type (e.g., NAVIGATION)
   * @param componentId Component ID for this sensor instance (e.g., GPS_PRIMARY)
   * @param commandId Command ID determines what read() returns (e.g., LATITUDE)
   * @param serialPort HardwareSerial instance connected to the GPS module (e.g., &Serial1)
   * @param updateInterval Update interval in ms
   * @param baudRate Baud rate for the serial connection (default 9600 for ATGM336H)
   */
  ATGM336HSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId, // Determines which value is returned
    HardwareSerial* serialPort,
    uint16_t updateInterval = 1000,
    uint32_t baudRate = 9600
  ) : Sensor(componentType, componentId, commandId,
             getValueTypeForCommand(commandId), // Determine ValueType based on command
             updateInterval),
      _serial(serialPort),
      _baudRate(baudRate)
  {
      // Set the specific protobuf value field tag based on commandId
      // Assuming SensorValue union doesn't have nanopb oneof tags
      // setValueFieldTag(commandId);
  }

  /**
   * Initialize the Serial connection for the GPS module
   */
  bool begin() override {
    if (_serial) {
      _serial->begin(_baudRate);
      #ifdef DEBUG_ENABLED
        Serial.print("GPS Serial configured at ");
        Serial.println(_baudRate);
      #endif
      return true;
    }
    #ifdef DEBUG_ENABLED
      Serial.println("GPS Serial port pointer is null!");
    #endif
    return false;
  }

  /**
   * Process available serial data and feed it to TinyGPS++.
   * Recommended to call this frequently from the main loop or a task.
   */
  void processSerial() {
      if (!_serial) return;
      while (_serial->available() > 0) {
          _gps.encode(_serial->read());
      }
  }

  /**
   * Read GPS data
   * @return SensorValue containing the specific value (e.g., Latitude, Speed)
   *         based on the commandId the sensor was initialized with.
   *
   * Note: This implementation calls processSerial() internally.
   */
  SensorValue read() override {
    // Process any pending serial data first
    processSerial();

    // Update the SensorValue based on the commandId from navigation.pb.h
    switch (_commandId) {
      case kart_navigation_NavigationCommandId_LATITUDE:
        // Use isUpdated() to only update when TinyGPS++ has new data for this field
        if (_gps.location.isValid() && _gps.location.isUpdated()) {
          _baseSensorValue.float_value = (float)_gps.location.lat();
        }
        break;
      case kart_navigation_NavigationCommandId_LONGITUDE:
        if (_gps.location.isValid() && _gps.location.isUpdated()) {
           _baseSensorValue.float_value = (float)_gps.location.lng();
        }
        break;
      case kart_navigation_NavigationCommandId_ALTITUDE:
        if (_gps.altitude.isValid() && _gps.altitude.isUpdated()) {
           _baseSensorValue.float_value = (float)_gps.altitude.meters();
        }
        break;
      case kart_navigation_NavigationCommandId_SPEED:
        if (_gps.speed.isValid() && _gps.speed.isUpdated()) {
           _baseSensorValue.float_value = (float)_gps.speed.kmph();
        }
        break;
      case kart_navigation_NavigationCommandId_COURSE:
        if (_gps.course.isValid() && _gps.course.isUpdated()) {
           _baseSensorValue.float_value = (float)_gps.course.deg();
        }
        break;
      case kart_navigation_NavigationCommandId_SATELLITES:
        if (_gps.satellites.isValid() && _gps.satellites.isUpdated()) {
           // Ensure value fits within uint8_t - Fix min() call types
           _baseSensorValue.uint8_value = (uint8_t)min((uint32_t)255, _gps.satellites.value());
        }
        break;
      case kart_navigation_NavigationCommandId_HDOP:
        if (_gps.hdop.isValid() && _gps.hdop.isUpdated()) {
           _baseSensorValue.float_value = (float)(_gps.hdop.value() / 100.0); // HDOP is in hundredths
        }
        break;
      case kart_navigation_NavigationCommandId_GPS_FIX_STATUS:
         // Use isUpdated() to only update when fix status potentially changes
         // Note: TinyGPS++ location.isValid() check reflects basic fix state
         if (_gps.location.isUpdated()) {
             if (_gps.location.isValid()) {
                 // Basic check for any valid fix. More detail (DGPS) needs NMEA parsing.
                 _baseSensorValue.uint8_value = kart_navigation_GpsFixStatus_FIX_ACQUIRED;
             } else {
                 _baseSensorValue.uint8_value = kart_navigation_GpsFixStatus_NO_FIX;
             }
         }
         // For differential fix, you'd need to parse the GGA sentence mode indicator.
         break;
      default:
        // Handle unsupported command ID
        #ifdef DEBUG_ENABLED
          Serial.print("ATGM336HSensor: Unsupported commandId: ");
          Serial.println(_commandId);
        #endif
        // Set to a default invalid value based on expected type
        // Using UINT24 as placeholder for FLOAT
        if (_valueType == kart_common_ValueType_UINT24) _baseSensorValue.float_value = NAN; // Still set float field, but type is UINT24
        else if (_valueType == kart_common_ValueType_UINT8) _baseSensorValue.uint8_value = 0xFF; // Or other invalid marker
        // Add other types as needed
        break;
    }

    // Return the potentially updated value (holds last known value if no update)
    return _baseSensorValue;
  }

// Add a public getter for the internal TinyGPSPlus object if needed for direct access (e.g., debug status)
  TinyGPSPlus& getGpsObject() { return _gps; }

// Change _gps to public if direct access is preferred over getter
// public:
//  TinyGPSPlus _gps;      // TinyGPS++ instance

// --- Configuration Methods --- 

/**
 * Sets the GPS module update rate.
 * @param rateValue Value from kart_navigation_GpsUpdateRateValue enum (e.g., 1, 5, 10 Hz).
 * @return True if the command was sent, False otherwise.
 */
bool setUpdateRate(uint8_t rateValue) {
  if (!_serial) return false;
  // Example command for u-blox (ATGM336H often uses compatible commands)
  // $PCAS02,period*CS<CR><LF>  where period is in ms (e.g., 1000ms for 1Hz, 200ms for 5Hz, 100ms for 10Hz)
  uint16_t periodMs;
  switch (rateValue) {
      case kart_navigation_GpsUpdateRateValue_RATE_1_HZ: periodMs = 1000; break;
      case kart_navigation_GpsUpdateRateValue_RATE_5_HZ: periodMs = 200; break;
      case kart_navigation_GpsUpdateRateValue_RATE_10_HZ: periodMs = 100; break;
      default:
          #ifdef DEBUG_ENABLED
            Serial.printf("ATGM336H: Invalid Update Rate Value: %d\n", rateValue);
          #endif
          return false;
  }

  char cmdBuffer[30];
  snprintf(cmdBuffer, sizeof(cmdBuffer), "PCAS02,%d", periodMs);
  return sendNmeaCommand(cmdBuffer);
}

/**
 * Sets the enabled GNSS constellations (Placeholder - requires specific module commands).
 * @param constellationValue Value from kart_navigation_GnssConstellationValue enum.
 * @return True if the command was sent, False otherwise.
 */
bool setGnssConstellation(uint8_t constellationValue) {
   if (!_serial) return false;
   #ifdef DEBUG_ENABLED
     Serial.printf("ATGM336H: Set GNSS Constellation (%d) - Not Implemented\n", constellationValue);
   #endif
  // TODO: Implement sending the specific NMEA or proprietary command for ATGM336H
  // Example: This often involves complex commands like $PCAS14 or UBX CFG-GNSS
  // sendNmeaCommand("..."); 
  return false; // Placeholder
}

/**
 * Sets which NMEA sentences are output (Placeholder - requires specific module commands).
 * @param nmeaOutputValue Value from kart_navigation_NmeaOutputValue enum (might be a bitmask).
 * @return True if the command was sent, False otherwise.
 */
bool setNmeaOutput(uint8_t nmeaOutputValue) {
  if (!_serial) return false;
  #ifdef DEBUG_ENABLED
    Serial.printf("ATGM336H: Set NMEA Output (%d) - Not Implemented\n", nmeaOutputValue);
  #endif
  // TODO: Implement sending the specific NMEA or proprietary command ($PCAS03, $PCAS04, UBX CFG-MSG)
  // This often involves setting enable/disable flags for GGA, GLL, GSA, GSV, RMC, VTG etc.
  // sendNmeaCommand("...");
  return false; // Placeholder
}

/**
 * Sets the static navigation mode.
 * @param modeValue Value from kart_navigation_StaticNavigationValue enum (0=off, 1=on).
 * @return True if the command was sent, False otherwise.
 */
bool setStaticNavigation(uint8_t modeValue) {
   if (!_serial) return false;
  // Example command for u-blox (often compatible)
  // $PCAS06,mode*CS<CR><LF> (0=Portable/Off, 1=Stationary/On)
  if (modeValue > 1) { // Basic validation
     #ifdef DEBUG_ENABLED
       Serial.printf("ATGM336H: Invalid Static Navigation Value: %d\n", modeValue);
     #endif
     return false;
  }
  char cmdBuffer[20];
  snprintf(cmdBuffer, sizeof(cmdBuffer), "PCAS06,%d", modeValue);
  return sendNmeaCommand(cmdBuffer);
}

private:
  HardwareSerial* _serial; // Pointer to the Serial port instance
  uint32_t _baudRate;
  TinyGPSPlus _gps;      // TinyGPS++ instance

  /**
   * Helper function to determine the ValueType based on the command ID.
   */
  static kart_common_ValueType getValueTypeForCommand(uint8_t commandId) {
      switch (commandId) {
          case kart_navigation_NavigationCommandId_LATITUDE:
          case kart_navigation_NavigationCommandId_LONGITUDE:
          case kart_navigation_NavigationCommandId_ALTITUDE:
          case kart_navigation_NavigationCommandId_SPEED:
          case kart_navigation_NavigationCommandId_COURSE:
          case kart_navigation_NavigationCommandId_HDOP:
              return kart_common_ValueType_UINT24; // Using UINT24 as placeholder for FLOAT
          case kart_navigation_NavigationCommandId_SATELLITES:
          case kart_navigation_NavigationCommandId_GPS_FIX_STATUS:
              return kart_common_ValueType_UINT8;
          default:
              #ifdef DEBUG_ENABLED
                Serial.print("ATGM336H: Unknown commandId for ValueType: ");
                Serial.println(commandId);
              #endif
              return kart_common_ValueType_UINT24; // Default placeholder
      }
  }

  // Commenting out as SensorValue union likely doesn't support nanopb oneof tags
  /*
   void setValueFieldTag(uint8_t commandId) {
        switch (commandId) {
          case kart_navigation_NavigationCommandId_LATITUDE:
          // ... other float cases ...
          case kart_navigation_NavigationCommandId_HDOP:
              _baseSensorValue.which_value = kart_common_SensorValue_float_value_tag;
              break;
          case kart_navigation_NavigationCommandId_SATELLITES:
          case kart_navigation_NavigationCommandId_GPS_FIX_STATUS:
               _baseSensorValue.which_value = kart_common_SensorValue_uint8_value_tag;
               break;
          default:
               #ifdef DEBUG_ENABLED
                 Serial.print("ATGM336H: Unknown commandId for FieldTag: ");
                 Serial.println(commandId);
               #endif
               // Set a default, although this case shouldn't ideally happen
               _baseSensorValue.which_value = kart_common_SensorValue_float_value_tag;
               break;
      }
   }
  */

  /**
   * Helper function to construct and send a NMEA command sentence with checksum.
   * @param commandPayload The NMEA command payload (e.g., "PCAS02,1000").
   * @return True if the command was sent, False otherwise.
   */
  bool sendNmeaCommand(const char* commandPayload) {
      if (!_serial || !commandPayload) return false;

      uint8_t checksum = 0;
      for (const char* p = commandPayload; *p; ++p) {
          checksum ^= *p;
      }

      char finalCmd[strlen(commandPayload) + 6]; // $ + payload + * + CS + CR + LF + \0
      snprintf(finalCmd, sizeof(finalCmd), "$%s*%02X\r\n", commandPayload, checksum);

      #ifdef DEBUG_ENABLED
        Serial.print("ATGM336H: Sending command: ");
        Serial.print(finalCmd);
      #endif

      size_t written = _serial->print(finalCmd);
      return (written == strlen(finalCmd));
  }
};

#endif // ATGM336H_SENSOR_H 