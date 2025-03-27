# Go-Kart Platform: LLM Context Primer

This document provides essential context for LLMs working with the Go-Kart Platform codebase.

## Core Concepts

<!-- LLM_CODE_MARKER: core_concepts -->
- **CAN Bus**: Primary communication backbone between all components
- **Protocol Messages**: Standardized format (msg_type, comp_type, comp_id, cmd_id, value_type, value)
- **Components**: Physical modules with specific responsibilities (lights, motors, etc.)
- **Dashboard**: Central control system running Flask/Python
- **Telemetry**: Data collection and storage system
- **CrossPlatformCAN**: Library that abstracts CAN communication across platforms
<!-- LLM_CODE_MARKER_END -->

## Component Relationships

<!-- LLM_CODE_MARKER: component_relationships -->
```
dashboard -(CAN commands)-> components
components -(CAN status)-> dashboard
dashboard -(WebSocket)-> browser UI
telemetry -(data storage)-> database
```
<!-- LLM_CODE_MARKER_END -->

## Technology Stack

<!-- LLM_CODE_MARKER: technology_stack -->
| Component | Languages | Frameworks | Hardware |
|-----------|-----------|------------|----------|
| Dashboard | Python | Flask, SocketIO | Raspberry Pi |
| Components | C++ | Arduino, PlatformIO | Arduino/ESP32 |
| Protocol | Protocol Buffers | nanopb | - |
| Telemetry | Python | SQLite | Raspberry Pi |
<!-- LLM_CODE_MARKER_END -->

## Development Patterns

<!-- LLM_CODE_MARKER: development_patterns -->
1. **Component Development**:
   - Edit component code in `components/{component}/src/`
   - Test locally with `pio test` (use renode for hardware virtualization)
   - Upload to Arduino with `pio run -t upload`

2. **Dashboard Development**:
   - Edit server code in `dashboard/server/`
   - Test with `python app.py`
   - Access UI at `http://localhost:5000`

3. **Protocol Changes**:
   - Edit `protocol/*.proto`
   - Generate code with `./protocol/build.sh`
   - Update implementations in affected components
<!-- LLM_CODE_MARKER_END -->

## Execution Environment

<!-- LLM_CODE_MARKER: execution_environment -->
- **Dashboard**: Runs on Raspberry Pi with Linux (Raspbian)
- **Components**: Run on Arduino-compatible microcontrollers
- **Development**: PlatformIO for components, Python virtual env for dashboard
- **CAN Interface**: MCP2515 CAN controller connected via SPI
<!-- LLM_CODE_MARKER_END -->

## Terminal Sessions

<!-- LLM_CODE_MARKER: terminal_sessions -->
### Non-Interactive Commands
Always use these patterns when running terminal commands:

- For apt: `sudo apt-get install -y [package]`
- Server processes: `python app.py &` (use `&` to run in background)
- Build processes: `./build.sh > build.log 2>&1` (redirect output)
- Process checking: `ps aux | grep [process]`
- Process termination: `pkill -f [process]`

### Background Process Pattern
```bash
# Start a process in the background
nohup python dashboard/server/app.py > dashboard.log 2>&1 &

# Get the process ID
pid=$!

# Save the PID for later reference
echo $pid > dashboard.pid

# Check if process is running
if kill -0 $pid 2>/dev/null; then
    echo "Dashboard is running with PID $pid"
else
    echo "Dashboard failed to start"
fi
```
<!-- LLM_CODE_MARKER_END -->

## Testing Strategies

<!-- LLM_CODE_MARKER: testing_strategies -->
### Component Testing
- Unit tests for individual functions
- Integration tests that verify CAN message handling
- Test mode for physical testing of components

### Dashboard Testing
- API endpoint tests with pytest
- CAN communication tests with virtual CAN interface
- UI tests with Selenium (optional)

### Test Exit Conditions
- All tests must pass with exit code 0
- Coverage must be at least 80%
- No memory leaks in C++ code
<!-- LLM_CODE_MARKER_END -->

## Common Workflows

<!-- LLM_CODE_MARKER: common_workflows -->
### Local-Remote Development
1. LOCAL: Make code changes in `components/[component]`
2. LOCAL: Run `./scripts/test_local.sh [component]` to validate
3. LOCAL: If tests pass, run `./scripts/build_deployment.sh [component]`
4. REMOTE: Run `./scripts/deploy.sh [component]` to install on target
5. REMOTE: Run `./scripts/verify_deployment.sh [component]` to confirm

### Protocol Updates
1. Edit protocol definition in `protocol/[component].proto`
2. Run `./protocol/build.sh` to generate code for all platforms
3. Update component implementations to match new protocol
4. Update dashboard server protocol registry
5. Test communication between dashboard and components
<!-- LLM_CODE_MARKER_END -->

## Solution Constraints

<!-- LLM_CODE_MARKER: solution_constraints -->
### Required Characteristics
- All CAN communication MUST use the CrossPlatformCAN library
- All component tests MUST pass before deployment
- No direct hardware access outside abstraction layers

### Forbidden Approaches
- Bypassing protocol validation
- Hardcoding values that should come from configuration
- Disabling error handling for convenience
- Modifying generated protocol code directly
<!-- LLM_CODE_MARKER_END -->

## Key File Locations

<!-- LLM_CODE_MARKER: key_file_locations -->
| Purpose | File Path | Description |
|---------|-----------|-------------|
| Dashboard Entry | dashboard/server/app.py | Flask server main entry point |
| CAN Interface | dashboard/server/lib/can/interface.py | Dashboard CAN implementation |
| Protocol Registry | dashboard/server/lib/can/protocol_registry.py | Protocol message mapping |
| Lights Controller | components/lights/src/main.cpp | Lights component implementation |
| Protocol Definitions | protocol/*.proto | Protocol definitions |
| CrossPlatformCAN | components/common/lib/CrossPlatformCAN | CAN communication library |
| Component Config | components/lights/include/Config.h | Component configuration |
<!-- LLM_CODE_MARKER_END -->

## Troubleshooting Guide

<!-- LLM_CODE_MARKER: troubleshooting_guide -->
### CAN Communication Issues
- Check if interface is up: `ip -details link show can0`
- Verify wiring and connections to CAN controller
- Monitor CAN traffic: `candump can0`
- Send test message: `cansend can0 123#DEADBEEF`

### Dashboard Server Issues
- Check if process is running: `ps aux | grep app.py`
- Verify log output: `tail -f dashboard.log`
- Check network connectivity: `curl http://localhost:5000/api/status`
- Restart server: `pkill -f app.py && python dashboard/server/app.py &`

### Component Issues
- Check serial output: `platformio device monitor`
- Verify upload: `platformio run -t upload`
- Test with debug build: `platformio run -e debug`
<!-- LLM_CODE_MARKER_END --> 