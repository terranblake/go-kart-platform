#ifndef GY521_SENSOR_H
#define GY521_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "../src/Sensor.h"
#include "common.pb.h" // Include common types like ComponentType, ValueType
#include "navigation.pb.h" // Assuming IMU command IDs are defined here

// Removed placeholder definitions for ACCEL_*, GYRO_*, IMU_TEMP
// They should now be defined in navigation.pb.h
// Example: kart_navigation_NavigationCommandId_ACCEL_X

/**
 * GY521Sensor - Measures acceleration and rotation using MPU-6050.
 *
 * Communicates over I2C. Returns a specific value (Accel X/Y/Z, Gyro X/Y/Z, Temp)
 * based on the commandId provided during initialization.
 */
class GY521Sensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type (e.g., NAVIGATION)
   * @param componentId Component ID for this sensor instance (e.g., IMU_PRIMARY)
   * @param commandId Command ID (e.g., ACCEL_X, GYRO_Y) determines what read() returns
   * @param updateInterval Update interval in ms
   * @param wire TwoWire instance (e.g., &Wire)
   * @param address I2C address of the MPU-6050
   */
  GY521Sensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId, // Determines which value is returned
    uint16_t updateInterval = 100,
    TwoWire *wire = &Wire,
    uint8_t address = MPU6050_I2CADDR_DEFAULT
  ) : Sensor(componentType, componentId, commandId,
             kart_common_ValueType_UINT24, // Using UINT24 as placeholder for FLOAT
             updateInterval),
      _wire(wire),
      _address(address),
      _lastAccelX(0.0f), _lastAccelY(0.0f), _lastAccelZ(0.0f),
      _lastGyroX(0.0f), _lastGyroY(0.0f), _lastGyroZ(0.0f),
      _lastChipTempC(0.0f)
  {
    // Assuming SensorValue union in Sensor.h doesn't have nanopb oneof tags
    // _baseSensorValue.which_value = kart_common_SensorValue_float_value_tag;
  }

  /**
   * Initialize the MPU-6050 sensor
   */
  bool begin() override {
    if (!_mpu.begin(_address, _wire)) {
      #ifdef DEBUG_ENABLED
        Serial.println("Failed to find MPU6050 chip");
      #endif
      return false;
    }
    #ifdef DEBUG_ENABLED
      Serial.println("MPU6050 Found!");
      // Optionally print default settings after begin?
      // Adafruit_MPU6050 doesn't have easy getters for current range/filter
    #endif

    // Set sensor ranges (optional, defaults are reasonable)
    // Example: _mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    // Example: _mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    // Example: _mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    return true;
  }

  /**
   * Read sensor data (accel, gyro, temp)
   * @return SensorValue containing the specific value (e.g., Accel X, Gyro Y)
   *         based on the commandId the sensor was initialized with.
   */
  SensorValue read() override {
    sensors_event_t a, g, temp;
    if (_mpu.getEvent(&a, &g, &temp)) {
      // Store last known good values
      _lastAccelX = a.acceleration.x;
      _lastAccelY = a.acceleration.y;
      _lastAccelZ = a.acceleration.z;
      _lastGyroX = g.gyro.x;
      _lastGyroY = g.gyro.y;
      _lastGyroZ = g.gyro.z;
      _lastChipTempC = temp.temperature;

      // Return value based on configured command ID from navigation.pb.h
      switch (_commandId) {
        case kart_navigation_NavigationCommandId_ACCEL_X:
          _baseSensorValue.float_value = _lastAccelX;
          break;
        case kart_navigation_NavigationCommandId_ACCEL_Y:
          _baseSensorValue.float_value = _lastAccelY;
          break;
        case kart_navigation_NavigationCommandId_ACCEL_Z:
          _baseSensorValue.float_value = _lastAccelZ;
          break;
        case kart_navigation_NavigationCommandId_GYRO_X:
          _baseSensorValue.float_value = _lastGyroX;
          break;
        case kart_navigation_NavigationCommandId_GYRO_Y:
          _baseSensorValue.float_value = _lastGyroY;
          break;
        case kart_navigation_NavigationCommandId_GYRO_Z:
          _baseSensorValue.float_value = _lastGyroZ;
          break;
        case kart_navigation_NavigationCommandId_IMU_TEMPERATURE:
          _baseSensorValue.float_value = _lastChipTempC;
          break;
        default:
          // Handle unsupported command ID - perhaps return 0 or NaN?
          #ifdef DEBUG_ENABLED
            Serial.print("GY521Sensor: Unsupported commandId: ");
            Serial.println(_commandId);
          #endif
          _baseSensorValue.float_value = NAN; // Use Not-a-Number for invalid float
          break;
      }
    } else {
      #ifdef DEBUG_ENABLED
        Serial.println("Failed to read MPU6050 sensor");
      #endif
      // Return last known value or an error indicator?
      // Setting to NAN might be appropriate on error
       _baseSensorValue.float_value = NAN;
    }
    return _baseSensorValue;
  }

  // Optional: Getters for raw values
  float getLastAccelX() { return _lastAccelX; }
  float getLastAccelY() { return _lastAccelY; }
  float getLastAccelZ() { return _lastAccelZ; }
  float getLastGyroX() { return _lastGyroX; }
  float getLastGyroY() { return _lastGyroY; }
  float getLastGyroZ() { return _lastGyroZ; }
  float getLastChipTempC() { return _lastChipTempC; }

  // --- Configuration Methods --- 

  /**
   * Set the accelerometer measurement range.
   * @param rangeValue Value from kart_navigation_AccelerometerRangeValue enum.
   * @return True if successful, False otherwise.
   */
  bool setAccelerometerRange(uint8_t rangeValue) {
    mpu6050_accel_range_t accelRange;
    switch (rangeValue) {
      case kart_navigation_AccelerometerRangeValue_RANGE_2_G:
        accelRange = MPU6050_RANGE_2_G;
        break;
      case kart_navigation_AccelerometerRangeValue_RANGE_4_G:
        accelRange = MPU6050_RANGE_4_G;
        break;
      case kart_navigation_AccelerometerRangeValue_RANGE_8_G:
        accelRange = MPU6050_RANGE_8_G;
        break;
      case kart_navigation_AccelerometerRangeValue_RANGE_16_G:
        accelRange = MPU6050_RANGE_16_G;
        break;
      default:
        #ifdef DEBUG_ENABLED
          Serial.printf("GY521: Invalid Accelerometer Range Value: %d\n", rangeValue);
        #endif
        return false;
    }
    _mpu.setAccelerometerRange(accelRange);
    #ifdef DEBUG_ENABLED
      Serial.printf("GY521: Set Accelerometer Range to: %d\n", rangeValue);
    #endif
    // Add delay? MPU6050 needs time to settle after range change.
    delay(10); 
    return true; 
  }

  /**
   * Set the gyroscope measurement range.
   * @param rangeValue Value from kart_navigation_GyroscopeRangeValue enum.
   * @return True if successful, False otherwise.
   */
  bool setGyroscopeRange(uint8_t rangeValue) {
    mpu6050_gyro_range_t gyroRange;
     switch (rangeValue) {
      case kart_navigation_GyroscopeRangeValue_RANGE_250_DEG:
        gyroRange = MPU6050_RANGE_250_DEG;
        break;
      case kart_navigation_GyroscopeRangeValue_RANGE_500_DEG:
        gyroRange = MPU6050_RANGE_500_DEG;
        break;
      case kart_navigation_GyroscopeRangeValue_RANGE_1000_DEG:
        gyroRange = MPU6050_RANGE_1000_DEG;
        break;
      case kart_navigation_GyroscopeRangeValue_RANGE_2000_DEG:
        gyroRange = MPU6050_RANGE_2000_DEG;
        break;
      default:
         #ifdef DEBUG_ENABLED
          Serial.printf("GY521: Invalid Gyroscope Range Value: %d\n", rangeValue);
        #endif
        return false;
    }
    _mpu.setGyroRange(gyroRange);
     #ifdef DEBUG_ENABLED
      Serial.printf("GY521: Set Gyroscope Range to: %d\n", rangeValue);
    #endif
    delay(10); // Delay for settling
    return true;
  }

  /**
   * Set the Digital Low Pass Filter bandwidth.
   * @param bwValue Value from kart_navigation_FilterBandwidthValue enum.
   * @return True if successful, False otherwise.
   */
  bool setFilterBandwidth(uint8_t bwValue) {
    mpu6050_bandwidth_t filterBw;
    switch (bwValue) {
      case kart_navigation_FilterBandwidthValue_BAND_260_HZ:
        filterBw = MPU6050_BAND_260_HZ;
        break;
      case kart_navigation_FilterBandwidthValue_BAND_184_HZ:
         filterBw = MPU6050_BAND_184_HZ;
        break;
       case kart_navigation_FilterBandwidthValue_BAND_94_HZ:
         filterBw = MPU6050_BAND_94_HZ;
        break;
       case kart_navigation_FilterBandwidthValue_BAND_44_HZ:
         filterBw = MPU6050_BAND_44_HZ;
        break;
       case kart_navigation_FilterBandwidthValue_BAND_21_HZ:
         filterBw = MPU6050_BAND_21_HZ;
        break;
       case kart_navigation_FilterBandwidthValue_BAND_10_HZ:
         filterBw = MPU6050_BAND_10_HZ;
        break;
       case kart_navigation_FilterBandwidthValue_BAND_5_HZ:
         filterBw = MPU6050_BAND_5_HZ;
        break;
      default:
        #ifdef DEBUG_ENABLED
          Serial.printf("GY521: Invalid Filter Bandwidth Value: %d\n", bwValue);
        #endif
        return false;
    }
    _mpu.setFilterBandwidth(filterBw);
    #ifdef DEBUG_ENABLED
      Serial.printf("GY521: Set Filter Bandwidth to: %d\n", bwValue);
    #endif
    delay(10); // Delay for settling
    return true;
  }

  /**
   * Trigger an IMU calibration routine (Placeholder).
   * Actual implementation would depend on the chosen calibration method.
   */
  bool triggerCalibration() {
    #ifdef DEBUG_ENABLED
      Serial.println("GY521: Calibration triggered (Not implemented).");
    #endif
    // TODO: Implement calibration logic (e.g., averaging readings while stationary)
    // This might involve setting flags and processing over multiple loop iterations.
    return true; // Placeholder
  }

private:
  Adafruit_MPU6050 _mpu;
  TwoWire* _wire;
  uint8_t _address;

  // Store last read values
  float _lastAccelX, _lastAccelY, _lastAccelZ;
  float _lastGyroX, _lastGyroY, _lastGyroZ;
  float _lastChipTempC;
};

#endif // GY521_SENSOR_H 