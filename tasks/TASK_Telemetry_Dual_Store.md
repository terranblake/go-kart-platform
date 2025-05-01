<!-- LLM_TASK_META
id: TBD
title: Implement Dual-Mode Telemetry Collector with Streaming Uplink
priority: high
estimated_time: 8h
component: telemetry/collector, protocol
dependencies: shared/lib/python/can, shared/lib/python/telemetry, tasks/TASK_TELEMETRY_COLLECTOR.md
-->

# Task: Implement Dual-Mode Telemetry Collector with Streaming Uplink

## Overview
<!-- LLM_CONTEXT: task_overview -->
Evolve the existing Telemetry Collector Service (`tasks/TASK_TELEMETRY_COLLECTOR.md`) to operate in a dual mode using a unified codebase:
1.  **Vehicle Role:** Maintains a persistent local store (SQLite). Provides an API for the Dashboard to query recent data. Establishes a persistent streaming connection (e.g., WebSocket) to a remote collector instance and continuously sends historical data.
2.  **Remote Role:** Runs the same codebase. Listens for incoming streaming connections from vehicles. Receives telemetry data and stores it persistently.

This task focuses on the collector refactoring, the streaming uplink mechanism, and the necessary protocol updates for uplink status reporting.
<!-- LLM_CONTEXT_END -->

## Requirements
<!-- LLM_CONTEXT: requirements -->
- **Protocol Updates (Uplink Status Only):**
    - Add a new `ComponentType`: `SYSTEM_MONITOR` (if not already present).
    - Add a new `ComponentId` under `SYSTEM_MONITOR`: `UPLINK_MANAGER`.
    - Add new `CommandId`s under `SYSTEM_MONITOR` / `UPLINK_MANAGER` for uplink status reporting (e.g., Status Enum [Connecting, Connected, Disconnected, Error], Queue Size, Avg Latency, Avg Throughput).
    - Update `protocol/build.sh` and regenerate all protocol files.
- **Unified Collector (`telemetry/collector`):**
    - Refactor `PersistentTelemetryStore`:
        - Ensure it supports configuration for different DB paths (`DB_PATH`).
        - Method for dashboard API to query recent data (last `DASHBOARD_RETENTION_S` seconds) when `ROLE=vehicle`.
        - Add flag/column (`uplinked BOOLEAN DEFAULT 0`) to track upload status.
        - Method to fetch *un-uplinked* records sequentially for streaming.
        - Method to mark records as uploaded after successful transmission acknowledgement.
        - Pruning logic: Remove successfully uploaded records, respecting `MAX_LOCAL_RECORDS` safety cap.
    - Implement `UplinkManager` (runs if `ROLE=vehicle`):
        - Uses a persistent streaming protocol (e.g., WebSockets via `websockets` library) to connect to `REMOTE_ENDPOINT_URL`.
        - Manages connection state (connecting, connected, disconnected, error) and handles retries with backoff.
        - Continuously queries the store for un-uplinked records and sends them over the stream.
        - Requires an acknowledgement mechanism from the remote end to confirm receipt before marking records as `uplinked=1` locally.
        - Implements buffering logic: If disconnected, continue storing locally. When reconnected, resume streaming from the last acknowledged point.
        - Reports its status and statistics (Queue Size, Latency, Throughput based on send/ack timings) via `SYSTEM_MONITOR`/`UPLINK_MANAGER` CAN messages.
    - Refactor `collector.py`:
        - Load unified config (`ROLE`, `DB_PATH`, `REMOTE_ENDPOINT_URL`, etc.).
        - Conditionally initialize/start CAN listener (`ENABLE_CAN_LISTENER`).
        - Conditionally initialize/start `UplinkManager` thread (`ENABLE_UPLINK_MANAGER`).
    - Refactor `api.py`:
        - Adjust dashboard history endpoint for `ROLE=vehicle`.
        - Add WebSocket endpoint (`/ws/telemetry/upload`) (active if `ROLE=remote`) to accept incoming connections from vehicles.
            - Handles multiple concurrent vehicle connections.
            - Receives streamed telemetry records.
            - Sends acknowledgements back to the vehicle upon successful storage.
            - Uses the collector's `PersistentTelemetryStore` instance to save records to the *remote* DB.
- **Configuration:**
    - Unified config: `ROLE` (`vehicle`/`remote`), `DB_PATH`, `REMOTE_ENDPOINT_URL` (vehicle only, format e.g., `ws://host:port/ws/telemetry/upload`), `ENABLE_CAN_LISTENER` (vehicle only), `ENABLE_UPLINK_MANAGER` (vehicle only), `DASHBOARD_RETENTION_S` (vehicle only), `MAX_LOCAL_RECORDS`, API key/auth mechanism for WebSocket (optional).
- **Testing:**
    - Unit tests for `UplinkManager` logic (connection state, buffering, ack handling, stats calculation).
    - Integration tests simulating vehicle collector and remote collector interaction over a WebSocket, including connection drops/reconnects. **Uplink tests must interact with a real WebSocket endpoint** (e.g., a local test server spun up during the test).
<!-- LLM_CONTEXT_END -->

## Design Proposal
<!-- LLM_CONTEXT: design -->
1.  **Protocol (Uplink Status):**
    - Modify `protocol/common.proto` to add `SYSTEM_MONITOR` if needed.
    - Create/Modify `protocol/system_monitor.proto`: Define `SystemMonitorComponentId` including `UPLINK_MANAGER`. Define `SystemMonitorCommandId` for uplink status (e.g., `UPLINK_STATUS`, `UPLINK_QUEUE_SIZE`, `UPLINK_AVG_LATENCY_MS`, `UPLINK_AVG_THROUGHPUT_KBPS`). Define `UplinkStatusValue` enum.
    - Run `./protocol/build.sh`.
2.  **Unified Collector (`telemetry/collector`):**
    - Refactor `PersistentTelemetryStore` as described in Requirements (add `uplinked` column, methods for sequential un-uplinked fetching, marking uploaded).
    - Create `uplink_manager.py`:
        - `UplinkManager` class (threading.Thread).
        - Uses `websockets` library for client connection management.
        - `run()` method: Maintains connection loop, state machine (Disconnected -> Connecting -> Connected -> Error). In `Connected` state, queries store for records, sends via WebSocket (e.g., JSON encoded), waits for ack from server. Handles connection errors, implements backoff.
        - Needs logic to track last ack'd record timestamp/ID to resume correctly.
        - Periodically sends `SYSTEM_MONITOR`/`UPLINK_MANAGER` status CAN messages.
    - Modify `collector.py`:
        - Load unified config.
        - Conditional startup of CAN listener and `UplinkManager` based on `ROLE`.
    - Modify `api.py`:
        - Adapt dashboard history endpoint.
        - If `ROLE=remote`, start a WebSocket server (using `websockets` library) on a configured path (e.g., `/ws/telemetry/upload`).
        - WebSocket server handler: Accepts connections, receives messages (records), saves to remote `PersistentTelemetryStore`, sends acknowledgements back to the client.
3.  **Health Reporting Agents:** (Moved to `TASK_System_Health_Monitoring.md`)

**⚠️ DESIGN REVIEW CHECKPOINT**: Please confirm this revised design for Task 1 (Streaming Uplink) before proceeding.
<!-- LLM_CONTEXT_END -->

## Implementation Plan (High-Level)
<!-- LLM_CONTEXT: implementation_plan -->
1.  **Protocol (Uplink Status):** Define and generate protocol messages for uplink status.
2.  **Collector Refactor (Store):** Modify `PersistentTelemetryStore` (DB schema, fetch/mark methods).
3.  **Collector Refactor (Remote API):** Implement WebSocket server endpoint in `api.py` for `ROLE=remote`.
4.  **Collector Refactor (Uplink Manager):** Implement `UplinkManager` using WebSockets client, including connection logic, streaming, ack handling, and buffering.
5.  **Collector Refactor (Main):** Implement unified config loading and conditional component initialization in `collector.py`.
6.  **Integration & Testing:** Test vehicle->remote streaming, ack mechanism, disconnect/reconnect scenarios using a real WebSocket test server.
<!-- LLM_CONTEXT_END -->

## Testing Strategy
<!-- LLM_CONTEXT: testing -->
- **Unit Tests:**
    - `PersistentTelemetryStore` methods (DB interactions, marking, pruning, fetching un-uplinked).
    - `UplinkManager` state machine logic, buffering logic (mock WebSocket connection for state tests if needed, but not for interaction tests).
- **Integration Tests:**
    - **Uplink Process:** Start a test WebSocket server. Run the vehicle collector with `UplinkManager`. Verify connection, data streaming, acknowledgements. Verify local DB records are marked `uplinked`. Simulate server disconnects/reconnects and verify `UplinkManager` handles buffering and resumes correctly. Verify `UPLINK_MANAGER` status CAN messages.
    - **Remote Receiver:** Send test data via WebSocket client to the remote collector's endpoint. Verify data is stored correctly in the remote DB and acknowledgements are sent.
<!-- LLM_CONTEXT_END -->

## Acceptance Criteria
<!-- LLM_CONTEXT: acceptance_criteria -->
- [ ] Protocol files are updated and generated correctly for `UPLINK_MANAGER` status reporting.
- [ ] Local collector (`ROLE=vehicle`) stores all telemetry in SQLite.
- [ ] Local collector API provides data within the configured recent time window.
- [ ] Local collector (`ROLE=vehicle`) establishes a persistent WebSocket connection to the remote endpoint.
- [ ] Un-uplinked data is streamed from the vehicle to the remote collector via WebSocket.
- [ ] Remote collector (`ROLE=remote`) receives streamed data via WebSocket endpoint and stores it.
- [ ] An acknowledgement mechanism ensures data is marked `uplinked` locally only after remote storage confirmation.
- [ ] Uplink status (Connected, Disconnected, Queue Size, Latency, Throughput) is reported over CAN by the vehicle collector.
- [ ] Data persists locally if the remote endpoint is unavailable.
- [ ] Uplink resumes streaming from the last acknowledged point when the remote endpoint becomes available again.
- [ ] Local database prunes successfully uploaded data, staying below `MAX_LOCAL_RECORDS` safety cap.
- [ ] All relevant unit and integration tests pass, with uplink tests hitting a real WebSocket endpoint.
<!-- LLM_CONTEXT_END -->

## References
<!-- LLM_CONTEXT: references -->
- [Original Telemetry Task](tasks/TASK_TELEMETRY_COLLECTOR.md)
- [Protocol Definitions](protocol/)
- [Shared Python Libs](shared/lib/python/)
- [Python websockets library](https://websockets.readthedocs.io/en/stable/)
- [Python threading](https://docs.python.org/3/library/threading.html)
<!-- LLM_CONTEXT_END --> 