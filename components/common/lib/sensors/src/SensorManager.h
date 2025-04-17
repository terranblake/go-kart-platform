#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "../../common/lib/CrossPlatformCAN/ProtobufCANInterface.h"
#include "Sensor.h"

/**
 * SensorManager - Central manager for all sensors on a node
 * 
 * Handles sensor initialization, periodic sampling, and CAN communication
 * for all sensors on a particular node
 */
class SensorManager {
public:
  /**
   * Constructor
   * 
   * @param canInterface Reference to the CAN interface
   */
  SensorManager(ProtobufCANInterface& canInterface) 
    : _canInterface(canInterface), _sensorCount(0) {}
  
  /**
   * Add a sensor to the manager
   * 
   * @param sensor Pointer to a sensor object
   * @return true if the sensor was added successfully
   */
  bool addSensor(Sensor* sensor) {
    if (_sensorCount >= MAX_SENSORS) {
      return false;
    }
    
    _sensors[_sensorCount++] = sensor;
    return true;
  }
  
  /**
   * Initialize all sensors
   * 
   * @return true if all sensors were initialized successfully
   */
  bool begin() {
    bool success = true;
    
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i]) {
        success &= _sensors[i]->begin();
      }
    }
    
    return success;
  }
  
  /**
   * Process all sensors
   * 
   * Should be called regularly in the main loop to sample and transmit
   * sensor data according to each sensor's update interval
   */
  void process() {
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i]) {
        _sensors[i]->process(_canInterface);
      }
    }
  }
  
  /**
   * Get a sensor by command ID (sensor type) and location
   * 
   * @param commandId The sensor type (from SensorCommandId enum)
   * @param locationId The sensor location (specific to each sensor type)
   * @return Pointer to the sensor or nullptr if not found
   */
  Sensor* getSensor(uint8_t commandId, uint8_t locationId) {
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i] && 
          _sensors[i]->getSensorCommandId() == commandId && 
          _sensors[i]->getLocationId() == locationId) {
        return _sensors[i];
      }
    }
    
    return nullptr;
  }
  
private:
  static const uint8_t MAX_SENSORS = 16;
  
  ProtobufCANInterface& _canInterface;
  Sensor* _sensors[MAX_SENSORS] = {nullptr};
  uint8_t _sensorCount;
};

#endif // SENSOR_MANAGER_H 