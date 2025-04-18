#ifndef RPM_SENSOR_H
#define RPM_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "common.pb.h"
#include "motors.pb.h"
#include "../include/Config.h" // Include Config.h for pin definitions

// Forward declarations for ISRs if they are defined globally in main.cpp
// If they are member functions or static, this needs adjustment.
extern void hallSensorA_ISR(); 
extern void hallSensorB_ISR();
extern void hallSensorC_ISR();

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
    // ISR assignment removed - assuming global ISRs from main.cpp based on Config.h prototypes
  }

  bool begin() override {
    // Pin mode configuration is removed as it caused issues.
    // Rely on default pin modes or configuration elsewhere.

    // Attach interrupts using pins defined in Config.h
    // Ensure the ISR functions (hallSensorA_ISR, etc.) are accessible (e.g., declared extern)
    attachInterrupt(digitalPinToInterrupt(HALL_A_PIN), hallSensorA_ISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(HALL_B_PIN), hallSensorB_ISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(HALL_C_PIN), hallSensorC_ISR, CHANGE);
    
    // Initialize state variables
    _lastRPM = 0;
    _pulseCount = 0;
    _lastPulseTime = millis();
    _lastCalcTime = millis();
    _lastPulseCount = 0;

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
    uint16_t rpm = calculateRPM(); // Changed to uint16_t to match return type
    _baseSensorValue.uint16_value = rpm;
    return _baseSensorValue;
  }

  /**
   * Increment pulse counter
   * Called directly by the global ISRs defined in main.cpp
   */
  // This method might need to be static or accessed via a singleton instance
  // if called from global C-style ISRs. For now, assume main.cpp handles it.
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
  
// Make pulse count accessible if needed by global ISRs
// Consider making this instance accessible globally (e.g., via a pointer in main.cpp)
// volatile uint32_t _pulseCount = 0; 
// volatile uint32_t _lastPulseTime = 0;

private:
  volatile uint32_t _pulseCount;
  volatile uint32_t _lastPulseTime;
  // ISR function pointers removed
  uint16_t _lastRPM;
  SensorValue _sensorValue; // Reusable sensor value object
  
  // Variables for RPM calculation
  uint32_t _lastCalcTime;
  uint32_t _lastPulseCount;
  
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
    uint32_t timeDiff = currentTime - _lastCalcTime;

    // Prevent division by zero if timeDiff is too small
    if (timeDiff == 0) {
        return _lastRPM; // Return last calculated RPM
    }

    // Calculation factor assumes 6 state changes per electrical revolution
    // and 3 electrical revolutions per mechanical for this motor (adjust if needed)
    _lastRPM = (uint16_t)((countDiff * 60000UL) / (timeDiff * 6UL * 3UL)); 
    
    // Store values for next calculation
    _lastCalcTime = currentTime;
    _lastPulseCount = currentCount;
    
    return _lastRPM;
  }
};

#endif // RPM_SENSOR_H 