# Go-Kart CAN Protocol Documentation

This document describes the CAN (Controller Area Network) protocol used in the Go-Kart platform, including message formats, component types, commands, and usage examples.

## Table of Contents

1. [Overview](#overview)
2. [Message Format](#message-format)
3. [Component Types](#component-types)
4. [Command Structure](#command-structure)
5. [Value Types and Encoding](#value-types-and-encoding)
6. [Acknowledgment System](#acknowledgment-system)
7. [Python Implementation](#python-implementation)
8. [API Endpoints](#api-endpoints)
9. [Examples](#examples)

## Overview

The Go-Kart platform uses a CAN bus network to communicate between various components such as lights, motors, controls, sensors, and more. The protocol is designed to be compact and efficient for use on resource-constrained devices like Arduino microcontrollers.

The protocol is defined using Protocol Buffers (.proto files) and implemented in both C++ (for Arduino components) and Python (for the dashboard server).

## Message Format

Each CAN message consists of 8 bytes structured as follows:

```
Byte 0: [2 bits MessageType][3 bits ComponentType][3 bits reserved]
Byte 1: Reserved for future use
Byte 2: Component ID (8 bits)
Byte 3: Command/Value ID (8 bits)
Byte 4: [4 bits ValueType][4 bits reserved]
Bytes 5-7: Value (24 bits)
```

This compact format allows for efficient communication while still providing a flexible command structure.

### Message Types (2 bits)

- `COMMAND (0)`: Initiates an action or requests data
- `STATUS (1)`: Reports status/data or acknowledges a command
- `ACK (2)`: Explicit acknowledgment of a command
- `ERROR (3)`: Reports an error condition

## Component Types

The system is organized into logical component types, each representing a different subsystem of the go-kart:

- `LIGHTS (0)`: All lighting systems
- `MOTORS (1)`: Motor control systems
- `SENSORS (2)`: Various sensors throughout the kart
- `BATTERY (3)`: Battery management systems
- `CONTROLS (4)`: User input and control systems

Each component type can have multiple specific components identified by their Component ID.

### Light Components

- `FRONT (0)`: Front lights
- `REAR (1)`: Rear lights
- `INTERIOR (4)`: Interior lighting
- `AUXILIARY (8)`: Auxiliary lights
- `UNDERGLOW (9)`: Underglow lighting effects
- `ALL (255)`: All lights (broadcast)

### Control Components

- `THROTTLE (0)`: Throttle control
- `BRAKE (1)`: Brake control
- `STEERING (2)`: Steering control
- `TRANSMISSION (3)`: Transmission control
- `SUSPENSION (4)`: Suspension control
- `COOLING (5)`: Cooling system control
- `USER_INTERFACE (6)`: User interface controls
- `SECURITY (7)`: Security systems
- `DIAGNOSTIC (8)`: Diagnostic systems
- `AUTONOMOUS (9)`: Autonomous driving systems
- `ALL (10)`: All control systems (broadcast)

## Command Structure

Each component type has a set of commands it can respond to. Here are some key commands:

### Light Commands

- `MODE (0)`: Set light mode (off, on, auto, etc.)
- `SIGNAL (1)`: Control turn signals
- `INTENSITY (2)`: Set light intensity
- `PATTERN (3)`: Set light pattern
- `COLOR (4)`: Set light color
- `BRAKE (8)`: Control brake lights

### Control Commands

- `ENABLE (0)`: Enable a control system
- `DISABLE (1)`: Disable a control system
- `RESET (2)`: Reset a control system
- `MODE (3)`: Set control mode
- `PARAMETER (4)`: Set a control parameter
- `EMERGENCY (7)`: Emergency commands

## Value Types and Encoding

Values are encoded according to their type to maximize the use of the 24-bit value field:

- `BOOLEAN (0)`: Simple true/false value
- `INT8 (1)`: 8-bit signed integer
- `UINT8 (2)`: 8-bit unsigned integer
- `INT16 (3)`: 16-bit signed integer
- `UINT16 (4)`: 16-bit unsigned integer
- `INT24 (5)`: 24-bit signed integer
- `UINT24 (6)`: 24-bit unsigned integer
- `FLOAT16 (7)`: 16-bit floating point

The `packValue` and `unpackValue` functions in both the C++ and Python implementations handle the encoding and decoding of these values.

## Acknowledgment System

The protocol includes an acknowledgment system to ensure reliable communication:

1. A device sends a `COMMAND` message
2. The recipient processes the command
3. The recipient sends a `STATUS` message with the same component type, component ID, command ID, and value
4. For critical commands, an explicit `ACK` message may be requested

This system ensures that commands are received and processed correctly.

## Python Implementation

The Python implementation in the dashboard server uses Cython bindings to the C++ `ProtobufCANInterface` to ensure consistency between platforms. The implementation includes:

- Loading protocol definitions from Protocol Buffer files
- CAN message sending and receiving
- Message packing and unpacking
- State tracking for telemetry

### Directory Structure

```
dashboard/server/lib/can/
├── __init__.py            # Package initialization
├── _can_interface.pyx     # Cython bindings to C++ implementation
├── interface.py           # Python interface for CAN communication
└── protocol.py            # Protocol definitions and utilities
```

### Build System

The Python implementation uses Cython to generate bindings to the C++ implementation. To build:

```bash
cd dashboard/server
pip install -e .
```

This will compile the Cython extension and install the package in development mode.

## API Endpoints

The server provides several API endpoints to explore and use the protocol:

- `GET /api/protocol`: Get the complete protocol structure
- `GET /api/protocol/<component_type>`: Get information about a specific component type
- `GET /api/protocol/<component_type>/<component_name>`: Get information about a specific component
- `GET /api/protocol/<component_type>/<component_name>/<command_name>`: Get information about a specific command

These endpoints can be used to explore the protocol structure and available commands.

## Examples

### Sending a Command

To send a light mode command from Python:

```python
from lib.can import CANCommandGenerator

# Create command generator
can_gen = CANCommandGenerator()

# Send light mode command
can_gen.send_message('lights', 'front', 'mode', 'ON')
```

### Handling a Command in C++

```cpp
#include "ProtobufCANInterface.h"

// Create CAN interface
ProtobufCANInterface canInterface(0x01);

// Register handler for light mode command
canInterface.registerHandler(
    kart_common_ComponentType_LIGHTS,
    0, // FRONT
    kart_lights_LightCommandId_MODE,
    [](kart_common_MessageType msg_type, kart_common_ComponentType comp_type,
       uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value) {
        // Handle light mode command
        if (value == kart_lights_LightModeValue_ON) {
            // Turn on light
        } else if (value == kart_lights_LightModeValue_OFF) {
            // Turn off light
        }
    }
);
```

## Extending the Protocol

To extend the protocol for new components or commands:

1. Add new definitions to the appropriate .proto files
2. Rebuild the protocol using the build.sh script
3. Update the Python protocol implementation to include the new definitions

This ensures that all systems use the same protocol definitions.

---

The Go-Kart CAN protocol provides a flexible, efficient, and reliable communication system for the various components of the platform. By following this documentation, you can understand, use, and extend the protocol as needed. 