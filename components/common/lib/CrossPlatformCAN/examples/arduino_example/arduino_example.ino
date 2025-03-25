/**
 * Arduino example for CrossPlatformCAN library
 * 
 * This example shows how to use the ProtobufCANInterface on Arduino.
 * Hardware requirements:
 * - Arduino (Nano or other)
 * - MCP2515 CAN controller connected via SPI
 * 
 * Connections:
 * - CS -> 10
 * - INT -> 2
 * - SCK -> 13
 * - MOSI -> 11
 * - MISO -> 12
 * - VCC -> 5V
 * - GND -> GND
 */

#define ARDUINO

#include "ProtobufCANInterface.h"

// Define a node ID for this device
#define NODE_ID 0x01

// Create a CAN interface instance
ProtobufCANInterface canInterface(NODE_ID);

// Handler for received messages
void messageHandler(kart_common_MessageType msg_type, 
                   kart_common_ComponentType comp_type,
                   uint8_t component_id, uint8_t command_id, 
                   kart_common_ValueType value_type, int32_t value) {
  // Print received message
  Serial.print("Received message: ");
  Serial.print("Type=");
  Serial.print(msg_type);
  Serial.print(", Component=");
  Serial.print(comp_type);
  Serial.print(", ID=");
  Serial.print(component_id);
  Serial.print(", Command=");
  Serial.print(command_id);
  Serial.print(", Value=");
  Serial.println(value);
  
  // Example: Toggle LED when receiving a specific command
  if (comp_type == kart_common_ComponentType_LIGHTS && 
      command_id == 1 && 
      value_type == kart_common_ValueType_BOOLEAN) {
    digitalWrite(LED_BUILTIN, value ? HIGH : LOW);
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("Arduino CAN Interface Example");
  
  // Initialize GPIO
  pinMode(LED_BUILTIN, OUTPUT);
  
  // Initialize CAN interface (default: 500kbps)
  if (!canInterface.begin(500000)) {
    Serial.println("Failed to initialize CAN interface!");
    while (1); // Stop if we couldn't initialize
  }
  
  // Register message handler
  canInterface.registerHandler(
    kart_common_MessageType_COMMAND,       // Message type (COMMAND only)
    kart_common_ComponentType_LIGHTS,      // Component type
    0x01,                                  // Component ID
    0x01,                                  // Command ID
    messageHandler                         // Handler function
  );
  
  Serial.println("CAN Interface initialized successfully!");
}

void loop() {
  // Process incoming CAN messages
  canInterface.process();
  
  // Example: Send a message every second
  static unsigned long lastSendTime = 0;
  if (millis() - lastSendTime > 1000) {
    lastSendTime = millis();
    
    // Toggle state
    static bool ledState = false;
    ledState = !ledState;
    
    // Send the toggle command
    canInterface.sendMessage(
      kart_common_MessageType_COMMAND,     // Message type: Command
      kart_common_ComponentType_LIGHTS,    // Component type: Lights
      0x01,                               // Component ID: 1
      0x01,                               // Command ID: 1 (Toggle)
      kart_common_ValueType_BOOLEAN,      // Value type: Boolean
      ledState                            // Value: true/false
    );
    
    Serial.print("Sent toggle command with value: ");
    Serial.println(ledState);
  }
} 