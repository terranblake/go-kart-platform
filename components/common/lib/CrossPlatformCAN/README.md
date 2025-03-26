# CrossPlatformCAN Library

A cross-platform CAN interface library that works seamlessly on both Raspberry Pi (Linux) and Arduino platforms. The library provides a consistent API for CAN communication and supports Protocol Buffer message serialization.

## Features

- Unified API for both Raspberry Pi and Arduino platforms
- Automatic platform detection at compile time
- Protocol Buffer message serialization support
- Message handler registration for event-driven programming
- Efficient 8-byte message format supporting various data types
- Python bindings using CFFI
- Socket-based CAN communication on Linux
- Fallback to can-utils when needed

## Requirements

### For Raspberry Pi

- Linux with SocketCAN support (built into Linux kernel)
- CAN hardware interface (e.g., MCP2515 based HAT or USB-CAN adapter)
- C++11 compatible compiler
- CMake 3.10 or newer
- Protocol Buffer generated files
- can-utils package for fallback option

### For Arduino

- Arduino IDE or PlatformIO
- Arduino-CAN library
- CAN transceiver hardware (e.g., MCP2515 + TJA1050)
- SPI connection to the CAN controller

## Installation

### Raspberry Pi

1. Install dependencies:
   ```bash
   sudo apt-get update
   sudo apt-get install -y cmake build-essential can-utils
   ```

2. Set up the CAN hardware:
   ```bash
   # Configure and bring up the CAN interface
   sudo ip link set can0 type can bitrate 500000
   sudo ip link set up can0
   
   # Verify the interface is up
   ip -details link show can0
   ```

3. Build the library using the provided script:
   ```bash
   cd ~/go-kart-platform/components/common/lib/CrossPlatformCAN
   chmod +x raspi_build.sh
   ./raspi_build.sh
   ```
   
   The build script performs the following tasks:
   - Creates necessary directories (include, build)
   - Copies header files to the right locations
   - Sets compilation flags for Linux platform
   - Compiles the library as a shared library (.so)
   - Copies the shared library to the python directory
   - Exports C API symbols for CFFI integration

### Arduino

1. Install the Arduino-CAN library:
   - Open Arduino IDE
   - Go to Sketch > Include Library > Manage Libraries
   - Search for "CAN" and install the library by Sandeep Mistry

2. Copy the CrossPlatformCAN library files to your Arduino libraries folder:
   ```bash
   cp -r CrossPlatformCAN ~/Arduino/libraries/
   ```

## Usage

### C++ API

```cpp
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
  // Handle the message
  // ...
}

void setup() {
  // Initialize CAN interface (default: 500kbps)
  canInterface.begin(500000);
  
  // Register message handler
  canInterface.registerHandler(kart_common_ComponentType_LIGHTS, 
                              0x01, 
                              0x01, 
                              messageHandler);
}

void loop() {
  // Process incoming CAN messages
  canInterface.process();
  
  // Send a message
  canInterface.sendMessage(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    0x01,
    0x01,
    kart_common_ValueType_BOOLEAN,
    true
  );
}
```

### Python API

The library provides Python bindings using CFFI. The Python interface can be used as follows:

```python
from can_interface import CANInterface, MessageType, ComponentType, ValueType

# Create a CAN interface instance
can = CANInterface(node_id=0x01)

# Initialize the CAN interface
can.begin(baudrate=500000, device="can0")

# Define a message handler
def message_handler(msg_type, comp_type, comp_id, cmd_id, val_type, value):
    print(f"Received message: {msg_type}, {comp_type}, {comp_id}, {cmd_id}, {val_type}, {value}")

# Register the handler
can.register_handler(
    ComponentType.LIGHTS,
    component_id=0x01,
    command_id=0x01,
    handler=message_handler
)

# Start automatic message processing in a background thread
can.start_processing()

# Send a message
can.send_message(
    MessageType.COMMAND,
    ComponentType.LIGHTS,
    component_id=0x01,
    command_id=0x01,
    value_type=ValueType.BOOLEAN,
    value=1
)

# When done, stop processing
can.stop_processing()
```

The Python interface has a fallback mechanism to use can-utils (cansend/candump) if the CFFI library cannot be loaded or initialized.

## Message Format

The library uses a consistent 8-byte message format:

| Byte | Description |
|------|-------------|
| 0    | Header byte: Message type (2 bits) + Component type (3 bits) + Reserved (3 bits) |
| 1    | Reserved for future use |
| 2    | Component ID |
| 3    | Command ID |
| 4    | Value type (4 bits) + Reserved (4 bits) |
| 5-7  | Value data (up to 24 bits) |

### Message Types

- COMMAND (0) - Instructions to be executed
- STATUS (1) - Status updates
- ACK (2) - Acknowledgments
- ERROR (3) - Error reports

### Component Types

- LIGHTS (0)
- MOTORS (1)
- SENSORS (2)
- BATTERY (3)
- CONTROLS (4)

### Value Types

- BOOLEAN (0)
- INT8 (1)
- UINT8 (2)
- INT16 (3)
- UINT16 (4)
- INT24 (5)
- UINT24 (6)
- FLOAT16 (7)

## Binary Data Transfer

The library provides support for sending larger binary data payloads that exceed the 8-byte limit of a single CAN frame. This is particularly useful for applications like sending animation frames to LED controllers or firmware updates.

### Binary Data Message Format

Binary data is split across multiple CAN frames with the following structure:

#### First Frame (Start Frame)
| Byte | Description |
|------|-------------|
| 0    | Header byte: Message type (2 bits) + Component type (3 bits) + Reserved (3 bits) |
| 1    | Flags/Sequence: Start flag (bit 7) + Sequence number (bits 0-3) |
| 2    | Component ID |
| 3    | Command ID |
| 4    | Value type (4 bits) + Frame count or reserved (4 bits) |
| 5-7  | Up to 3 bytes of data |

#### Subsequent Frames
| Byte | Description |
|------|-------------|
| 0    | Header byte: Message type (2 bits) + Component type (3 bits) + Reserved (3 bits) |
| 1    | Flags/Sequence: End flag (bit 6) + Sequence number (bits 0-3) |
| 2    | Component ID |
| 3    | Command ID |
| 4    | Bytes remaining in this frame |
| 5-7  | Up to 3 bytes of data |

### Using Binary Data Transfer

#### C++ Example

```cpp
// Create binary data to send (e.g., animation frame data)
uint8_t frameData[32] = { /* ... data ... */ };

// Send the binary data
canInterface.sendBinaryData(
  kart_common_MessageType_COMMAND,
  kart_common_ComponentType_LIGHTS,
  0x01, // Component ID
  kart_lights_LightCommandId_ANIMATION_FRAME, // Command ID
  kart_common_ValueType_UINT8, // Value type
  frameData,
  sizeof(frameData)
);

// Register a handler for binary data
canInterface.registerBinaryHandler(
  kart_common_MessageType_COMMAND,
  kart_common_ComponentType_LIGHTS,
  0x01, // Component ID
  kart_lights_LightCommandId_ANIMATION_FRAME, // Command ID,
  [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type,
     uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type,
     const void* data, size_t data_size) {
    // Process the received binary data
    const uint8_t* frameData = static_cast<const uint8_t*>(data);
    // ...
  }
);
```

#### Python Example

```python
# Create binary data to send
frame_data = bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10])

# Send the binary data
can.send_binary_data(
    component_type_name="LIGHTS",
    component_name="FRONT",
    command_name="ANIMATION_FRAME",
    value_type="UINT8",
    data=frame_data
)

# Register a handler for binary data
def binary_handler(msg_type, comp_type, comp_id, cmd_id, value_type, data, data_size):
    # Process the binary data
    print(f"Received {data_size} bytes of binary data")
    
can.register_binary_handler(
    ComponentType.LIGHTS,
    component_id=0x01,
    command_id=LightsCommand.ANIMATION_FRAME,
    handler=binary_handler
)
```

### Implementation Details

The binary data transfer is implemented by:

1. Breaking the data into chunks that fit into CAN frames
2. Adding metadata to track the sequence and identify start/end frames
3. Reassembling the data on the receiving end
4. Calling the appropriate binary data handler when complete

The maximum supported binary data size is defined by `MAX_BINARY_SIZE` (default: 1024 bytes).

## Build Process Details

### raspi_build.sh

This script is used to build the library for Raspberry Pi:

1. Sets up include directories for header files
2. Defines preprocessing flags for Linux platform
3. Compiles the library as a shared object (.so)
4. Copies the compiled library to the Python directory
5. Uses `nm` to verify exported symbols for C API

### deploy_to_pi.sh

This script deploys the library to a Raspberry Pi:

1. Creates the directory structure on the Pi
2. Copies all necessary files (source files, headers, and protocol files)
3. Makes scripts executable
4. Provides instructions for building and testing

### C API

The library exposes a C API through `c_api.cpp` for use with other languages:

- `can_interface_create()` - Creates a CAN interface instance
- `can_interface_destroy()` - Destroys the instance
- `can_interface_begin()` - Initializes the CAN interface
- `can_interface_register_handler()` - Registers a message handler
- `can_interface_send_message()` - Sends a CAN message
- `can_interface_process()` - Processes incoming messages

## Troubleshooting

### Raspberry Pi

- Check if the CAN interface is up: `ip -details link show can0`
- Monitor CAN traffic: `candump can0`
- Send a test frame: `cansend can0 123#DEADBEEF`
- Use socket_direct_test.py to send test messages directly via socket API
- If you get compilation errors about missing headers:
  - Ensure all Protocol Buffer headers are in the include directory
  - Make sure nanopb headers are available in include/nanopb

### CAN Error States

If `ip -details link show can0` shows an error state:

- ERROR-ACTIVE: Normal state, can send and receive
- ERROR-WARNING: Error counters are high but still operational
- ERROR-PASSIVE: Cannot send active error frames, only passive ones
- BUS-OFF: Cannot send or receive, needs reset

## License

This library is released under the MIT License. 