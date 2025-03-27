# Go-Kart Platform Workflow Patterns

This document provides standardized workflow patterns for common development tasks, especially those requiring multiple terminals or local-remote coordination.

## Development Environment Setup

<!-- LLM_CODE_MARKER: setup_workflow -->
```bash
# Step 1: Clone repository
git clone https://github.com/username/go-kart-platform.git
cd go-kart-platform

# Step 2: Install dependencies
sudo apt-get update
sudo apt-get install -y python3-pip can-utils cmake build-essential

# Step 3: Set up virtual CAN for testing
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0

# Step 4: Install Python dependencies for dashboard
cd dashboard/server
pip install -r requirements.txt
cd ../..

# Step 5: Install PlatformIO for component development
pip install platformio

# Step 6: Generate protocol files
cd protocol
./build.sh
cd ..
```
<!-- LLM_CODE_MARKER_END -->

## Component Development Workflow

<!-- LLM_CODE_MARKER: component_workflow -->
### Basic Development Pattern

**Terminal 1: Code and Build**
```bash
# Edit component code
cd components/lights
# Edit src/main.cpp or include/Config.h

# Build component
platformio run

# Run component tests
platformio test
```

**Terminal 2: Monitor CAN**
```bash
# Start CAN monitoring
candump vcan0
```

### Complete Development Cycle

1. Edit component code (Terminal 1)
2. Build and run tests (Terminal 1)
3. Start CAN monitoring (Terminal 2)
4. Upload to device (Terminal 1)
5. Test manually (Terminal 3)
6. Capture logs for analysis (Terminal 2)
7. Repeat as needed
<!-- LLM_CODE_MARKER_END -->

## Dashboard Development Workflow

<!-- LLM_CODE_MARKER: dashboard_workflow -->
### Basic Development Pattern

**Terminal 1: Code and Run Server**
```bash
cd dashboard/server
# Edit app.py or lib files

# Run server in background
python app.py > dashboard.log 2>&1 &
# Store PID for later termination
server_pid=$!
```

**Terminal 2: Monitor Logs**
```bash
# Monitor server logs
tail -f dashboard/server/dashboard.log
```

**Terminal 3: Send Test Commands**
```bash
# Send test CAN message
cansend vcan0 123#0100010000
```

### Complete Development Cycle

1. Edit server code (Terminal 1)
2. Start server in background (Terminal 1)
3. Monitor logs in real-time (Terminal 2)
4. Send test commands (Terminal 3)
5. Check web UI in browser (http://localhost:5000)
6. Terminate server when done (Terminal 1)
   ```bash
   kill $server_pid
   ```
7. Repeat as needed
<!-- LLM_CODE_MARKER_END -->

## Local-Remote Development Workflow

<!-- LLM_CODE_MARKER: local_remote_workflow -->
### Setup

**Local Terminal: Configure SSH**
```bash
# Ensure passwordless SSH is set up
ssh-keygen -t rsa -b 4096
ssh-copy-id pi@raspberrypi.local
```

### Development Cycle

**Local Terminal 1: Code and Build**
```bash
# Edit and build component
cd components/lights
platformio run

# Package for deployment
mkdir -p deploy
cp .pio/build/*/firmware.bin deploy/
cp include/Config.h deploy/
```

**Local Terminal 2: Deploy**
```bash
# Create deployment package
cd components/lights
tar -czf lights.tar.gz deploy

# Copy to Raspberry Pi
scp lights.tar.gz pi@raspberrypi.local:~/

# Deploy remotely
ssh pi@raspberrypi.local "mkdir -p ~/go-kart/components/lights && \
  tar -xzf lights.tar.gz -C ~/go-kart/components/lights && \
  cd ~/go-kart/components/lights && \
  ./flash.sh"
```

**Remote Terminal: Monitor and Test**
```bash
# Set up CAN interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Monitor CAN traffic
candump can0

# In another terminal session:
cd ~/go-kart/dashboard/server
python app.py
```

### Complete Local-Remote Cycle

1. Develop and test locally (Local Terminal 1)
2. Build deployment package (Local Terminal 1)
3. Deploy to Raspberry Pi (Local Terminal 2)
4. Set up and monitor remote device (Remote Terminal)
5. Test functionality (Remote Terminal)
6. Capture logs and results (Remote Terminal)
7. Repeat development cycle (Local Terminal 1)
<!-- LLM_CODE_MARKER_END -->

## Protocol Update Workflow

<!-- LLM_CODE_MARKER: protocol_workflow -->
### Setup

**Terminal 1: Edit Protocol Files**
```bash
cd protocol
# Edit common.proto or component-specific proto files
```

### Development Cycle

**Terminal 1: Generate Code**
```bash
cd protocol
./build.sh
```

**Terminal 2: Update Components**
```bash
# Update component implementations to use new protocol
cd components/lights
# Edit src files to use new protocol definitions
platformio run
```

**Terminal 3: Update Dashboard**
```bash
# Update dashboard protocol registry
cd dashboard/server
# Edit lib/can/protocol_registry.py if needed
python app.py
```

### Complete Protocol Update Cycle

1. Edit protocol definitions (Terminal 1)
2. Generate code for all platforms (Terminal 1)
3. Update component implementations (Terminal 2)
4. Update dashboard protocol registry (Terminal 3)
5. Test component with new protocol (Terminal 2)
6. Test dashboard with new protocol (Terminal 3)
7. Commit changes when working correctly (Terminal 1)
<!-- LLM_CODE_MARKER_END -->

## Integration Testing Workflow

<!-- LLM_CODE_MARKER: integration_workflow -->
### Setup

**Terminal 1: Set Up Test Environment**
```bash
# Start virtual CAN interface
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

### Test Cycle

**Terminal 1: Start Dashboard Server**
```bash
cd dashboard/server
python app.py > dashboard.log 2>&1 &
server_pid=$!
```

**Terminal 2: Simulate Component**
```bash
cd tools/simulator
python simulate_component.py --type=lights > lights.log 2>&1 &
lights_pid=$!
```

**Terminal 3: Monitor CAN Traffic**
```bash
# Monitor CAN messages
candump vcan0 -T 500
```

**Terminal 4: Run Integration Tests**
```bash
cd tests/integration
python test_lights_dashboard.py
```

### Complete Integration Test Cycle

1. Set up test environment (Terminal 1)
2. Start dashboard server (Terminal 1)
3. Start component simulators (Terminal 2)
4. Monitor CAN traffic (Terminal 3)
5. Run integration tests (Terminal 4)
6. Check test results (Terminal 4)
7. Stop all processes when done:
   ```bash
   kill $server_pid $lights_pid
   ```
<!-- LLM_CODE_MARKER_END -->

## Common Multi-Terminal Operations

<!-- LLM_CODE_MARKER: multi_terminal_operations -->
### Build All Components

**Terminal 1: Build Script**
```bash
#!/bin/bash
# build_all.sh
components=("lights" "motors" "batteries" "inputs" "sensors")
success=true

for comp in "${components[@]}"; do
  echo "Building $comp..."
  cd "components/$comp"
  if ! platformio run; then
    echo "ERROR: Failed to build $comp"
    success=false
  fi
  cd ../..
done

if $success; then
  echo "All components built successfully"
  exit 0
else
  echo "Some components failed to build"
  exit 1
fi
```

### Deploy All Components

**Terminal 1: Deploy Script**
```bash
#!/bin/bash
# deploy_all.sh
components=("lights" "motors" "batteries" "inputs" "sensors")
remote_host="pi@raspberrypi.local"
remote_path="/home/pi/go-kart/components"
success=true

# Ensure remote directory exists
ssh "$remote_host" "mkdir -p $remote_path"

for comp in "${components[@]}"; do
  echo "Deploying $comp..."
  
  # Create deployment package
  cd "components/$comp"
  mkdir -p deploy
  cp .pio/build/*/firmware.bin deploy/
  tar -czf "${comp}.tar.gz" deploy
  
  # Copy to remote
  scp "${comp}.tar.gz" "${remote_host}:~/"
  
  # Extract and deploy on remote
  ssh "$remote_host" "mkdir -p $remote_path/$comp && \
    tar -xzf ~/${comp}.tar.gz -C $remote_path/$comp && \
    cd $remote_path/$comp && \
    ./flash.sh || echo 'Failed to flash $comp'"
  
  # Clean up
  rm -rf deploy "${comp}.tar.gz"
  cd ../..
done

echo "Deployment complete"
```
<!-- LLM_CODE_MARKER_END -->

<!-- LLM_TASK_META
id: {task-id}
title: {short-descriptive-title}
priority: {high|medium|low}
estimated_time: {hours or points}
component: {primary-component-path}
dependencies: {comma-separated-dependencies}
-->

# Task: {Title}

## Overview
<!-- LLM_CONTEXT: task_overview -->
A brief, clear description of the task to be completed, focusing on what needs to be accomplished rather than how.
<!-- LLM_CONTEXT_END -->

## Requirements
<!-- LLM_CONTEXT: requirements -->
- Requirement 1
- Requirement 2
- Requirement 3
<!-- LLM_CONTEXT_END -->

## Design
<!-- LLM_CONTEXT: design -->
High-level design considerations and architectural decisions.

**⚠️ DESIGN REVIEW CHECKPOINT**: Before proceeding to implementation, please confirm this design approach.
<!-- LLM_CONTEXT_END -->

## Implementation Plan
<!-- LLM_CONTEXT: implementation_plan -->
1. Step 1
   - Sub-task A
   - Sub-task B
2. Step 2
   - Sub-task A
   - Sub-task B
3. Step 3
   - Sub-task A
   - Sub-task B

**⚠️ IMPLEMENTATION REVIEW CHECKPOINT**: After outlining implementation details but before writing code.
<!-- LLM_CONTEXT_END -->

## Technical Constraints
<!-- LLM_CONTEXT: constraints -->
- Constraint 1
- Constraint 2
- Constraint 3
<!-- LLM_CONTEXT_END -->

## Testing Strategy
<!-- LLM_CONTEXT: testing -->
- Unit tests:
  - Test case 1
  - Test case 2
- Integration tests:
  - Test case 1
  - Test case 2
- Manual verification:
  - Verification step 1
  - Verification step 2
<!-- LLM_CONTEXT_END -->

## Acceptance Criteria
<!-- LLM_CONTEXT: acceptance_criteria -->
- [ ] Criteria 1
- [ ] Criteria 2
- [ ] Criteria 3
<!-- LLM_CONTEXT_END -->

## References
<!-- LLM_CONTEXT: references -->
- [Link to relevant documentation]({doc-path})
- [Link to related code]({code-path})
- [Link to related issue]({issue-link})
<!-- LLM_CONTEXT_END --> 