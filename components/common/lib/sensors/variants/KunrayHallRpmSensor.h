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
    // Explicitly configure Hall sensor pins as inputs with pull-ups
    pinMode(HALL_A_PIN, INPUT_PULLUP); // Reverted back to INPUT_PULLUP for use with external RC filter
    pinMode(HALL_B_PIN, INPUT_PULLUP);
    pinMode(HALL_C_PIN, INPUT_PULLUP);

    // Attach interrupt only to all hall sensor input pins
    attachInterruptArg(digitalPinToInterrupt(HALL_A_PIN), staticIsrHandler, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(HALL_B_PIN), staticIsrHandler, this, CHANGE);
    attachInterruptArg(digitalPinToInterrupt(HALL_C_PIN), staticIsrHandler, this, CHANGE);
    
    // Initialize state variables
    _lastRPM = 0;
    _pulseCount = 0;
    _lastPulseTime = millis();
    _lastCalcTime = millis();   // Time of the last RPM calculation
    _lastCalcCount = 0;   // Pulse count at the last RPM calculation

    return true; // Indicate successful logical initialization
  }
  
  /**
   * Read RPM
   * @return SensorValue containing RPM
   */
  SensorValue read() override {
    // Calculate RPM
    uint32_t rpm = calculateRPM(); // Changed to uint32_t to match return type
    _baseSensorValue.uint16_value = rpm;
    return _baseSensorValue;
  }

  /**
   * Increment pulse counter - Called by the static ISR handler.
   * Minimal version - No critical sections or debounce here.
   */
  void IRAM_ATTR incrementPulse() {
    // Debounce removed - relying solely on hardware filtering (e.g., RC filter)
    _pulseCount++;
  }
  
  /**
   * Get the current RPM value
   * @return RPM value
   */
  uint32_t getRPM() const {
    return _lastRPM;
  }

private:
  volatile uint32_t _pulseCount;
  volatile uint32_t _lastPulseTime; // Re-enabled declaration
  uint32_t _lastRPM; // Changed to uint32_t to prevent overflow
  SensorValue _sensorValue; // Reusable sensor value object
  
  // Variables for interval-based RPM calculation
  uint32_t _lastCalcTime;      // Time of the last RPM calculation
  uint32_t _lastCalcCount;     // Pulse count at the last RPM calculation
  
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
  uint32_t calculateRPM() {
    uint32_t profileStartTime = micros(); // Profiling start
    uint32_t currentCount;
    uint32_t tempLastPulseTime; // Keep for stop detection
    uint32_t currentTime = millis();
    
    // --- Read shared variables atomically --- 
    noInterrupts();
    currentCount = _pulseCount;
    tempLastPulseTime = _lastPulseTime;
    // Update last pulse time here *after* reading count, less ideal but safer than millis() in ISR
    // Only update if pulses actually occurred since last check to prevent timeout issue
    if (currentCount != _lastCalcCount) { 
        _lastPulseTime = currentTime; // Not perfectly accurate, but keeps ISR minimal
        tempLastPulseTime = _lastPulseTime; // Use updated time for this check
    }
    interrupts();
    // --- End atomic read ---

    // --- Stop detection based on last pulse time --- 
    if (currentTime - tempLastPulseTime > 2000) { // Keep 2s timeout for stop detection
      #if DEBUG_MODE 
        if (_lastRPM != 0) Serial.println("RPM Calc: Motor stopped (timeout)."); 
      #endif
      _lastRPM = 0;
      _lastCalcTime = currentTime;   // Reset calculation time on stop
      _lastCalcCount = currentCount; // Reset calculation count on stop
      return 0; // Return 0 immediately if stopped
    }
    
    // --- Calculate based on interval ---
    uint32_t timeDiff = currentTime - _lastCalcTime;
    uint32_t pulsesInInterval;

    // Prevent division by zero and ensure minimum interval has passed
    // Note: Sensor::process should already handle the interval, but this adds safety.
    if (timeDiff == 0) {
        return _lastRPM; // Return previous value until interval passes
    }

    // Calculate pulses during the interval, handling overflow
    if (currentCount >= _lastCalcCount) {
        pulsesInInterval = currentCount - _lastCalcCount;
    } else {
        // Handle overflow of _pulseCount
        pulsesInInterval = (0xFFFFFFFF - _lastCalcCount) + currentCount + 1;
    }

    // Calculate pulses per second
    float pulsesPerSecond = (float)pulsesInInterval / (timeDiff / 1000.0f);

    #if DEBUG_MODE
        // Log pulses per second
        Serial.printf("RPM Calc: Pulses/sec = %.2f (pulses=%u, time=%ums)\n", pulsesPerSecond, pulsesInInterval, timeDiff);
    #endif

    // Calculate RPM: (Pulses/Second * 60 Seconds/Minute) / 6 Pulses/Revolution
    _lastRPM = (uint32_t)((pulsesPerSecond * 60.0f) / 36.0f);

    // Update state for next calculation
    _lastCalcTime = currentTime;
    _lastCalcCount = currentCount;

    uint32_t profileEndTime = micros(); // Profiling end
    #if DEBUG_MODE
        Serial.printf("RPM Calc: Took %u us\n", profileEndTime - profileStartTime);
    #endif
    return _lastRPM;
  }
};

// Implementation of the static ISR handler
ICACHE_RAM_ATTR void KunrayHallRpmSensor::staticIsrHandler(void* arg) {
    // Cast the argument back to our class instance
    KunrayHallRpmSensor* instance = static_cast<KunrayHallRpmSensor*>(arg);
    // Call the non-static member function
    if (instance) { // Basic null check
        instance->incrementPulse();
    }
}

#endif // KUNRAY_HALL_RPM_SENSOR_H 