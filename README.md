# Go-Kart Platform

This project implements a modular electric go-kart platform with rich telemetry, dynamic control systems, and an extensible architecture designed to make adding new components and sensors as easy as possible

## Project Overview

The Go-Kart Platform creates a high-performance electric vehicle with a data-rich ecosystem for configuration, monitoring, and control. The platform provides EV-like functionality scaled down for fabrication and testing purposes.

### Key Features

- On-board dashboard for real-time control and monitoring
- Comprehensive telemetry collection and storage
- Modular component control system
- CAN bus network with standardized protocol
- Remote telemetry storage and analysis
- Cross-device component messaging

## Project Structure

- **docs/** - Documentation
- **protocol/** - Protocol Buffer definitions for communication
- **dashboard/** - On-board dashboard UI (Raspberry Pi)
- **telemetry/** - Telemetry storage system (Raspberry Pi)
- **components/** - Arduino-based component controllers
  - **lights/** - Light control system
  - **motors/** - Motor control system
  - **batteries/** - Battery management system
  - **inputs/** - User input processing (throttle, brake, steering)
  - **sensors/** - Various sensor implementations
  - **common/lib/** - Common libraries
    - **[ProtobufCANInterface/](components/common/lib/ProtobufCANInterface/README.md)** - Cross-platform Protocol Buffer CAN interface
- **cluster/** - Kubernetes-based cloud infrastructure
- **hardware/** - Hardware specifications and designs
- **tools/** - Development and debugging tools

## CAN Communication Interface

The heart of the go-kart platform's communication system is the [ProtobufCANInterface](components/common/lib/ProtobufCANInterface/README.md), a cross-platform library that enables standardized messaging between all components using Protocol Buffers over CAN bus.

### Key Features

- Supports both Arduino (embedded) and non-Arduino (desktop) environments
- Implements Protocol Buffer message encoding/decoding
- Provides event-based message handling with callbacks
- Offers comprehensive protocol for lights, motors, controls, and other systems
- Includes detailed debugging support
- Automatically tested via continuous integration

To learn more about the available protocol commands and how to use them, see the [ProtobufCANInterface documentation](components/common/lib/ProtobufCANInterface/README.md).

## Animation Support

The Go-Kart Platform includes support for LED animations through the LED control system. Animations are defined as a series of frames, where each frame contains LED color data.

### Animation Features

- Frame-based animation system for LED control
- Animation playback over the CAN bus
- Support for creating and storing animations
- Animation commands: start, frame-by-frame data transfer, stop

### Animation Implementation

The animation system works through binary data transfer over the CAN bus using the following commands:
- `ANIMATION_START`: Initiates an animation sequence
- `ANIMATION_FRAME`: Sends a single frame of animation data
- `ANIMATION_END`: Marks the end of an animation sequence
- `ANIMATION_STOP`: Immediately stops the current animation

### Recent Animation System Improvements

- Fixed C API type conversion issues in CrossPlatformCAN library to ensure proper binary data transmission
- Enhanced protocol registry to ensure animation commands are always properly registered
- Added automated tests to verify animation protocol functionality
- Improved binary data handling between Raspberry Pi and Arduino components

See [TODO.md](TODO.md) for current issues and planned improvements to the animation system.

<details>
```
go-kart-platform/
├── README.md                     # Project overview, setup instructions
├── docs/                         # Documentation
│   ├── architecture/             # System architecture diagrams and descriptions
│   ├── hardware/                 # Hardware specifications and wiring diagrams
│   └── protocol/                 # Protocol documentation and examples
│
├── protocol/                     # Protocol Buffer definition
│   ├── kart_protocol.proto       # Main protocol definition file
│   ├── build.sh                  # Protocol compilation script
│   ├── nanopb/                   # Nanopb submodule or local copy
│   │   └── generator/            # Nanopb generator tools
│   │
│   └── generated/                # Generated code for different platforms
│       ├── python/               # Python protocol bindings
│       │   ├── kart_protocol_pb2.py # Generated Python protocol
│       │   └── README.md         # Usage instructions
│       │
│       └── nanopb/               # Optimized nanopb bindings for embedded devices
│           ├── kart_protocol.pb.h # Generated nanopb header
│           ├── kart_protocol.pb.c # Generated nanopb implementation
│           └── README.md         # Usage instructions
│
├── dashboard/                    # Primary Raspberry Pi Zero W (On-board Dashboard)
│   ├── server/                   # Python web server for UI and command
│   │   ├── app.py                # Main application entry point
│   │   ├── requirements.txt      # Python dependencies
│   │   ├── static/               # Static web assets
│   │   │   ├── css/              # Stylesheets
│   │   │   ├── js/               # JavaScript files
│   │   │   └── img/              # Images and icons
│   │   │
│   │   ├── templates/            # HTML templates
│   │   │   ├── dashboard.html    # Main dashboard interface
│   │   │   ├── settings.html     # Settings interface
│   │   │   └── components/       # Reusable UI components
│   │   │
│   │   ├── lib/                  # Server libraries
│   │   │   ├── can_interface.py  # CAN bus communication
│   │   │   ├── protocol.py       # Protocol message handling
│   │   │   ├── components.py     # Component management
│   │   │   └── telemetry.py      # Short-term telemetry storage
│   │   │
│   │   └── api/                  # API endpoints
│   │       ├── commands.py       # Command endpoints
│   │       └── telemetry.py      # Telemetry endpoints
│   │
│   ├── config/                   # Configuration files
│   │   ├── network.conf          # Network configuration
│   │   └── components.conf       # Component configuration
│   │
│   └── scripts/                  # Utility scripts
│       ├── setup.sh              # Initial setup script
│       ├── start.sh              # Start server script
│       └── can_setup.sh          # CAN interface setup
│
├── telemetry/                    # Secondary Raspberry Pi Zero W (Telemetry Storage)
│   ├── collector/                # Telemetry collector application
│   │   ├── collector.py          # Main collector application
│   │   ├── requirements.txt      # Python dependencies
│   │   ├── storage.py            # Local storage management
│   │   ├── uploader.py           # Remote upload functionality
│   │   └── can_interface.py      # CAN bus interface
│   │
│   ├── config/                   # Configuration files
│   │   ├── storage.conf          # Storage configuration
│   │   └── network.conf          # Network configuration
│   │
│   └── scripts/                  # Utility scripts
│       ├── setup.sh              # Setup script
│       └── mount.sh              # Storage mount script
│
├── components/                   # PlatformIO projects for all component controllers
│   ├── platformio.ini            # Global PlatformIO config for all components
│   ├── common/                   # Common code shared by all components
│   │   ├── lib/                  # Common libraries
│   │   │   ├── CanInterface/     # CAN bus interface library
│   │   │   │   ├── src/
│   │   │   │   │   ├── CanInterface.cpp
│   │   │   │   │   └── CanInterface.h
│   │   │   │   └── library.json
│   │   │   │
│   │   │   └── ProtocolHandler/  # Protocol handling library
│   │   │       ├── src/
│   │   │       │   ├── ProtocolHandler.cpp
│   │   │       │   └── ProtocolHandler.h
│   │   │       └── library.json
│   │   │
│   │   └── scripts/              # Build scripts
│   │       └── component_builder.py # Script to handle component selection
│   │
│   ├── lights/                   # Lights component
│   │   ├── src/                  # Project source files
│   │   │   ├── main.cpp          # Main application file
│   │   │   ├── LightComponents.cpp # Light component implementations
│   │   │   └── LightComponents.h # Light component definitions
│   │   ├── include/              # Project include files
│   │   │   └── Config.h          # Component configuration
│   │   └── test/                 # Unit tests
│   │
│   ├── motors/                   # Motor component
│   │   ├── src/                  # Project source files
│   │   │   ├── main.cpp          # Main application file
│   │   │   ├── MotorComponents.cpp # Motor component implementations
│   │   │   └── MotorComponents.h # Motor component definitions
│   │   ├── include/              # Project include files
│   │   │   └── Config.h          # Component configuration
│   │   └── test/                 # Unit tests
│   │
│   ├── batteries/                # Battery component
│   │   ├── src/                  # Project source files
│   │   │   ├── main.cpp          # Main application file
│   │   │   ├── BatteryComponents.cpp # Battery component implementations
│   │   │   └── BatteryComponents.h # Battery component definitions
│   │   ├── include/              # Project include files
│   │   │   └── Config.h          # Component configuration
│   │   └── test/                 # Unit tests
│   │
│   ├── inputs/                   # User input component (throttle, brake, steering)
│   │   ├── src/                  # Project source files
│   │   │   ├── main.cpp          # Main application file
│   │   │   ├── InputComponents.cpp # Input component implementations
│   │   │   └── InputComponents.h # Input component definitions
│   │   ├── include/              # Project include files
│   │   │   └── Config.h          # Component configuration
│   │   └── test/                 # Unit tests
│   │
│   └── sensors/                  # Sensors component with variants
│       ├── src/                  # Common source files
│       │   ├── main.cpp          # Main application common to all sensors
│       │   ├── SensorBase.cpp    # Base sensor implementation
│       │   └── SensorBase.h      # Base sensor interface
│       │
│       ├── include/              # Common include files
│       │   └── CommonConfig.h    # Shared configuration settings
│       │
│       ├── lib/                  # Sensor-specific libraries
│       │   ├── TemperatureSensors/ # Temperature sensor library
│       │   │   ├── src/
│       │   │   │   ├── TemperatureSensor.cpp
│       │   │   │   └── TemperatureSensor.h
│       │   │   └── library.json
│       │   │
│       │   ├── MotionSensors/    # Motion sensors (accel, gyro) library
│       │   │   ├── src/
│       │   │   │   ├── AccelerometerSensor.cpp
│       │   │   │   ├── AccelerometerSensor.h
│       │   │   │   ├── GyroscopeSensor.cpp
│       │   │   │   └── GyroscopeSensor.h
│       │   │   └── library.json
│       │   │
│       │   ├── GPSSensor/        # GPS sensor library
│       │   │   ├── src/
│       │   │   │   ├── GPSSensor.cpp
│       │   │   │   └── GPSSensor.h
│       │   │   └── library.json
│       │   │
│       │   └── ProximitySensors/ # Proximity sensors library
│       │       ├── src/
│       │       │   ├── ProximitySensor.cpp
│       │       │   └── ProximitySensor.h
│       │       └── library.json
│       │
│       ├── variants/             # Sensor variant-specific code
│       │   ├── temperature/      # Temperature sensor variant
│       │   │   ├── Config.h      # Temperature-specific configuration
│       │   │   └── SensorImplementation.cpp  # Temperature implementation
│       │   │
│       │   ├── motion/           # Motion sensor variant
│       │   │   ├── Config.h      # Motion-specific configuration
│       │   │   └── SensorImplementation.cpp  # Motion implementation
│       │   │
│       │   ├── gps/              # GPS sensor variant
│       │   │   ├── Config.h      # GPS-specific configuration
│       │   │   └── SensorImplementation.cpp  # GPS implementation
│       │   │
│       │   └── proximity/        # Proximity sensor variant
│       │       ├── Config.h      # Proximity-specific configuration
│       │       └── SensorImplementation.cpp  # Proximity implementation
│       │
│       └── test/                 # Tests for all sensor variants
│
├── cluster/                      # Kubernetes cluster for remote telemetry storage
│   ├── kubernetes/               # Kubernetes deployment manifests
│   │   ├── telemetry-storage/    # Telemetry storage service
│   │   │   ├── deployment.yaml   # Pod deployment configuration
│   │   │   ├── service.yaml      # Service configuration
│   │   │   └── pvc.yaml          # Persistent volume claim
│   │   │
│   │   └── telemetry-api/        # Telemetry API service
│   │       ├── deployment.yaml   # API deployment configuration
│   │       ├── service.yaml      # Service configuration
│   │       └── ingress.yaml      # Ingress configuration
│   │
│   ├── docker/                   # Docker container definitions
│   │   ├── telemetry-storage/    # Telemetry storage container
│   │   │   └── Dockerfile        # Container definition
│   │   │
│   │   └── telemetry-api/        # Telemetry API container
│   │       └── Dockerfile        # Container definition
│   │
│   ├── services/                 # Microservice implementations
│   │   ├── telemetry-storage/    # Telemetry storage service
│   │   │   ├── app.py            # Main application
│   │   │   ├── requirements.txt  # Dependencies
│   │   │   └── storage.py        # Storage implementation
│   │   │
│   │   └── telemetry-api/        # Telemetry API service
│   │       ├── app.py            # Main application
│   │       ├── requirements.txt  # Dependencies
│   │       └── api.py            # API implementation
│   │
│   └── grafana/                  # Grafana dashboard configuration
│       ├── dashboards/           # Dashboard JSON definitions
│       │   ├── overview.json     # Overview dashboard
│       │   ├── telemetry.json    # Detailed telemetry dashboard
│       │   └── components.json   # Component-specific dashboards
│       │
│       └── provisioning/         # Grafana provisioning configuration
│           ├── dashboards/       # Dashboard provisioning
│           └── datasources/      # Data source configuration
│
├── hardware/                     # Hardware designs and documentation
│   ├── wiring/                   # Wiring diagrams
│   │   ├── main_harness.pdf      # Main wiring harness diagram
│   │   ├── can_bus.pdf           # CAN bus wiring diagram
│   │   └── power.pdf             # Power distribution diagram
│   │
│   ├── components/               # Component documentation
│   │   ├── rpi_zero_w/           # Raspberry Pi Zero W documentation
│   │   ├── arduino_nano/         # Arduino Nano documentation
│   │   ├── mcp2515/              # MCP2515 CAN module documentation
│   │   └── components/           # Physical component documentation
│   │
│   └── mounting/                 # Mounting and enclosure designs
│       ├── dashboard.stl         # Dashboard enclosure 3D model
│       ├── telemetry.stl         # Telemetry enclosure 3D model
│       └── components/           # Component enclosures
│
└── tools/                        # Development and testing tools
    ├── simulator/                # CAN bus simulator
    │   ├── simulator.py          # CAN bus simulator script
    │   └── requirements.txt      # Dependencies
    │
    ├── protocol-tools/           # Protocol testing tools
    │   ├── message_builder.py    # Message building utility
    │   ├── message_tester.py     # Message testing utility
    │   └── proto_compiler.sh     # Script to compile protocol for all targets
    │
    └── diagnostics/              # Diagnostic tools
        ├── can_monitor.py        # CAN bus monitoring tool
        └── telemetry_viewer.py   # Telemetry viewing tool
```
</details>


## Architecture

### System Architecture

The system is designed around a distributed architecture with multiple microcontroller units (MCUs) communicating over a Controller Area Network (CAN):

1. **Dashboard MCU** (Raspberry Pi Zero W)
   - Processes user input
   - Displays system status on a 10" tablet
   - Commands state changes over the CAN network
   - Provides short-term telemetry storage (5 min)

2. **Telemetry MCU** (Raspberry Pi Zero W)
   - Stores all telemetry data
   - Transmits data to remote cluster when network is available
   - Potential future expansion for camera streams

3. **Component MCUs** (Arduino Nano)
   - Dedicated MCUs for each logical component group
   - Processes sensor data
   - Responds to commands
   - Reports component state

4. **Remote Cluster** (Kubernetes)
   - Long-term telemetry storage
   - Data analysis and visualization via Grafana
   - API endpoints for remote access

### Communication Protocol

All devices communicate via a MCP2515 CAN bus network (8 bytes/message) implementing Protocol Buffers to standardize messaging:

- **Component Types**: Lights, Motors, Batteries, Controls, Sensors
- **Components**: Map to physical devices (lights.front, batteries.rear_left, controls.brake)
- **Commands**: Actions for each component (lights.mode, controls.steering.angle, motors.speed)
- **States**: Updated telemetry data (state.lights.mode, state.motors.temperature, state.batteries.voltage)

This protocol provides a consistent interface across all system components with strongly-typed messages and efficient binary encoding.

### Development Architecture

The project uses PlatformIO for component development with a unified configuration approach:

- Global `platformio.ini` for all components
- Shared libraries for common functionality
- Component-specific implementations
- Multi-variant sensor configurations
- Environment-based build targets

## Component Descriptions

### Dashboard

The dashboard runs on a Raspberry Pi Zero W and provides:

- Web-based user interface displayed on a 10" tablet
- Component configuration controls
- Real-time telemetry display
- Command interface to all on-board systems
- Short-term telemetry storage and playback

### Telemetry Storage

The telemetry system runs on a separate Raspberry Pi Zero W and:

- Captures all CAN bus messages
- Stores data locally until network connection is available
- Uploads data to remote cluster for permanent storage
- Maintains system history and analytics data

### Component Controllers

Arduino-based controllers manage physical components:

- **Lights**: Front/rear lights, turn signals, brake lights, interior lighting
- **Motors**: Drive motors, speed control, temperature monitoring
- **Batteries**: Voltage/current monitoring, charge state, temperature
- **Inputs**: Handles analog inputs (throttle, brake, steering)
- **Sensors**: Multiple variants for different sensor types:
  - Temperature sensors
  - Motion sensors (accelerometer, gyroscope)
  - GPS location
  - Proximity detection

## User Interaction

The end user interacts with the go-kart through:

1. **Physical Controls**
   - Analog steering, throttle, and brake inputs
   - Physical emergency controls

2. **Dashboard Interface**
   - 10" tablet mounted on the kart
   - Touch interface for configuration and monitoring
   - Real-time status display
   - Component control (non-critical systems)
   
3. **Remote Monitoring**
   - Grafana dashboards for telemetry analysis
   - Historical data viewing
   - Performance tracking

## Extending the Platform

### Adding New Component Types

1. **Update Protocol Definition**
   - Add new component type to `kart_protocol.proto`
   - Define command and state messages
   - Run protocol compiler to generate bindings

2. **Create Component Implementation**
   - Add new directory under `components/`
   - Implement component-specific logic
   - Add to `platformio.ini` as a new environment

3. **Update Dashboard**
   - Add UI components for new functionality
   - Create command handlers
   - Add telemetry display elements

### Adding New Sensor Variants

1. **Create Sensor Library**
   - Add new library under `components/sensors/lib/`
   - Implement sensor driver code

2. **Create Sensor Variant**
   - Add new variant under `components/sensors/variants/`
   - Implement `SensorImplementation.cpp`
   - Define `Config.h` with pin assignments

3. **Update PlatformIO Configuration**
   - Add new environment in `platformio.ini`
   - Specify build flags and dependencies

### Extending Message Protocol

todo (add links to specific files and include examples of extensibility)

1. **Modify Protocol Definition**
   - Add new message types, enums, or fields
   - Maintain backward compatibility where possible
   - Update version number

2. **Regenerate Bindings**
   - Run protocol compiler for all platforms
   - Update Python bindings for dashboard/telemetry
   - Update C/C++ bindings for Arduino components

3. **Update Message Handlers**
   - Modify code to handle new message types
   - Update serialization/deserialization logic
   - Test compatibility

### Setting Up the Project

```bash
# Clone the repository
git clone https://github.com/yourusername/go-kart-platform.git
cd go-kart-platform

todo(finish adding setup steps)
```

### Building Components

```bash
# Navigate to the components directory
cd components

# Build the lights component
COMPONENT=lights pio run -e lights

# Build a specific sensor variant
COMPONENT=sensors pio run -e temp_sensor

# Upload to a connected board
COMPONENT=motors pio run -e motors -t upload
```

### Setting Up Raspberry Pi Systems

```bash
# Set up the dashboard
cd dashboard/scripts
./setup.sh

# Set up the telemetry system
cd telemetry/scripts
./setup.sh
```

### Deploying the Cluster

```bash
# Deploy the Kubernetes resources
kubectl apply -f cluster/kubernetes/telemetry-storage/
kubectl apply -f cluster/kubernetes/telemetry-api/

# Set up Grafana dashboards
kubectl apply -f cluster/grafana/
```
