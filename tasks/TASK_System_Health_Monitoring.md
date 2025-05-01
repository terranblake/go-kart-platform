<!-- LLM_TASK_META
id: TBD
title: Implement System Health Monitoring & Reporting
priority: medium
estimated_time: 6h
component: protocol, shared/lib/health, tools/health_monitor, components/common/lib/HealthMonitor
dependencies: shared/lib/python/can, components/common/lib/CrossPlatformCAN
-->

# Task: Implement System Health Monitoring & Reporting

## Overview
<!-- LLM_CONTEXT: task_overview -->
Define and implement a system for monitoring and reporting key health and resource utilization metrics for both the Raspberry Pi (vehicle controller) and the primary ESP32 components. This involves:
1.  Defining relevant metrics in the CAN protocol.
2.  Creating shared libraries to gather these metrics on each platform.
3.  Implementing reporting agents/tasks that periodically send these metrics over the CAN bus.

This task is separate from the telemetry data uplink mechanism but uses the same underlying CAN communication.
<!-- LLM_CONTEXT_END -->

## Requirements
<!-- LLM_CONTEXT: requirements -->
- **Protocol Updates:**
    - Ensure `ComponentType`: `SYSTEM_MONITOR` exists.
    - Add `ComponentId`s under `SYSTEM_MONITOR`: `RASPBERRY_PI`, `ESP32_MAIN` (or specific component IDs).
    - Add `CommandId`s under `SYSTEM_MONITOR` for health metrics:
        - Cross-Device: `COMPONENT_UPTIME_S`, `INTERNAL_TEMP_C`, `CPU_LOAD_PERCENT`, `MEMORY_FREE_BYTES`.
        - RPi Specific: `DISK_USAGE_PERCENT`, `NETWORK_SENT_KBPS`, `NETWORK_RECV_KBPS`.
        - ESP32 Specific: `WIFI_RSSI_DBM`, `TASK_STACK_HIGHWATER_BYTES`.
    - Define appropriate `ValueType` for each metric.
    - Update `protocol/build.sh` and regenerate all protocol files.
- **Shared Health Libraries (`shared/lib/health/`):**
    - Create `shared/lib/health/core` (C++?): Potential common logic (e.g., uptime calculation if possible).
    - Create `shared/lib/health/rpi_reporter` (Python): Class/functions using `psutil` to get RPi specific metrics (CPU, Mem, Disk, Net, Temp, Uptime).
    - Create `shared/lib/health/esp32_reporter` (C++): Class/functions using ESP-IDF APIs (`esp_timer_get_time`, `esp_get_free_heap_size`, `esp_temperature_sensor_get_celsius`, WiFi API, `uxTaskGetStackHighWaterMark`) for ESP32 metrics.
- **Reporting Agents:**
    - **RPi (`tools/health_monitor/rpi_monitor.py`):**
        - Standalone Python script.
        - Imports and uses `shared.lib.health.rpi_reporter`.
        - Uses shared `CANInterfaceWrapper`.
        - Periodically (e.g., every 5 seconds) gathers metrics and sends them as individual CAN `STATUS` messages using `SYSTEM_MONITOR`/`RASPBERRY_PI` type/ID and corresponding command IDs.
        - Should be manageable via `systemd` or similar.
    - **ESP32 (`components/common/lib/HealthMonitor`):**
        - New shared PlatformIO library.
        - Provides a class (e.g., `SystemHealthReporter`).
        - Takes a `CrossPlatformCAN` instance.
        - Includes/uses `shared.lib.health.esp32_reporter`.
        - Provides a method (e.g., `reportMetrics()`) to be called periodically from the main `loop()` or a dedicated FreeRTOS task in the ESP32 firmware.
        - Gathers metrics and sends individual CAN `STATUS` messages using `SYSTEM_MONITOR`/`ESP32_MAIN` type/ID.
- **Configuration:**
    - Reporting interval should be configurable for both RPi and ESP32 agents.
- **Testing:**
    - Unit tests for metric gathering functions in shared libraries.
    - Integration tests verifying that agents send correctly formatted CAN messages at the expected frequency.
<!-- LLM_CONTEXT_END -->

## Design Proposal
<!-- LLM_CONTEXT: design -->
1.  **Protocol:**
    - Modify `protocol/common.proto` to add `SYSTEM_MONITOR` if needed.
    - Create/Modify `protocol/system_monitor.proto` defining `SystemMonitorComponentId` (`RASPBERRY_PI`, `ESP32_MAIN`) and `SystemMonitorCommandId` enums for the 9 health metrics defined in requirements.
    - Run `./protocol/build.sh`.
2.  **Shared Libraries (`shared/lib/health/`):**
    - `core`: Minimal C++ or header-only utilities if any truly cross-platform logic applies.
    - `rpi_reporter` (Python): Implement functions wrapping `psutil` calls for each required RPi metric.
    - `esp32_reporter` (C++): Implement functions wrapping ESP-IDF calls for each required ESP32 metric.
3.  **Reporting Agents:**
    - `tools/health_monitor/rpi_monitor.py`: Python script using `schedule` or `threading.Timer` for periodic execution. Imports `rpi_reporter`, `CANInterfaceWrapper`. Gathers metrics, formats CAN messages (`MessageType.STATUS`, `ComponentType.SYSTEM_MONITOR`, `ComponentId.RASPBERRY_PI`, specific `CommandId`, value), sends via CAN interface.
    - `components/common/lib/HealthMonitor`: PlatformIO library. `SystemHealthReporter` class constructor takes `CrossPlatformCAN*`. `reportMetrics()` method calls functions from `esp32_reporter`, formats CAN messages, sends via the CAN interface pointer.

**⚠️ DESIGN REVIEW CHECKPOINT**: Confirm design for health monitoring.
<!-- LLM_CONTEXT_END -->

## Implementation Plan (High-Level)
<!-- LLM_CONTEXT: implementation_plan -->
1.  **Protocol:** Define and generate new protocol messages for health metrics.
2.  **Shared Libs:** Implement metric gathering functions in `rpi_reporter` (Python) and `esp32_reporter` (C++).
3.  **RPi Agent:** Implement the `rpi_monitor.py` script.
4.  **ESP32 Agent:** Implement the `HealthMonitor` PlatformIO library and `SystemHealthReporter` class.
5.  **Integration:** Add calls to `SystemHealthReporter::reportMetrics()` in the main loop/task of relevant ESP32 components.
6.  **Testing:** Unit test metric functions. Integration test CAN message output from both agents.
<!-- LLM_CONTEXT_END -->

## Testing Strategy
<!-- LLM_CONTEXT: testing -->
- **Unit Tests:**
    - Test individual metric gathering functions in `rpi_reporter` and `esp32_reporter` (may require mocking underlying system calls/APIs).
- **Integration Tests:**
    - Run `rpi_monitor.py` on an RPi (or simulated env with `psutil`), connect to `vcan0`, use `candump` to verify correctly formatted `SYSTEM_MONITOR`/`RASPBERRY_PI` status messages appear periodically.
    - Compile and run ESP32 firmware including the `HealthMonitor` library on hardware or Renode simulation, connect to `vcan0` or physical CAN, use `candump` to verify `SYSTEM_MONITOR`/`ESP32_MAIN` status messages appear periodically.
<!-- LLM_CONTEXT_END -->

## Acceptance Criteria
<!-- LLM_CONTEXT: acceptance_criteria -->
- [ ] Protocol files are updated and generated correctly for `SYSTEM_MONITOR` and health metric command IDs.
- [ ] RPi health metrics (CPU, Mem, Disk, Net, Temp, Uptime) are reported correctly over CAN as individual `STATUS` messages.
- [ ] ESP32 health metrics (CPU Load est., Heap, Temp, RSSI, Uptime, Stack HWM) are reported correctly over CAN as individual `STATUS` messages.
- [ ] Reporting agents run periodically at configurable intervals.
- [ ] Shared libraries correctly gather metrics on their respective platforms.
- [ ] All relevant unit and integration tests pass.
<!-- LLM_CONTEXT_END -->

## References
<!-- LLM_CONTEXT: references -->
- [Protocol Definitions](protocol/)
- [Shared Python Libs](shared/lib/python/)
- [Shared C++ Libs](components/common/lib/)
- [psutil Documentation](https://psutil.readthedocs.io/en/latest/)
- [ESP-IDF System APIs](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/index.html)
- [PlatformIO Documentation](https://docs.platformio.org/en/latest/)
<!-- LLM_CONTEXT_END --> 