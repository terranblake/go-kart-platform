<!-- LLM_CONTEXT
component: dashboard/server
purpose: Central control system with web UI and CAN communication
language: Python with Flask and SocketIO
key_classes: 
  - CANInterfaceWrapper: High-level CAN communication
  - ProtocolRegistry: Protocol message mapping
  - TelemetryStore: State storage
communication: Sends commands to components, receives status updates
-->

# Go-Kart Dashboard Server

## Overview
Central control system for the go-kart platform with web UI, CAN communication, and telemetry management.

## Architecture
<!-- LLM_CODE_MARKER: architecture -->
```
┌─────────────┐      ┌───────────────┐
│ Web Browser │◄────►│  Flask/Socket │
└─────────────┘      │      API      │
                     └───────┬───────┘
                             │
                     ┌───────┴───────┐
                     │CANInterfaceWrap│
                     └───────┬───────┘
                             │
                     ┌───────┴───────┐
                     │ SocketCAN/CFFI│
                     └───────┬───────┘
                             │
                             ▼
                     ┌───────────────┐
                     │   CAN Bus     │
                     └───────────────┘
```
<!-- LLM_CODE_MARKER_END -->

## Core Components
<!-- LLM_CODE_MARKER: core_components -->
| Component | Purpose | Key Methods |
|-----------|---------|-------------|
| CANInterfaceWrapper | High-level CAN API | send_message(), register_handler(), send_command() |
| ProtocolRegistry | Maps protocol definitions | create_message(), get_component_type(), get_command_id() |
| TelemetryStore | State management | update_state(), get_component_state() |
| SocketIO | Real-time UI updates | emit(), on() |
<!-- LLM_CODE_MARKER_END -->

## Message Flow
<!-- LLM_CODE_MARKER: message_flow -->
1. **Web UI → API**: User clicks button or makes setting change
2. **API → CANInterfaceWrapper**: Calls appropriate handler
3. **CANInterfaceWrapper → ProtocolRegistry**: Creates message with correct values
4. **CANInterfaceWrapper → CAN Bus**: Sends message to components 
5. **CAN Bus → CANInterfaceWrapper**: Receives status updates
6. **CANInterfaceWrapper → TelemetryStore**: Updates component state
7. **TelemetryStore → SocketIO → Web UI**: Updates UI with new state
<!-- LLM_CODE_MARKER_END -->

## API Reference

### CANInterfaceWrapper
<!-- LLM_API_BOUNDARY: CANInterfaceWrapper -->
```python
class CANInterfaceWrapper:
    """High-level CAN interface for the dashboard server."""
    
    def __init__(self, node_id=0x01, channel='can0', baudrate=500000, 
                 telemetry_store=None):
        """Initialize the CAN interface."""
        
    def start_processing(self):
        """Start background processing of CAN messages."""
        
    def stop_processing(self):
        """Stop background processing of CAN messages."""
        
    def send_message(self, msg_type, comp_type, comp_id, cmd_id, 
                    value_type, value):
        """Send a raw CAN message."""
        
    def send_command(self, message_type_name, component_type_name, 
                    component_name, command_name, value_name=None, 
                    direct_value=None):
        """Send a high-level command using protocol registry."""
        
    def register_handler(self, message_type, comp_type, comp_id, 
                        cmd_id, handler):
        """Register a handler for incoming messages."""
```
<!-- LLM_API_END -->

### ProtocolRegistry
<!-- LLM_API_BOUNDARY: ProtocolRegistry -->
```python
class ProtocolRegistry:
    """Registry for protocol definitions and message creation."""
    
    def create_message(self, message_type, component_type, 
                      component_name, command_name, value_name=None, 
                      value=None):
        """Create a complete message tuple from high-level parameters."""
        
    def get_message_type(self, message_type_name):
        """Get numeric message type from name."""
        
    def get_component_type(self, component_type_name):
        """Get numeric component type from name."""
        
    def get_component_id(self, component_type, component_name):
        """Get numeric component ID from type and name."""
        
    def get_command_id(self, component_type, command_name):
        """Get numeric command ID from component type and command name."""
        
    def get_command_value(self, component_type, command_name, value_name):
        """Get numeric value from component type, command and value names."""
```
<!-- LLM_API_END -->

## Usage Examples
<!-- LLM_CODE_MARKER: usage_examples -->
### Sending a Command
```python
# Turn on front lights
can_interface.send_command(
    message_type_name="COMMAND", 
    component_type_name="LIGHTS",
    component_name="FRONT", 
    command_name="MODE",
    value_name="ON"
)

# Set motor speed
can_interface.send_command(
    message_type_name="COMMAND", 
    component_type_name="MOTORS",
    component_name="MAIN", 
    command_name="SPEED",
    direct_value=50  # 50% power
)
```

### Registering a Handler
```python
def handle_light_status(msg_type, comp_type, comp_id, cmd_id, val_type, value):
    print(f"Light status update: {value}")
    
# Register for light status messages
can_interface.register_handler(
    message_type="STATUS",
    comp_type="LIGHTS",
    comp_id=0x01,  # Front lights
    cmd_id=0x00,   # MODE command
    handler=handle_light_status
)
```
<!-- LLM_CODE_MARKER_END -->

## Common Problems & Solutions
<!-- LLM_CODE_MARKER: troubleshooting -->
| Problem | Possible Cause | Solution |
|---------|----------------|----------|
| Cannot connect to CAN | Interface not up | `sudo ip link set up can0` |
| Message not received | Wrong component ID | Check component_id in send_command |
| No UI updates | WebSocket disconnect | Check browser console, restart Flask |
| Unknown component/command | Protocol mismatch | Update protocol registry or regenerate code |
| Message processing errors | Handler exception | Check logs, fix handler code |
<!-- LLM_CODE_MARKER_END -->

## Setup Instructions
```bash
# Install dependencies
pip install -r requirements.txt

# Configure CAN interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Run server
python app.py
``` 