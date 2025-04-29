#ifndef DIGITAL_INPUT_SENSOR_H
#define DIGITAL_INPUT_SENSOR_H

#include <Arduino.h>
#include "../src/Sensor.h"

/**
 * DigitalInputSensor - Reads a digital input pin and reports its state.
 *
 * Reports state as UINT8 (typically 0 for LOW, 1 for HIGH).
 * Can be used for simple switches, buttons, or other binary sensors.
 */
class DigitalInputSensor : public Sensor {
public:
  /**
   * Constructor
   * @param componentType Component type (e.g., CONTROLS)
   * @param componentId Component ID (e.g., HAZARD_SWITCH)
   * @param commandId Command ID (e.g., STATE)
   * @param pin The digital GPIO pin to read.
   * @param updateInterval Update interval in ms.
   * @param inputMode Arduino pinMode (e.g., INPUT, INPUT_PULLUP, INPUT_PULLDOWN).
   * @param activeLow If true, LOW is considered the 'active' or 'on' state (reports 1), HIGH reports 0.
   */
  DigitalInputSensor(
    kart_common_ComponentType componentType,
    uint8_t componentId,
    uint8_t commandId,
    uint8_t pin,
    uint16_t updateInterval = 200, // Slower updates often OK for switches
    uint8_t inputMode = INPUT_PULLUP, // Default to pullup for simple switches
    bool activeLow = false
  ) : Sensor(componentType, componentId, commandId, kart_common_ValueType_UINT8, updateInterval),
      _pin(pin),
      _inputMode(inputMode),
      _activeLow(activeLow),
      _lastReportedState(-1) // Initialize to invalid state
  {}

  /**
   * Initialize the sensor pin.
   */
  bool begin() override {
    pinMode(_pin, _inputMode);
    // Read initial state to avoid sending update immediately if unchanged
    int initialState = digitalRead(_pin);
    _lastReportedState = _activeLow ? !initialState : initialState;
    _baseSensorValue.uint8_value = _lastReportedState;
    return true;
  }

  /**
   * Read the digital pin state.
   * @return SensorValue containing the state (0 or 1).
   */
  SensorValue read() override {
    int currentState = digitalRead(_pin);
    // Map to 0 or 1 based on activeLow setting
    uint8_t mappedState = _activeLow ? !currentState : currentState;
    _baseSensorValue.uint8_value = mappedState;
    return _baseSensorValue;
  }

  /**
   * Override process to only send updates on state change.
   */
  bool process(ProtobufCANInterface& canInterface, bool forceUpdate = false) override {
    uint32_t currentTime = millis();
    bool shouldSend = false;

    if (forceUpdate || (currentTime - _lastUpdateTime >= _updateInterval)) {
      _lastUpdateTime = currentTime;
      SensorValue currentVal = read();
      uint8_t currentState = currentVal.uint8_value;

      // Check if state has changed since last report
      if (forceUpdate || currentState != _lastReportedState) {
          #if DEBUG_MODE 
            // Optional: Log state change
            // Serial.printf("Digital Sensor %d:%d changed to %d\n", _componentId, _commandId, currentState);
          #endif
          shouldSend = true;
          _lastReportedState = currentState; // Update last reported state
      }
    }

    if (shouldSend) {
      // Send the message using the base class value (_baseSensorValue)
      return canInterface.sendMessage(
        kart_common_MessageType_STATUS, 
        (kart_common_ComponentType)_componentType,
        _componentId, 
        _commandId, 
        _valueType, 
        getValue() // getValue() uses _baseSensorValue which was updated in read()
      );
    } else {
      return false; // No update sent
    }
  }

private:
  uint8_t _pin;
  uint8_t _inputMode;
  bool _activeLow;
  int8_t _lastReportedState; // Store last state to send only on change
};

#endif // DIGITAL_INPUT_SENSOR_H 