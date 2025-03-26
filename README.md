<!-- LLM_CONTEXT
project: Go-Kart Platform
purpose: Modular electric go-kart with telemetry and control systems
architecture: CAN bus network connecting microcontrollers to dashboard
key_components:
  - dashboard: Central UI and control system (Python/Flask)
  - components: Microcontroller modules (Arduino/C++)
  - telemetry: Data collection and storage
  - protocol: Standardized messaging format
-->

# Go-Kart Platform

## Core Concept
Modular electric go-kart with standardized communication, component control, and telemetry.

## System Architecture
```
┌─────────────────┐     ┌─────────────────┐
│    Dashboard    │     │    Telemetry    │
│  (Raspberry Pi) │◄────►  (Raspberry Pi) │
└────────┬────────┘     └────────┬────────┘
         ^                       ^
         │                       │
         ▼                       ▼
┌─────────────────────────────────────────┐
│              CAN Bus Network            │
└─────┬─────────┬──────────┬─────────┬────┘
      ^         ^          ^         ^
      ▼         ▼          ▼         ▼
┌──────────┐ ┌────────┐ ┌──────┐ ┌─────────┐
│  Lights  │ │ Motors │ │ BMS  │ │ Sensors │
└──────────┘ └────────┘ └──────┘ └─────────┘
```

## Component Reference

| Component | Purpose | Tech Stack | Key Files |
|-----------|---------|------------|-----------|
| Dashboard | UI and control | Python, Flask | dashboard/server/app.py |
| Telemetry | Data storage | Python, sqlite | telemetry/collector/ |
| Lights | Lighting control | Arduino, C++ | components/lights/ |
| Motors | Motor control | Arduino, C++ | components/motors/ |
| Protocol | Message definitions | Protobuf | protocol/kart_protocol.proto |
| CrossPlatformCAN | CAN interface | C++, Python | components/common/lib/CrossPlatformCAN/ |

## Protocol Summary
- **Message Format**: `[msg_type, comp_type, comp_id, cmd_id, value_type, value]`
- **Message Types**: COMMAND(0), STATUS(1), ACK(2), ERROR(3)
- **Component Types**: LIGHTS(0), MOTORS(1), SENSORS(2), BATTERY(3), CONTROLS(4)

## Quick Start
```bash
# Set up the CAN interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Start the dashboard
cd dashboard/server
python app.py &

# Start the telemetry collector
cd telemetry/collector
python collector.py &
```

## Project Structure
<!-- LLM_CODE_MARKER: project_structure -->
```
go-kart-platform/
├── dashboard/           # UI and command system
│   └── server/          # Flask server with CAN interface
├── telemetry/           # Data collection and storage
│   └── collector/       # CAN data collector
├── components/          # Arduino component controllers
│   ├── lights/          # Light control system
│   ├── motors/          # Motor control system
│   ├── batteries/       # Battery management
│   ├── inputs/          # User input processing
│   └── common/lib/      # Shared libraries
│       └── CrossPlatformCAN/  # CAN interface library
├── protocol/            # Protocol Buffer definitions
└── docs/                # Documentation
```
<!-- LLM_CODE_MARKER_END -->

## Development Patterns
<!-- LLM_CODE_MARKER: development_patterns -->
1. **Component Development**:
   - Edit component code in `components/{component}/src/`
   - Test locally with `platformio test`
   - Upload to Arduino with `platformio run -t upload`

2. **Dashboard Development**:
   - Edit server code in `dashboard/server/`
   - Test with `python app.py`
   - Access UI at `http://localhost:5000`

3. **Protocol Changes**:
   - Edit `protocol/kart_protocol.proto`
   - Generate code with `./protocol/build.sh`
   - Update implementations in affected components
<!-- LLM_CODE_MARKER_END -->

## See Also
- [CrossPlatformCAN Documentation](components/common/lib/CrossPlatformCAN/README.md)
- [Protocol Reference](protocol/README.md)
- [Component Implementation Guide](docs/components.md)
