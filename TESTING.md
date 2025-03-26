# Go-Kart Platform Testing Guide

This document defines standard testing patterns and exit conditions to prevent test loops.

## Test Pattern Structure

<!-- LLM_CODE_MARKER: test_pattern_structure -->
Every test script should follow this structure:

1. **Setup**: Prepare test environment
2. **Execution**: Run the test
3. **Verification**: Check results against expected values
4. **Cleanup**: Reset environment to original state
5. **Reporting**: Output clear success/failure message

Each phase must have a clear completion condition. Never create open-ended test scripts. The script should easily integrate with CI/CD pipelines like github actions
<!-- LLM_CODE_MARKER_END -->

## Exit Conditions

<!-- LLM_CODE_MARKER: exit_conditions -->
All test scripts MUST have explicit exit conditions:

- **Maximum Iterations**: Set a hard limit (usually 3-5) on retry attempts
- **Timeout Limits**: Add absolute timeouts (30-60 seconds max)
- **Clear Success Criteria**: Define exactly what constitutes success
- **Failure Recognition**: Define explicit failure conditions
- **Cleanup Trigger**: Always run cleanup on exit (even after failure)

```python
# Example exit condition pattern
MAX_ATTEMPTS = 3
attempts = 0
success = False

while attempts < MAX_ATTEMPTS and not success:
    attempts += 1
    try:
        # Test code here
        if expected_condition:
            success = True
    except Exception as e:
        print(f"Attempt {attempts} failed: {e}")

# Final verdict
if success:
    print("TEST PASSED")
    exit(0)
else:
    print("TEST FAILED after {attempts} attempts")
    exit(1)
```
<!-- LLM_CODE_MARKER_END -->

## Component Test Protocol

<!-- LLM_CODE_MARKER: component_test_protocol -->
### C++ Component Testing

**Setup:**
```cpp
#include <Arduino.h>
#include <unity.h>
#include "ProtobufCANInterface.h"

// Global test variables
ProtobufCANInterface* canInterface;
bool testPassed = false;
uint8_t testValue = 0;

void setUp(void) {
    // Initialize test environment
    canInterface = new ProtobufCANInterface(0x01);
    canInterface->begin(500000);
    testPassed = false;
    testValue = 0;
}

void tearDown(void) {
    // Clean up test environment
    delete canInterface;
}
```

**Execution:**
```cpp
void test_function() {
    // Run the test with timeout
    unsigned long startTime = millis();
    unsigned long timeout = 1000; // 1 second timeout
    
    // Send test message
    canInterface->sendMessage(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        0x01,
        0x01,
        kart_common_ValueType_UINT8,
        42
    );
    
    // Wait for response with timeout
    while (!testPassed && (millis() - startTime < timeout)) {
        canInterface->process();
        delay(10);
    }
    
    // Verify result
    TEST_ASSERT_EQUAL(42, testValue);
}
```

**Success Pattern:**
- Test assertions pass
- No exceptions thrown
- Execution completes within timeout

**Exit Pattern:**
```cpp
int main() {
    UNITY_BEGIN();
    RUN_TEST(test_function);
    return UNITY_END();
}
```
<!-- LLM_CODE_MARKER_END -->

## Dashboard Test Protocol

<!-- LLM_CODE_MARKER: dashboard_test_protocol -->
### Python Server Testing

**Setup:**
```python
import pytest
import time
from lib.can.interface import CANInterfaceWrapper
from lib.telemetry.store import TelemetryStore

@pytest.fixture
def can_interface():
    # Create a test CAN interface with mock behavior
    telemetry_store = TelemetryStore()
    interface = CANInterfaceWrapper(
        node_id=0x01,
        channel='vcan0',  # Use virtual CAN
        telemetry_store=telemetry_store
    )
    interface.start_processing()
    yield interface
    interface.stop_processing()
```

**Execution:**
```python
def test_send_command(can_interface):
    # Max attempts and timeout
    max_attempts = 3
    timeout = 5.0  # seconds
    success = False
    attempts = 0
    
    while attempts < max_attempts and not success:
        attempts += 1
        start_time = time.time()
        
        # Send test command
        result = can_interface.send_command(
            message_type_name="COMMAND",
            component_type_name="LIGHTS",
            component_name="FRONT",
            command_name="MODE",
            value_name="ON"
        )
        
        # Wait for response with timeout
        while time.time() - start_time < timeout:
            # Check if state was updated
            state = can_interface.telemetry_store.get_component_state("LIGHTS", "FRONT")
            if state and state.get("mode") == "ON":
                success = True
                break
            time.sleep(0.1)
    
    # Final assertion
    assert success, f"Command failed after {attempts} attempts"
```

**Success Pattern:**
- All assertions pass
- Function returns without exceptions
- Test completes within timeout

**Exit Pattern:**
```bash
# Run with explicit timeout
pytest tests/test_can_interface.py -v --timeout=30
```
<!-- LLM_CODE_MARKER_END -->

## Integration Test Protocol

<!-- LLM_CODE_MARKER: integration_test_protocol -->
### Communication Testing

**Setup:**
```python
def setup_integration_test():
    # Set up CAN interfaces
    os.system("sudo ip link set vcan0 type vcan")
    os.system("sudo ip link set up vcan0")
    
    # Start processes
    dashboard_process = subprocess.Popen(
        ["python", "dashboard/server/app.py"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    
    # Wait for startup
    time.sleep(2)
    return dashboard_process
```

**Execution:**
```python
def run_integration_test():
    # Start candump to capture messages
    candump_process = subprocess.Popen(
        ["candump", "vcan0"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )
    
    # Send test message
    os.system("cansend vcan0 123#0100010000")
    
    # Wait for response with timeout
    start_time = time.time()
    timeout = 5.0
    response_received = False
    
    while time.time() - start_time < timeout and not response_received:
        output = candump_process.stdout.readline().decode("utf-8")
        if "vcan0  321" in output:  # Response ID
            response_received = True
    
    # Kill candump process
    candump_process.terminate()
    
    return response_received
```

**Cleanup:**
```python
def cleanup_integration_test(dashboard_process):
    # Terminate processes
    dashboard_process.terminate()
    
    # Wait for termination
    try:
        dashboard_process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        dashboard_process.kill()
    
    # Reset CAN interface
    os.system("sudo ip link set down vcan0")
```

**Complete Test Pattern:**
```python
def test_integration():
    max_attempts = 3
    success = False
    attempts = 0
    
    while attempts < max_attempts and not success:
        attempts += 1
        dashboard_process = setup_integration_test()
        
        try:
            success = run_integration_test()
        finally:
            cleanup_integration_test(dashboard_process)
    
    assert success, f"Integration test failed after {attempts} attempts"
```
<!-- LLM_CODE_MARKER_END -->

## Test Execution Guidelines

<!-- LLM_CODE_MARKER: test_execution_guidelines -->
0. **Build before testing**
   - Dashboard: Use 
   - PlatformIO: package `platformio/tool-renode`

1. **Always use non-interactive mode**:
   - PlatformIO: `pio test -e <environment> --no-color` (should be run from the specific component folder where the ini file lives e.g. components/lights or components/motors )
   - Pytest: `pytest -v --no-header`

2. **Set explicit timeouts**:
   - PlatformIO: `pio test -e <environment> --timeout=30`
   - Pytest: `pytest --timeout=30`

3. **Capture output**:
   - Redirect to file: `platformio test > test_output.log 2>&1`
   - Tee to both console and file: `pytest | tee test_output.log`

4. **Parse results**:
   - Check exit code: `echo $?`
   - Count failures: `grep -c "FAILED" test_output.log`
   - Extract summary: `grep "test summary" test_output.log`

5. **Maximum test iteration count**:
   - Never exceed 3 retry attempts for any test
   - Always implement a circuit breaker pattern
   - Exit with clear status message on failure

6. **Use hardware virtualization when necessary**
   - CAN: Use a combination of the platformio.ini, Renode, and virtual networks to test CAN functionality
   - PlatformIO: package `platformio/tool-renode`
<!-- LLM_CODE_MARKER_END -->

## Troubleshooting Failed Tests

<!-- LLM_CODE_MARKER: troubleshooting_tests -->
### Component Test Failures

1. Check the compilation output:
   ```bash
   cat .pio/build/*/build.log
   ```

2. Verify hardware configuration:
   ```bash
   # Check Config.h constants
   grep "define" include/Config.h
   ```

3. Common CAN interface issues:
   - Baudrate mismatch
   - Missing protocol definitions: You can fix this by running the 
   - Incorrect component/command IDs

### Dashboard Test Failures

1. Check server logs:
   ```bash
   cat dashboard.log
   ```

2. Verify protocol registry:
   ```python
   # In Python test code
   print(protocol_registry.dump_registry())
   ```

3. Test CAN interface directly:
   ```bash
   # Send test message
   cansend vcan0 123#0100010000
   
   # Listen for responses
   candump vcan0
   ```
<!-- LLM_CODE_MARKER_END --> 