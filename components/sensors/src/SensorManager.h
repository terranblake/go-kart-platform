#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "Sensor.h"
#include "../variants/RpmSensor.h"

/**
 * SensorManager - Handles static access to sensors
 * Particularly useful for interrupt handlers
 */
class SensorManager {
public:
  /**
   * Initialize the manager
   */
  static void initialize() {
    // Reset all references
    for (uint8_t i = 0; i < MAX_RPM_SENSORS; i++) {
      _rpmSensors[i] = nullptr;
    }
    _rpmSensorCount = 0;
  }
  
  /**
   * Register an RPM sensor for static access
   * @param sensor Pointer to RPM sensor
   * @return true if registration was successful
   */
  static bool registerRpmSensor(RpmSensor* sensor) {
    if (_rpmSensorCount >= MAX_RPM_SENSORS) return false;
    
    _rpmSensors[_rpmSensorCount++] = sensor;
    return true;
  }
  
  /**
   * Hall sensor interrupt handler
   * Increments pulse count for all registered RPM sensors
   */
  static void ICACHE_RAM_ATTR hallSensorInterrupt() {
    for (uint8_t i = 0; i < _rpmSensorCount; i++) {
      if (_rpmSensors[i]) {
        _rpmSensors[i]->incrementPulse();
      }
    }
  }
  
private:
  // Maximum number of RPM sensors that can be registered
  static const uint8_t MAX_RPM_SENSORS = 4;
  
  // Static storage for RPM sensors
  static RpmSensor* _rpmSensors[MAX_RPM_SENSORS];
  static uint8_t _rpmSensorCount;
};

// Initialize static members
RpmSensor* SensorManager::_rpmSensors[MAX_RPM_SENSORS] = {nullptr};
uint8_t SensorManager::_rpmSensorCount = 0;

#endif // SENSOR_MANAGER_H 