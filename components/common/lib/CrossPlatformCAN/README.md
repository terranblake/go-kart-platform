<!-- LLM_CONTEXT
library: CrossPlatformCAN
purpose: Cross-platform CAN bus communication library for Arduino and Raspberry Pi
languages: C++, Python
message_format: 8-byte format with message type, component type, component ID, command ID, value type, and value
platforms: Arduino (with CAN library), Raspberry Pi (with SocketCAN)
key_apis: send_message, register_handler, process
-->

# CrossPlatformCAN Library

## Overview
Cross-platform CAN interface library for Raspberry Pi and Arduino with Protocol Buffer message serialization.

## Platform Support
<!-- LLM_CODE_MARKER: platform_support -->
| Platform | Communication Method | Dependencies | Hardware |
|----------|---------------------|--------------|----------|
| Raspberry Pi | SocketCAN (Linux kernel) | C++11, CMake 3.10+, can-utils | MCP2515 HAT or USB-CAN adapter |
| Arduino | Arduino-CAN library | Arduino IDE/PlatformIO | MCP2515 + TJA1050, SPI connection |
<!-- LLM_CODE_MARKER_END -->

## Message Format
<!-- LLM_CODE_MARKER: message_format -->
| Byte | Content | Description |
|------|---------|-------------|
| 0 | Header | Message type (2 bits) + Component type (3 bits) + Reserved (3 bits) |
| 1 | Reserved | Reserved for future use |
| 2 | Component ID | Identifies specific component instance |
| 3 | Command ID | Specific command or status identifier |
| 4 | Value type | Value type (4 bits) + Reserved (4 bits) |
| 5-7 | Value | Value data (up to 24 bits) |
<!-- LLM_CODE_MARKER_END -->

## Enumeration Values
<!-- LLM_CODE_MARKER: enumeration_values -->
### Message Types
- COMMAND (0): Instructions to execute
- STATUS (1): Status updates
- ACK (2): Acknowledgments
- ERROR (3): Error reports

### Component Types
- LIGHTS (0): Lighting systems
- MOTORS (1): Motor control
- SENSORS (2): Sensor readings
- BATTERY (3): Battery management
- CONTROLS (4): User input controls

### Value Types
- BOOLEAN (0): True/false values
- INT8 (1): 8-bit signed integer
- UINT8 (2): 8-bit unsigned integer
- INT16 (3): 16-bit signed integer
- UINT16 (4): 16-bit unsigned integer
- INT24 (5): 24-bit signed integer
- UINT24 (6): 24-bit unsigned integer
- FLOAT16 (7): 16-bit floating point
<!-- LLM_CODE_MARKER_END -->

## API Reference
<!-- LLM_API_BOUNDARY: C++ API -->
```cpp
// Create an interface with node ID
ProtobufCANInterface canInterface(uint8_t node_id);

// Initialize with baudrate
void begin(uint32_t baudrate = 500000);

// Register message handler
void registerHandler(
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(kart_common_MessageType, kart_common_ComponentType, 
                   uint8_t, uint8_t, kart_common_ValueType, int32_t)
);

// Send message
bool sendMessage(
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id,
    kart_common_ValueType value_type,
    int32_t value
);

// Process incoming messages (call in loop)
void process();
```
<!-- LLM_API_END -->

<!-- LLM_API_BOUNDARY: Python API -->
```python
# Create an interface with node ID
can = CANInterface(node_id=0x01)

# Initialize with baudrate and device
can.begin(baudrate=500000, device="can0")

# Register message handler
can.register_handler(
    ComponentType.LIGHTS,  # Component type
    component_id=0x01,     # Component ID
    command_id=0x01,       # Command ID
    handler=message_handler # Callback function
)

# Send message
can.send_message(
    MessageType.COMMAND,   # Message type
    ComponentType.LIGHTS,  # Component type
    component_id=0x01,     # Component ID
    command_id=0x01,       # Command ID
    value_type=ValueType.BOOLEAN, # Value type
    value=1                # Value
)

# Start/stop background processing
can.start_processing()
can.stop_processing()
```
<!-- LLM_API_END -->

## Usage Examples
<!-- LLM_CODE_MARKER: usage_examples -->
### Arduino Usage
```cpp
#include "ProtobufCANInterface.h"

// Create interface with node ID 0x01
ProtobufCANInterface canInterface(0x01);

// Message handler function
void handleLightCommand(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value) {
  // Handle light command
  if (command_id == 0x01) {  // MODE command
    // Set light mode based on value
  }
}

void setup() {
  // Initialize CAN interface
  canInterface.begin(500000);
  
  // Register handler for light commands
  canInterface.registerHandler(kart_common_ComponentType_LIGHTS,
                              0x01,  // Front lights
                              0x01,  // MODE command
                              handleLightCommand);
}

void loop() {
  // Process incoming messages
  canInterface.process();
}
```

### Python Usage
```python
from can_interface import CANInterface, MessageType, ComponentType, ValueType

# Create interface with node ID 0x01
can = CANInterface(node_id=0x01)

# Initialize
can.begin(baudrate=500000, device="can0")

# Message handler
def handle_light_status(msg_type, comp_type, comp_id, cmd_id, val_type, value):
    # Process light status update
    print(f"Light status: {value}")

# Register handler
can.register_handler(
    ComponentType.LIGHTS,
    component_id=0x01,
    command_id=0x01,
    handler=handle_light_status
)

# Start processing in background
can.start_processing()

# Send command
can.send_message(
    MessageType.COMMAND,
    ComponentType.LIGHTS,
    component_id=0x01,
    command_id=0x01,
    value_type=ValueType.BOOLEAN,
    value=1
)
```
<!-- LLM_CODE_MARKER_END -->

## Setup Instructions
<!-- LLM_CODE_MARKER: setup_instructions -->
### Raspberry Pi Setup
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y cmake build-essential can-utils

# Configure CAN interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Build library
cd components/common/lib/CrossPlatformCAN
chmod +x raspi_build.sh
./raspi_build.sh
```

### Arduino Setup
1. Install Arduino-CAN library from Library Manager
2. Copy CrossPlatformCAN to Arduino libraries folder
3. Include in your sketch with `#include "ProtobufCANInterface.h"`
<!-- LLM_CODE_MARKER_END -->

## Troubleshooting
<!-- LLM_CODE_MARKER: troubleshooting -->
### Raspberry Pi
- Check if CAN interface is up: `ip -details link show can0`
- Monitor CAN traffic: `candump can0`
- Send test message: `cansend can0 123#DEADBEEF`
- Verify library is loaded: `ldd python_binding.so`

### Arduino
- Verify SPI connections to CAN controller
- Check baudrate matches between all devices
- Monitor serial output for debugging messages
- Test with loopback configuration
<!-- LLM_CODE_MARKER_END -->

## License

This library is released under the MIT License. 