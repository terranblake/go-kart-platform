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
  
  /**
   * Send readings from all sensors immediately
   * 
   * Forces all sensors to take a reading and send it regardless of their update intervals
   */
  void sendAllReadings() {
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i] && _sensors[i]->isEnabled()) {
        _sensors[i]->process(_canInterface, true);
      }
    }
  }
  
  /**
   * Handle incoming CAN messages related to sensors
   * 
   * @param messageType Message type (command/status)
   * @param componentType Component type (should be SENSORS)
   * @param locationId Sensor location ID
   * @param commandId Sensor command ID (sensor type)
   * @param valueType Type of the value
   * @param value The value received
   */
  void handleMessage(
      kart_common_MessageType messageType,
      kart_common_ComponentType componentType,
      uint8_t locationId,
      uint8_t commandId,
      kart_common_ValueType valueType,
      int32_t value) {
    
    // Only process commands to the SENSORS component
    if (componentType != kart_common_ComponentType_SENSORS) {
      return;
    }
    
    // Get the target sensor
    Sensor* sensor = getSensor(commandId, locationId);
    if (!sensor) {
      // Sensor not found
      return;
    }
    
    // Handle different message types
    if (messageType == kart_common_MessageType_COMMAND) {
      // Process command (currently we just support enable/disable)
      // We use commandId 9 (STATUS) with value 0 (ENABLE) or 1 (DISABLE)
      if (commandId == 9) {  // STATUS command
        if (value == 0) {    // ENABLE
          sensor->setEnabled(true);
        } else if (value == 1) { // DISABLE
          sensor->setEnabled(false);
        }
      }
    }
  }
  
private:
  static const uint8_t MAX_SENSORS = 16;
  
  ProtobufCANInterface& _canInterface;
  Sensor* _sensors[MAX_SENSORS] = {nullptr};
  uint8_t _sensorCount;
};

#endif // SENSOR_MANAGER_H 