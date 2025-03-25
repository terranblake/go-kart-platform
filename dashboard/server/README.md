# Go-Kart Dashboard Server

This is the backend server for the Go-Kart dashboard. It provides APIs for telemetry data, command sending, and protocol documentation.

## Features

- Real-time telemetry data via SocketIO
- REST APIs for sending commands and retrieving state
- Integration with the ProtobufCANInterface for CAN communication
- Protocol documentation API

## Installation

### Prerequisites

- Python 3.6+
- CAN interface (e.g., SocketCAN on Linux)
- C++ compiler (for building the Cython extension)

### Setup

1. Clone the repository and navigate to the dashboard server directory:

```bash
git clone https://github.com/yourusername/go-kart-platform.git
cd go-kart-platform/dashboard/server
```

2. Create and activate a virtual environment:

```bash
python -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate
```

3. Install the package in development mode:

```bash
pip install -e .
```

This will build the Cython extension for the CAN interface.

## Running the Server

1. Make sure your CAN interface is set up. On Linux, you can set up a virtual CAN interface for testing:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
```

2. Start the server:

```bash
python app.py
```

The server will start on port 5000 by default.

## API Documentation

### Telemetry Endpoints

- `GET /api/state` - Get the current state of the Go-Kart
- `GET /api/history` - Get telemetry history
- `GET /api/telemetry/status` - Get telemetry connection status

### Command Endpoints

- `POST /api/command` - Send a command to the Go-Kart

<!-- deprecated the camera route for now since its too resource intensive for a single rpi to handle -->
<!-- - `GET /api/camera/status` - Get camera status -->

### Protocol Endpoints

- `GET /api/protocol` - Get the complete protocol structure
- `GET /api/protocol/components/{component_name}` - Get information about a specific component
- `GET /api/protocol/components/{component_name}/commands/{command_name}` - Get information about a specific command
- `POST /api/send_command` - Send a command to the CAN bus
  - Parameters: `component`, `command`, `value`

## SocketIO Events

- `state_update` - Emitted when the Go-Kart state changes

## Development

### Architecture

- `app.py` - Main entry point and Flask application setup
- `api/` - API endpoints
- `lib/` - Backend libraries
  - `can/` - CAN interface and protocol implementation
  - `telemetry/` - Telemetry data processing

### Building the Cython Extension

If you modify the Cython code, you need to rebuild the extension:

```bash
pip install -e .
```

### Debugging

To enable debug mode, set the `FLASK_ENV` environment variable to `development`:

```bash
export FLASK_ENV=development
python app.py
```

## License

MIT 