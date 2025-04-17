#ifndef RPM_SENSOR_H
#define RPM_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "common.pb.h"
#include "motors.pb.h"
/**
 * RpmSensor - Hall sensor-based RPM measurement
 * 
 * Uses interrupts to count pulses from hall sensors to calculate RPM
 */
class RpmSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentId Component ID of this sensor (from RpmSensorLocation enum)
   * @param updateInterval Update interval in ms
   */
  RpmSensor(uint8_t componentId, uint16_t updateInterval = 100) : 
    Sensor(kart_common_ComponentType_MOTORS, componentId, kart_motors_MotorCommandId_RPM, kart_common_ValueType_UINT16, updateInterval) {
    _pulseCount = 0;
    _lastPulseTime = 0;
    _isrA = hallSensorA_ISR;
    _isrB = hallSensorB_ISR;
    _isrC = hallSensorC_ISR;
  }

  bool begin() override {
    // Restore pin configuration and interrupt attachment here.
    // pinMode(HALL_A_PIN, INPUT); // Commented out - potential conflict or unnecessary
    // pinMode(HALL_B_PIN, INPUT); // Commented out - GPIO 3 (RX0) conflict?
    // pinMode(HALL_C_PIN, INPUT); // Commented out - GPIO 4 (CAN_INT) conflict?

    // Attach interrupts to the ISRs
    attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), _isrA, CHANGE);
    attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), _isrB, CHANGE); // GPIO 3, potential conflict
    attachInterrupt(digitalPinToInterrupt(HALL_C_PIN), _isrC, CHANGE);
    
    return true; // Indicate successful logical initialization
  }
  
  /**
   * Read RPM
   * @return SensorValue containing RPM
   */
  SensorValue read() override {
    // Wait for minimum time between calculations
    uint32_t currentTime = millis();
    uint32_t timeDiff = currentTime - _lastCalcTime;
    if (timeDiff < _updateInterval) {
      return _baseSensorValue;
    }

    // Calculate RPM
    uint32_t rpm = calculateRPM();
    _baseSensorValue.uint16_value = rpm;
    return _baseSensorValue;
  }

  /**
   * Increment pulse counter
   * Call this from interrupt handlers when hall sensor triggers
   */
  void incrementPulse() {
    _pulseCount++;
    _lastPulseTime = millis();
  }
  
  /**
   * Get the current RPM value
   * @return RPM value
   */
  uint16_t getRPM() const {
    return _lastRPM;
  }
  
private:
  volatile uint32_t _pulseCount = 0;
  volatile uint32_t _lastPulseTime = 0;
  void (*_isrA)(void);
  void (*_isrB)(void);
  void (*_isrC)(void);
  uint16_t _lastRPM = 0;
  SensorValue _sensorValue;    // Reusable sensor value object
  
  // Variables for RPM calculation
  uint32_t _lastCalcTime = 0;
  uint32_t _lastPulseCount = 0;
  
  /**
   * Calculate RPM based on pulse count
   * @return RPM value
   */
  uint16_t calculateRPM() {
    uint32_t currentTime = millis();
    
    // If no pulses for over 2 seconds, motor is stopped
    if (currentTime - _lastPulseTime > 2000) {
      _lastRPM = 0;
      return 0;
    }
    
    // Get pulse count (thread-safe)
    noInterrupts();
    uint32_t currentCount = _pulseCount;
    interrupts();
    
    // Calculate pulse difference
    uint32_t countDiff;
    if (currentCount >= _lastPulseCount) {
      countDiff = currentCount - _lastPulseCount;
    } else {
      // Handle overflow
      countDiff = (0xFFFFFFFF - _lastPulseCount) + currentCount + 1;
    }
    
    // If no pulses during this period, keep last RPM but decay it
    if (countDiff == 0) {
      // Decay RPM if no new pulses (makes UI smoother)
      if (_lastRPM > 0) {
        _lastRPM = (uint16_t)(_lastRPM * 0.9f);
      }
      return _lastRPM;
    }
    
    // Calculate RPM based on pulse count and time difference
    // For a 3-phase BLDC motor with 3 hall sensors, one revolution creates 6 pulses
    // RPM = (pulses / 6) * (60000 / milliseconds)
    // For Kunray MY1020 with ~4 pole pairs, divide by 4 to get true mechanical RPM
    uint32_t timeDiff = currentTime - _lastCalcTime;
    _lastRPM = (uint16_t)((countDiff * 60000UL) / (timeDiff * 6UL * 3UL));
    
    // Simple sanity check - cap at reasonable maximum
    // if (_lastRPM > 6000) {
    //   _lastRPM = 0; // Likely an erroneous reading
    // }
    
    // Store values for next calculation
    _lastCalcTime = currentTime;
    _lastPulseCount = currentCount;
    
    return _lastRPM;
  }
};

#endif // RPM_SENSOR_H 