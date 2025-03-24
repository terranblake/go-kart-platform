# Go-Kart Dashboard Server

This server provides an interface for controlling and monitoring the Go-Kart platform via a CAN bus network.

## Features

- CAN bus communication with the Go-Kart components
- API for commanding the Go-Kart (steering, throttle, brakes, lights, etc.)
- Real-time telemetry monitoring
- Protocol exploration API
- WebSocket for real-time updates to the dashboard UI

## Installation

### Prerequisites

- Python 3.7 or higher
- CAN interface (socketcan compatible)
- Protocol Buffer compiler (protoc)

### Setting up the Environment

1. Clone the repository:
   ```
   git clone https://github.com/your-username/go-kart-platform.git
   cd go-kart-platform
   ```

2. Install dependencies:
   ```
   cd dashboard/server
   pip install -r requirements.txt
   ```

3. Build the Cython extension:
   ```
   pip install -e .
   ```

### Setting up the CAN Interface

On Linux, you can set up a virtual CAN interface for testing:

```
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

For a real CAN interface:

```
sudo ip link set can0 up type can bitrate 500000
```

## Running the Server

Start the server with:

```
python app.py
```

The server will be available at http://localhost:5000

## API Endpoints

### Control Endpoints

- `POST /api/command`: Send a control command (speed, steering, brake)
- `POST /api/lights/<mode>`: Control the lights

### Telemetry Endpoints

- `GET /api/telemetry`: Get current telemetry data
- `GET /api/telemetry/history`: Get historical telemetry data

### Protocol Endpoints

- `GET /api/protocol`: Get the complete protocol structure
- `GET /api/protocol/<component_type>`: Get information about a specific component type
- `GET /api/protocol/<component_type>/<component_name>`: Get information about a specific component
- `GET /api/protocol/<component_type>/<component_name>/<command_name>`: Get information about a specific command

## Protocol Structure

The Go-Kart uses a custom CAN protocol implemented with Protocol Buffers. See `docs/protocol.md` for a detailed description of the protocol.

## Testing

Run tests with:

```
pytest
```

## License

MIT 