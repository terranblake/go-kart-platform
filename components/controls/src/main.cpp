#include <Arduino.h>
#include "Config.h"

#ifdef DEBUG_ENABLED
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
#endif

void setup() {
  #ifdef DEBUG_ENABLED
    Serial.begin(115200);
    DEBUG_PRINTLN("Initializing...");
  #endif
  
  // TODO: Component-specific initialization
}

void loop() {
  // TODO: Component-specific main loop
  delay(10);
}
