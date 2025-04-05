#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include <Arduino.h>
#include "ProtobufCANInterface.h"
#include "Sensor.h"

/**
 * SensorRegistry - Manages a collection of sensors for a component
 * Handles initialization, processing, and CAN transmission
 */
class SensorRegistry {
public:
  /**
   * Constructor
   * @param can ProtobufCANInterface reference
   * @param componentType Component type from kart_common_ComponentType
   * @param componentId Component ID
   */
  SensorRegistry(ProtobufCANInterface& can, uint8_t componentType, uint8_t componentId) : 
    _can(can), _componentType(componentType), _componentId(componentId) {}
  
  /**
   * Register a sensor with the registry
   * @param sensor Pointer to sensor object
   * @return true if registration was successful
   */
  bool registerSensor(Sensor* sensor) {
    if (_sensorCount >= MAX_SENSORS) return false;
    
    _sensors[_sensorCount++] = sensor;
    sensor->begin();
    return true;
  }
  
  /**
   * Process all sensors
   * Reads and transmits sensor data based on update intervals
   */
  void process() {
    uint32_t currentTime = millis();
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i]) {
        _sensors[i]->process(_can, _componentType, _componentId, currentTime);
      }
    }
  }
  
  /**
   * Get a sensor by ID
   * @param id Sensor ID
   * @return Pointer to sensor or nullptr if not found
   */
  Sensor* getSensor(uint8_t id) {
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i] && _sensors[i]->getId() == id) {
        return _sensors[i];
      }
    }
    return nullptr;
  }
  
  /**
   * Get number of registered sensors
   * @return Sensor count
   */
  uint8_t getSensorCount() const {
    return _sensorCount;
  }
  
private:
  static const uint8_t MAX_SENSORS = 16;
  Sensor* _sensors[MAX_SENSORS] = {nullptr};
  uint8_t _sensorCount = 0;
  ProtobufCANInterface& _can;
  uint8_t _componentType;
  uint8_t _componentId;
};

#endif // SENSOR_REGISTRY_H 