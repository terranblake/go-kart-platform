#ifndef GY521_SENSOR_H
#define GY521_SENSOR_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "../src/Sensor.h"
#include "common.pb.h" // Include common types like ComponentType, ValueType
#include "navigation.pb.h" // Assuming IMU command IDs are defined here
#include <vector> // Include for std::vector
#include <numeric> // Include for std::accumulate

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
    // Reset offsets on begin
    _accelXOffset = _accelYOffset = _accelZOffset = 0.0f;
    _gyroXOffset = _gyroYOffset = _gyroZOffset = 0.0f;

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
   * Read sensor data (accel, gyro, temp) - Now applies calibration offsets.
   * @return SensorValue containing the specific value (e.g., Accel X, Gyro Y)
   *         based on the commandId the sensor was initialized with.
   */
  SensorValue read() override {
    sensors_event_t a, g, temp;
    if (_mpu.getEvent(&a, &g, &temp)) {
      // Apply calibration offsets
      _lastAccelX = a.acceleration.x - _accelXOffset;
      _lastAccelY = a.acceleration.y - _accelYOffset;
      _lastAccelZ = a.acceleration.z - _accelZOffset;
      _lastGyroX = g.gyro.x - _gyroXOffset;
      _lastGyroY = g.gyro.y - _gyroYOffset;
      _lastGyroZ = g.gyro.z - _gyroZOffset;
      _lastChipTempC = temp.temperature; // Temperature typically doesn't need offset calibration

      // Return value based on configured command ID
      switch (_commandId) {
        case kart_navigation_NavigationCommandId_ACCEL_X:
          _baseSensorValue.float_value = _lastAccelX;
          break;
        case kart_navigation_NavigationCommandId_ACCEL_Y:
          _baseSensorValue.float_value = _lastAccelY;
          break;
        case kart_navigation_NavigationCommandId_ACCEL_Z:
           // Report Z relative to gravity (0 when level, ~9.8 when vertical)
           // We subtract the offset which was calculated relative to ~9.8, 
           // so the result should be near 0 when level during reading.
           // If you want absolute Z including gravity, don't subtract G during offset calculation.
           _baseSensorValue.float_value = _lastAccelZ;
           // Alternatively: _baseSensorValue.float_value = a.acceleration.z; // Raw Z
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
          #ifdef DEBUG_ENABLED
            Serial.print("GY521Sensor: Unsupported commandId: ");
            Serial.println(_commandId);
          #endif
          _baseSensorValue.float_value = NAN; 
          break;
      }
    } else {
      #ifdef DEBUG_ENABLED
        Serial.println("Failed to read MPU6050 sensor");
      #endif
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
   * Trigger an IMU calibration routine.
   * Reads multiple samples while assuming the sensor is stationary and level.
   * Calculates average offsets for accelerometer and gyroscope.
   * NOTE: Assumes Z-axis is aligned with gravity (~9.8 m/s^2).
   * NOTE: Offsets are NOT persistent across resets without NVS implementation.
   */
  bool triggerCalibration() {
    #ifdef DEBUG_ENABLED
      Serial.println("GY521: Starting Calibration - Keep sensor stationary and level!");
    #endif

    const int numSamples = 100;
    std::vector<float> ax, ay, az, gx, gy, gz;
    ax.reserve(numSamples); ay.reserve(numSamples); az.reserve(numSamples);
    gx.reserve(numSamples); gy.reserve(numSamples); gz.reserve(numSamples);

    // Discard initial readings
    for (int i=0; i<10; ++i) {
       sensors_event_t a, g, temp;
       _mpu.getEvent(&a, &g, &temp);
       delay(5);
    }

    // Collect samples
    for (int i=0; i < numSamples; ++i) {
       sensors_event_t a, g, temp;
       if (_mpu.getEvent(&a, &g, &temp)) {
          ax.push_back(a.acceleration.x);
          ay.push_back(a.acceleration.y);
          az.push_back(a.acceleration.z);
          gx.push_back(g.gyro.x);
          gy.push_back(g.gyro.y);
          gz.push_back(g.gyro.z);
       } else {
          #ifdef DEBUG_ENABLED
             Serial.println("GY521: Error reading sensor during calibration!");
          #endif
          return false; // Abort calibration on error
       }
       delay(20); // Small delay between samples
    }

    // Calculate averages
    float avgAx = std::accumulate(ax.begin(), ax.end(), 0.0f) / numSamples;
    float avgAy = std::accumulate(ay.begin(), ay.end(), 0.0f) / numSamples;
    float avgAz = std::accumulate(az.begin(), az.end(), 0.0f) / numSamples;
    float avgGx = std::accumulate(gx.begin(), gx.end(), 0.0f) / numSamples;
    float avgGy = std::accumulate(gy.begin(), gy.end(), 0.0f) / numSamples;
    float avgGz = std::accumulate(gz.begin(), gz.end(), 0.0f) / numSamples;

    // Calculate offsets (deviation from ideal stationary state)
    // Assumes sensor is level, so ideal Accel X/Y = 0, Accel Z = ~9.81, Gyro X/Y/Z = 0
    _accelXOffset = avgAx;
    _accelYOffset = avgAy;
    // Offset Z relative to gravity. Adjust 9.81 if needed.
    const float gravity_ms2 = 9.80665f; 
    _accelZOffset = avgAz - gravity_ms2; 
    _gyroXOffset = avgGx;
    _gyroYOffset = avgGy;
    _gyroZOffset = avgGz;

    #ifdef DEBUG_ENABLED
      Serial.println("GY521: Calibration Complete.");
      Serial.printf("  Offsets Accel (X,Y,Z): %.4f, %.4f, %.4f\n", _accelXOffset, _accelYOffset, _accelZOffset);
      Serial.printf("  Offsets Gyro (X,Y,Z):  %.4f, %.4f, %.4f\n", _gyroXOffset, _gyroYOffset, _gyroZOffset);
    #endif

    return true; 
  }

private:
  Adafruit_MPU6050 _mpu;
  TwoWire* _wire;
  uint8_t _address;

  // Store last read values
  float _lastAccelX, _lastAccelY, _lastAccelZ;
  float _lastGyroX, _lastGyroY, _lastGyroZ;
  float _lastChipTempC;

  // Calibration Offsets
  float _accelXOffset = 0.0f;
  float _accelYOffset = 0.0f;
  float _accelZOffset = 0.0f;
  float _gyroXOffset = 0.0f;
  float _gyroYOffset = 0.0f;
  float _gyroZOffset = 0.0f;
};

#endif // GY521_SENSOR_H 