<!-- LLM_TASK_META
id: sensors-001
title: Implement Generic Sensor Component
priority: high
estimated_time: 4 hours
component: components/sensors
dependencies: motors-001
-->

# Task: Implement Generic Sensor Component

## Overview
<!-- LLM_CONTEXT: task_overview -->
Create a generic sensor framework that can be used to handle various sensor types (temperature, RPM, voltage, etc.) across different components of the go-kart. This framework should be lightweight enough to run on Arduino Nano but also support more capable platforms like ESP8266/ESP32. It should integrate with the CAN bus system to send sensor readings to the rest of the platform.
<!-- LLM_CONTEXT_END -->

## Requirements
<!-- LLM_CONTEXT: requirements -->
- Create a base `Sensor` class that can represent any type of sensor
- Implement a `SensorRegistry` to manage multiple sensors on a single component
- Develop specific sensor implementations for temperature sensors and RPM sensors
- Create a `SensorManager` to handle static access and interrupt routing for RPM sensors
- Integration with the existing CrossPlatformCAN library for transmitting sensor data
- Demonstrate integration with the existing motor controller test code
- Support both ESP8266 and Arduino Nano platforms
<!-- LLM_CONTEXT_END -->

## Design
<!-- LLM_CONTEXT: design -->
The sensor framework will consist of the following key components:

1. **Base Sensor Class**: A lightweight abstract class that defines the core functionality all sensors should implement
   - Reading values at configurable intervals
   - Converting raw values to appropriate units
   - Providing data in a format suitable for CAN transmission

2. **SensorRegistry**: A collection manager that handles:
   - Registration of multiple sensors
   - Periodic processing of all registered sensors
   - Integration with CAN to broadcast sensor readings

3. **SensorManager**: A static manager that:
   - Provides global access to critical sensors that need interrupt handling
   - Routes interrupt calls to appropriate sensor objects
   - Centralizes ISR management to prevent duplicate interrupts

4. **Specific Sensor Implementations**:
   - Temperature sensors using NTC thermistors
   - RPM sensors using hall effect sensors
   - Framework extensible to other sensor types

The design will use the CrossPlatformCAN message format, repurposing existing fields to efficiently represent sensor data:
- Message Type: DATA (1)
- Component Type: The component the sensor belongs to (e.g., MOTORS)
- Component ID: The specific instance ID
- Command ID: Repurposed as Sensor ID within the component
- Value Type: The data type of the sensor reading
- Value: The actual sensor reading

<!-- LLM_CONTEXT_END -->

## Implementation Plan
<!-- LLM_CONTEXT: implementation_plan -->
1. Create the base sensor framework files
   - Create `components/sensors/src/Sensor.h` with the base sensor class
   - Create `components/sensors/src/SensorRegistry.h` to manage sensors
   - Create `components/sensors/src/SensorManager.h` for static access and interrupt routing

2. Implement specific sensor types
   - Create `components/sensors/variants/TemperatureSensor.h` for temperature monitoring
   - Create `components/sensors/variants/RpmSensor.h` for RPM monitoring

3. Update the BasicControllerTest to use sensor framework
   - Replace direct temperature readings with TemperatureSensor instances
   - Replace direct RPM calculations with RpmSensor
   - Update ISR functions to use SensorManager
   - Add mock CAN interface for testing

4. Test and validate
   - Test on ESP8266 platform
   - Ensure compatibility with Arduino Nano
   - Verify correct readings are obtained
   - Validate CAN message transmission

<!-- LLM_CONTEXT_END -->

## Technical Constraints
<!-- LLM_CONTEXT: constraints -->
- Memory footprint must be small enough to run on Arduino Nano (2KB RAM)
- Avoid dynamic memory allocation where possible for embedded targets
- Interrupt handlers must be efficient to prevent timing issues
- Must work with the existing CrossPlatformCAN message format
- Temperature sensor implementation must work with NTCLE100E3203JBD thermistors
- RPM sensor implementation must work with hall effect sensors on BLDC motors
<!-- LLM_CONTEXT_END -->

## Testing Strategy
<!-- LLM_CONTEXT: testing -->
- Unit tests:
  - Test temperature calculation accuracy with known resistance values
  - Test RPM calculation with simulated pulse inputs
  - Test SensorRegistry with mock sensors

- Integration tests:
  - Test SensorManager interrupt routing with multiple RPM sensors
  - Test all sensors updating and transmitting via CAN

- Manual verification:
  - Run BasicControllerTest with updated sensor framework
  - Verify temperature readings match expected values for the thermistors
  - Verify RPM readings match expected values for the motor
  - Confirm CAN messages are correctly formatted
<!-- LLM_CONTEXT_END -->

## Acceptance Criteria
<!-- LLM_CONTEXT: acceptance_criteria -->
- [ ] Base Sensor class implemented and working
- [ ] SensorRegistry correctly manages multiple sensors
- [ ] SensorManager successfully routes interrupts
- [ ] TemperatureSensor accurately reads from thermistors
- [ ] RpmSensor accurately calculates motor RPM
- [ ] BasicControllerTest integrated with sensor framework
- [ ] All sensors correctly transmit data over the CAN bus
- [ ] Framework works on both ESP8266 and Arduino Nano platforms
<!-- LLM_CONTEXT_END -->

## References
<!-- LLM_CONTEXT: references -->
- [CrossPlatformCAN Documentation](components/common/lib/CrossPlatformCAN/README.md)
- [BasicControllerTest](components/motors/test/BasicControllerTest.cpp)
- [Motor Implementation Task](tasks/TASK_implement_motors_component.md)
<!-- LLM_CONTEXT_END --> 