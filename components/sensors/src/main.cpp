#include <Arduino.h>
#include "CommonConfig.h"
#include "SensorBase.h"

// Forward declaration of the function that will be implemented in each variant
SensorBase* createSensorImplementation();

// Global objects
SensorBase* sensor = nullptr;

void setup() {
  #ifdef DEBUG_ENABLED
    Serial.begin(115200);
    DEBUG_PRINTLN("Sensor initializing...");
  #endif
  
  // Create the appropriate sensor implementation based on build flags
  sensor = createSensorImplementation();
  
  if (sensor != nullptr) {
    sensor->begin();
    DEBUG_PRINTLN("Sensor initialized successfully");
  } else {
    DEBUG_PRINTLN("Failed to create sensor implementation!");
  }
}

void loop() {
  if (sensor != nullptr) {
    // Read sensor data
    sensor->update();
    
    // Periodically send sensor data
    static unsigned long lastUpdateTime = 0;
    if (millis() - lastUpdateTime > 1000) { // Send every second
      sensor->sendData();
      lastUpdateTime = millis();
    }
  }
  
  delay(10);
}
