#ifndef KUNRAY_HALL_RPM_SENSOR_H
#define KUNRAY_HALL_RPM_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"
#include "common.pb.h"
#include "motors.pb.h"
#include "../include/Config.h" // Include Config.h for pin definitions

// Add platform-specific defines for ISR attributes if needed
// #if defined(ESP8266) || defined(ESP32) || defined(PLATFORM_ESP32)
// #define ICACHE_RAM_ATTR ICACHE_RAM_ATTR // REMOVED - Handled by framework
// #else
// #define ICACHE_RAM_ATTR
// #endif

// Forward declarations for ISRs removed

/**
 * KunrayHallRpmSensor - Hall sensor-based RPM measurement
 * 
 * Uses interrupts to count pulses from hall sensors to calculate RPM
 */
class KunrayHallRpmSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentId Component ID of this sensor (from RpmSensorLocation enum)
   * @param updateInterval Update interval in ms
   */
  KunrayHallRpmSensor(uint8_t componentId, uint16_t updateInterval = 100) : 
    Sensor(kart_common_ComponentType_MOTORS, componentId, kart_motors_MotorCommandId_RPM, kart_common_ValueType_UINT16, updateInterval) {
    _pulseCount = 0;
    _lastPulseTime = 0;
  }

  bool begin() override {
    // Pin mode configuration is removed as it caused issues.
    // Rely on default pin modes or configuration elsewhere.

    // Attach interrupts using attachInterruptArg and a static handler function
    // Ensure HALL_A_PIN, HALL_B_PIN, HALL_C_PIN are defined in Config.h
    // Note: ICACHE_RAM_ATTR is applied to the static handler itself, not here.
    attachInterruptArg(digitalPinToInterrupt(HALL_A_PIN), staticIsrHandler, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(HALL_B_PIN), staticIsrHandler, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(HALL_C_PIN), staticIsrHandler, this, CHANGE);
    
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
   * Increment pulse counter - Called by the static ISR handler.
   */
  void incrementPulse() { // Removed ICACHE_RAM_ATTR from here
    noInterrupts(); // Briefly disable interrupts to safely update shared variables
    _pulseCount++;
    _lastPulseTime = millis(); 
    interrupts(); // Re-enable interrupts
  }
  
  /**
   * Get the current RPM value
   * @return RPM value
   */
  uint16_t getRPM() const {
    return _lastRPM;
  }

private:
  volatile uint32_t _pulseCount;
  volatile uint32_t _lastPulseTime;
  uint16_t _lastRPM;
  SensorValue _sensorValue; // Reusable sensor value object
  
  // Variables for RPM calculation
  uint32_t _lastCalcTime;
  uint32_t _lastPulseCount;
  
  /**
   * Static ISR handler function.
   * Must be static to match the function pointer type required by attachInterruptArg.
   * The 'arg' passed will be the 'this' pointer of the specific KunrayHallRpmSensor instance.
   * Apply ICACHE_RAM_ATTR here as this code runs in ISR context.
   */
  static void ICACHE_RAM_ATTR staticIsrHandler(void* arg);
  
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

// Implementation of the static ISR handler
// Place it here in the header for simplicity, or move to a .cpp file if preferred.
ICACHE_RAM_ATTR void KunrayHallRpmSensor::staticIsrHandler(void* arg) {
    // Cast the argument back to our class instance
    KunrayHallRpmSensor* instance = static_cast<KunrayHallRpmSensor*>(arg);
    // Call the non-static member function
    if (instance) { // Basic null check
        instance->incrementPulse();
    }
}

#endif // KUNRAY_HALL_RPM_SENSOR_H 