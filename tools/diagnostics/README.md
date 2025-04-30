# Go-Kart Platform Diagnostic Tools

This directory contains tools for diagnosing and testing the Go-Kart Platform's CAN communication system.

## Tools

### CAN Data Simulator (`simulate_can_data.py`)

Simulates CAN messages using the existing CAN interface implementation to generate realistic test data.

#### Features

- Multiple simulation patterns:
  - `random`: Random messages across all components
  - `realistic`: Realistic messages based on typical vehicle states
  - `vehicle`: Simulates a moving vehicle with changing speed and steering
  - `sensors`: Focuses on sensor data with gradual, realistic changes
  - `navigation`: Simulates GPS, compass, and other navigational sensors with realistic movement patterns

#### Usage

```bash
# Basic usage with default parameters (vcan0 interface, realistic pattern)
python simulate_can_data.py

# Specify a different CAN channel
python simulate_can_data.py --channel=can0

# Choose a simulation pattern
python simulate_can_data.py --pattern=vehicle

# Run the navigation simulation
python simulate_can_data.py --pattern=navigation

# Run for a specific duration (in seconds)
python simulate_can_data.py --duration=60

# Adjust the message interval (default: 0.1s)
python simulate_can_data.py --interval=0.5

# Show debug logs
python simulate_can_data.py --debug
```

### CAN Monitor (`can_monitor.py`)

Monitors the CAN bus and displays messages in a human-readable format based on the protocol definitions.

#### Usage

```bash
# Monitor the default CAN interface (can0)
python can_monitor.py

# Specify a different interface
python can_monitor.py vcan0

# Display debug information about protocol enums
python can_monitor.py --debug
```

### Telemetry Viewer (`telemetry_viewer.py`)

A simple utility to view telemetry data collected by the Telemetry Collector service.

#### Features

- Live updating views:
  - Current state view: Shows the most recent state of all components
  - History view: Displays a table of recent telemetry updates
  - Component view: Focuses on a specific component's state

#### Usage

```bash
# View current state (default)
python telemetry_viewer.py

# View telemetry history
python telemetry_viewer.py --view=history

# View specific component state
python telemetry_viewer.py --view=component --component-type=MOTORS --component-id=DRIVE

# Connect to a different API endpoint
python telemetry_viewer.py --api-url=http://gokart-pi.local:5001

# Adjust refresh rate (in seconds)
python telemetry_viewer.py --refresh=2.5

# Enable debug logging
python telemetry_viewer.py --debug
```

## Setup

### Install Dependencies

```bash
pip install -r requirements.txt
```

### Setting up a Virtual CAN Interface (for testing without hardware)

On Linux, you can set up a virtual CAN interface for testing:

```bash
# Load the vcan kernel module
sudo modprobe vcan

# Create a virtual CAN interface
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Check if it's running
ip link show vcan0
```

## Example Workflow

1. Set up a virtual CAN interface
2. Start the CAN monitor in one terminal:
   ```bash
   python can_monitor.py vcan0
   ```
3. Start the CAN simulator in another terminal:
   ```bash
   python simulate_can_data.py --channel=vcan0 --pattern=vehicle
   ```
4. Observe the messages being sent and received
5. Try different simulation patterns:
   ```bash
   python simulate_can_data.py --channel=vcan0 --pattern=navigation
   ```
   This will simulate GPS coordinates, heading, altitude, and other navigational data

## Integration with Telemetry Collector

The simulation tools can be used to test the Telemetry Collector service:

1. Start the telemetry collector service:
   ```bash
   cd ../../telemetry/collector
   python collector.py
   ```

2. Run the CAN simulator to generate test data:
   ```bash
   cd tools/diagnostics
   python simulate_can_data.py --channel=can0
   ```

3. View the collected telemetry data:
   ```bash
   python telemetry_viewer.py
   ```

```bash
</rewritten_file> 