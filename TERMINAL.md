# Terminal Operations Guide

This guide provides structured patterns for terminal operations when working with the Go-Kart Platform.

## Non-Interactive Command Patterns

<!-- LLM_CODE_MARKER: non_interactive_commands -->
Always use these patterns when running commands that might prompt for input:

- APT package installation: `sudo apt-get install -y [package]`
- Python package installation: `pip install --no-input [package]`
- File removals: `rm -f [file]` (never use interactive `-i` flag)
- Directory creation: `mkdir -p [directory]` (create parents as needed)
- Directory removals: `rm -rf [directory]` (use with caution, never on /*)
- Git operations: `git checkout -f [branch]` or `git reset --hard`
<!-- LLM_CODE_MARKER_END -->

## Background Process Management

<!-- LLM_CODE_MARKER: background_processes -->
### Starting Background Processes
```bash
# Method 1: Simplest approach
python app.py &

# Method 2: More robust, with output capture
nohup python app.py > app.log 2>&1 &
pid=$!
echo $pid > app.pid
```

### Checking Background Processes
```bash
# Check if process is running
ps aux | grep "[p]ython app.py"  # The [p] prevents matching grep itself

# Check process status by PID
if [ -f app.pid ] && kill -0 $(cat app.pid) 2>/dev/null; then
    echo "App is running"
else
    echo "App is not running"
fi

# View process output
tail -f app.log
```

### Stopping Background Processes
```bash
# Method 1: By pattern
pkill -f "python app.py"

# Method 2: By PID file
if [ -f app.pid ]; then
    kill $(cat app.pid)
    rm app.pid
fi
```
<!-- LLM_CODE_MARKER_END -->

## Multi-Terminal Workflows

<!-- LLM_CODE_MARKER: multi_terminal_workflows -->
### Local Development Workflow
Terminal 1 (Local Development):
```bash
cd components/lights
# Edit files
pio test
```

Terminal 2 (Local CAN Monitoring):
```bash
# Monitor CAN traffic
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0
candump can0
```

Terminal 3 (Dashboard Server):
```bash
cd dashboard/server
python app.py
```

### Remote Deployment Workflow
Terminal 1 (Local Build):
```bash
cd components/lights
pio run -t upload
pio device monitor -b 115200
```

Terminal 2 (Remote Deployment):
```bash
# ensure your changes are based off of the main repository
# changes should be commit to a branch `git commit -m 'semantic commit message'`
# changes should be pushed to a branch `git push -u origin terran/some-branch-name`

ssh pi@192.168.1.154
git checkout -b terran/some-descriptive-branch-name # if on some other branch
git pull
```
<!-- LLM_CODE_MARKER_END -->

## Script Execution Patterns

<!-- LLM_CODE_MARKER: script_execution -->
### Making Scripts Executable
```bash
chmod +x script.sh
```

### Running Build Scripts
```bash
# Example: Build the protocol definitions to protocol/generated/
./protocol/build.sh

# Example: Compile the lights component code
cd components/lights && pio run -e debug

# Example: Build the dashboard/server (includes)
cd dashboard/server && python3 setup.py
```

### Running Deploy Scripts
```bash
# Example: Deploy to remote


# Example: Deploy with specific options
REMOTE_USER=pi REMOTE_PATH=/opt/go-kart ./deploy_to_pi.sh
```
<!-- LLM_CODE_MARKER_END -->

## Process Monitoring and Logging

<!-- LLM_CODE_MARKER: process_monitoring -->
### Dashboard Server
```bash
# Start with logging
python dashboard/server/app.py > dashboard.log 2>&1 &

# Monitor logs
tail -f dashboard.log

# Extract errors only
grep ERROR dashboard.log
```

### CAN Monitoring
```bash
# Basic CAN monitoring
candump can0

# Filter specific messages (e.g., only lights component)
candump can0,123:7FF

# Logging to file with timestamps
candump -L can0 > can_log.txt

# Send test message
cansend can0 123#DEADBEEF
```
<!-- LLM_CODE_MARKER_END -->

## Troubleshooting Commands

<!-- LLM_CODE_MARKER: troubleshooting -->
### Network Issues
```bash
# Check network interfaces
ip addr

# Test connectivity
ping -c 4 192.168.1.100

# Check CAN interface status
ip -details link show can0
```

### Process Issues
```bash
# Find process using a specific port
lsof -i :5000

# Check memory usage
free -h

# Check disk space
df -h

# Check CPU usage
top -b -n 1 | head -20
```

### Log Analysis
```bash
# Find errors in logs
grep -i error *.log

# View recent system logs
journalctl -n 50

# Check for CAN errors
ip -s link show can0
```
<!-- LLM_CODE_MARKER_END -->

## Common Operation Sequences

<!-- LLM_CODE_MARKER: operation_sequences -->
### Complete Dashboard Setup
```bash
# Install dependencies
sudo apt-get update
sudo apt-get install -y python3-pip can-utils

# Set up CAN interface
sudo ip link set can0 type can bitrate 500000
sudo ip link set up can0

# Install Python dependencies
cd dashboard/server
pip install -r requirements.txt

# Start the server
python app.py &
```

### Complete Component Build and Test
```bash
# Install PlatformIO (if needed)
pip install platformio

# Build component
cd components/lights
platformio run

# Run tests
platformio test

# Upload firmware
platformio run -t upload
```
<!-- LLM_CODE_MARKER_END --> 