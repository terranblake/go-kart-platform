#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include <Arduino.h>
#include "../../common/lib/CrossPlatformCAN/ProtobufCANInterface.h"
#include "Sensor.h"
#include "/Users/terranblake/Documents/go-kart-platform/protocol/generated/nanopb/sensors.pb.h"

/**
 * SensorRegistry - Manages a collection of sensors
 * 
 * Handles registration, periodic processing, and CAN transmission
 * for multiple sensors.
 */
class SensorRegistry {
public:
  /**
   * Constructor
   * 
   * @param canInterface CAN interface for transmission
   * @param componentType Component type from kart_common_ComponentType
   * @param componentId Component ID
   */
  SensorRegistry(
    ProtobufCANInterface& canInterface,
    kart_common_ComponentType componentType,
    uint8_t componentId
  ) : _canInterface(canInterface),
      _componentType(componentType),
      _componentId(componentId),
      _sensorCount(0) {}
  
  /**
   * Register a sensor
   * 
   * @param sensor Pointer to a sensor
   * @return true if registration successful
   */
  bool registerSensor(Sensor* sensor) {
    if (_sensorCount >= MAX_SENSORS) {
      return false;
    }
    Serial.println("Registering sensor");
    _sensors[_sensorCount++] = sensor;
    sensor->begin();
    delay(50);
    
    return true;
  }
  
  /**
   * Process all registered sensors
   * 
   * @param forceSend Force sending values even if not due
   */
  void process(bool forceSend = false) {
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i] && _sensors[i]->isEnabled()) {
        _sensors[i]->process(_canInterface, forceSend);
      }
    }
  }
  
  /**
   * Get a specific sensor by ID and type
   * 
   * @param sensorType Sensor type from SensorCommandId enum
   * @param locationId Sensor location ID
   * @return Pointer to the sensor or nullptr if not found
   */
  Sensor* getSensor(uint8_t sensorType, uint8_t locationId) {
    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i] && 
          _sensors[i]->getSensorCommandId() == sensorType && 
          _sensors[i]->getLocationId() == locationId) {
        return _sensors[i];
      }
    }
    return nullptr;
  }
  
  /**
   * Broadcast status of all registered sensors
   * 
   * @param status Status value (from SensorStatusValue enum)
   */
  void broadcastStatus(uint8_t status) {
    Serial.println("Broadcasting status");

    for (uint8_t i = 0; i < _sensorCount; i++) {
      if (_sensors[i]) {
        _canInterface.sendMessage(
          kart_common_MessageType_STATUS,
          _componentType,
          _sensors[i]->getLocationId(),
          kart_sensors_SensorCommandId_STATUS,
          kart_common_ValueType_UINT8,
          status
        );
        delay(50);
      }
    }
  }
  
private:
  static const uint8_t MAX_SENSORS = 16;
  
  ProtobufCANInterface& _canInterface;  // CAN interface reference
  kart_common_ComponentType _componentType;  // Component type
  uint8_t _componentId;  // Component ID
  Sensor* _sensors[MAX_SENSORS];  // Array of sensor pointers
  uint8_t _sensorCount;  // Number of registered sensors
};

#endif // SENSOR_REGISTRY_H 