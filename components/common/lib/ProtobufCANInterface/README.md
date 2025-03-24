# ProtobufCANInterface

A cross-platform interface for Protocol Buffer messaging over CAN bus, supporting both Arduino and desktop environments.

[![Build ProtobufCANInterface](https://github.com/USER/REPO/actions/workflows/build-can-interface.yml/badge.svg)](https://github.com/USER/REPO/actions/workflows/build-can-interface.yml)

## Features

- Support for both Arduino and non-Arduino (desktop) environments
- Implements Protocol Buffer message encoding/decoding
- Message type and component type handling
- Event-based message handling with callbacks
- Debugging support with detailed logging

## Usage

### Arduino Environment

```cpp
#include <ProtobufCANInterface.h>

// Create an interface with node ID 1
ProtobufCANInterface can(1);

void setup() {
  Serial.begin(115200);
  
  // Initialize with the default baud rate (500000)
  if (!can.begin()) {
    Serial.println("Failed to initialize CAN bus!");
    while (1);
  }
  
  // Register a handler for specific message types
  can.registerHandler(
    kart_common_ComponentType_LIGHTS,           // Component type
    kart_lights_LightComponentId_FRONT,         // Component ID (or kart_lights_LightComponentId_ALL for all)
    kart_lights_LightCommandId_MODE,            // Command ID
    handleLightCommand                          // Callback function
  );
}

void loop() {
  // Process incoming messages
  can.process();
  
  // Send a message to turn on front lights
  can.sendMessage(
    kart_common_MessageType_COMMAND,            // Message type
    kart_common_ComponentType_LIGHTS,           // Component type
    kart_lights_LightComponentId_FRONT,         // Component ID
    kart_lights_LightCommandId_MODE,            // Command ID
    kart_common_ValueType_UINT8,                // Value type
    kart_lights_LightModeValue_ON               // Value (ON)
  );
  
  delay(1000);
}

// Handler callback
void handleLightCommand(
  kart_common_MessageType message_type,
  kart_common_ComponentType component_type,
  uint8_t component_id,
  uint8_t command_id,
  kart_common_ValueType value_type,
  int32_t value
) {
  Serial.print("Received light command: ");
  
  // Check the specific command and value
  if (command_id == kart_lights_LightCommandId_MODE) {
    switch (value) {
      case kart_lights_LightModeValue_ON:
        Serial.println("ON");
        break;
      case kart_lights_LightModeValue_OFF:
        Serial.println("OFF");
        break;
      case kart_lights_LightModeValue_BRIGHT:
        Serial.println("BRIGHT");
        break;
      default:
        Serial.println(value);
    }
  }
  
  // Process the command
  // ...
}
```

### Non-Arduino Environment

#### Prerequisites

Before compiling, run the `prepare_includes.sh` script to copy the necessary header files:

```bash
# From the project root
./components/common/lib/ProtobufCANInterface/prepare_includes.sh
```

#### Compilation

You can compile directly with g++:

```bash
cd components/common/lib/ProtobufCANInterface
g++ -std=c++11 -I./include -I./src your_program.cpp src/ProtobufCANInterface.cpp -o your_program
```

#### Example Code

```cpp
#include "ProtobufCANInterface.h"
#include <iostream>

void handleMessage(
  kart_common_MessageType message_type,
  kart_common_ComponentType component_type,
  uint8_t component_id,
  uint8_t command_id,
  kart_common_ValueType value_type,
  int32_t value
) {
  std::cout << "Received message: ";
  
  // Check which component and command it is
  if (component_type == kart_common_ComponentType_LIGHTS) {
    std::cout << "LIGHTS, ";
    
    if (component_id == kart_lights_LightComponentId_FRONT) {
      std::cout << "FRONT, ";
    } else if (component_id == kart_lights_LightComponentId_REAR) {
      std::cout << "REAR, ";
    }
    
    if (command_id == kart_lights_LightCommandId_MODE) {
      std::cout << "MODE = ";
      
      switch (value) {
        case kart_lights_LightModeValue_ON: std::cout << "ON"; break;
        case kart_lights_LightModeValue_OFF: std::cout << "OFF"; break;
        default: std::cout << value; break;
      }
    }
  } else if (component_type == kart_common_ComponentType_CONTROLS) {
    std::cout << "CONTROLS, ";
    
    if (component_id == kart_controls_ControlComponentId_THROTTLE) {
      std::cout << "THROTTLE, ";
    }
    
    if (command_id == kart_controls_ControlCommandId_MODE) {
      std::cout << "MODE = ";
      
      switch (value) {
        case kart_controls_ControlModeValue_MANUAL: std::cout << "MANUAL"; break;
        case kart_controls_ControlModeValue_SPORT: std::cout << "SPORT"; break;
        default: std::cout << value; break;
      }
    }
  }
  
  std::cout << std::endl;
}

int main() {
  // Create an interface with node ID 1
  ProtobufCANInterface can(1);
  
  // Initialize with the default baud rate
  can.begin();
  
  // Register a handler for light commands
  can.registerHandler(
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_FRONT,
    kart_lights_LightCommandId_MODE,
    handleMessage
  );
  
  // Send a message to turn on front lights
  can.sendMessage(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_FRONT,
    kart_lights_LightCommandId_MODE,
    kart_common_ValueType_UINT8,
    kart_lights_LightModeValue_ON
  );
  
  // Process any incoming messages
  can.process();
  
  return 0;
}
```

## Building with CMake

The repository includes a CMakeLists.txt for building with CMake:

```bash
cd components/common/lib/ProtobufCANInterface
mkdir build && cd build
cmake ..
make
```

## API Reference

### Constructor

```cpp
ProtobufCANInterface(uint32_t nodeId)
```

Creates a new CAN interface with the specified node ID.

### Methods

```cpp
bool begin(long baudrate = 500000)
```

Initializes the CAN interface with the specified baud rate. Returns true if successful.

```cpp
void registerHandler(
  kart_common_ComponentType type,
  uint8_t component_id,
  uint8_t command_id,
  MessageHandler handler
)
```

Registers a callback function to handle specific message types.

```cpp
bool sendMessage(
  kart_common_MessageType message_type,
  kart_common_ComponentType component_type,
  uint8_t component_id,
  uint8_t command_id,
  kart_common_ValueType value_type,
  int32_t value
)
```

Sends a message over the CAN bus. Returns true if successful.

```cpp
void process()
```

Processes incoming CAN messages and calls registered handlers.

## Continuous Integration

This package is automatically tested with GitHub Actions on every commit that changes files in the ProtobufCANInterface directory or the Protocol Buffer files. 