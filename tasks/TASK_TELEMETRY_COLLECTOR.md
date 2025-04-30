<!-- LLM_TASK_META
id: TBD
title: Implement Telemetry Collector Service
priority: high
estimated_time: 4h
component: telemetry/collector
dependencies: shared/lib/python/can, shared/lib/python/telemetry
-->

# Task: Implement Telemetry Collector Service

## Overview
<!-- LLM_CONTEXT: task_overview -->
Implement a standalone Python service that listens to the CAN bus, aggregates telemetry data (current state and history), and provides an API for other services (like the Dashboard and future 'Main Brain') to access this data. This decouples the Dashboard from direct CAN access for telemetry viewing.
<!-- LLM_CONTEXT_END -->

## Requirements
<!-- LLM_CONTEXT: requirements -->
- Run as a separate, long-running process.
- Use the shared `CANInterfaceWrapper` to connect to the CAN bus (e.g., `can0` or `vcan0`).
- Listen for and process incoming CAN messages, specifically `STATUS` messages.
- Maintain the latest state of all known components based on received messages.
- Store a limited history of received state updates (e.g., last 1000 messages or last 5 minutes).
- Provide an HTTP API (using Flask or FastAPI) for querying data:
    - `/api/state/current`: Returns the latest known state of all components.
    - `/api/state/history`: Returns the stored history of state updates.
    - `/api/state/component/{component_type}/{component_id}`: Returns the latest state for a specific component instance.
- Use the shared `TelemetryStore` or an extended version for state management.
- Persist historical data locally (e.g., using SQLite).
- Be configurable (CAN channel, baud rate, API port, logging level).
- Include basic logging for diagnostics.
- Include `requirements.txt`.
<!-- LLM_CONTEXT_END -->

## Design
<!-- LLM_CONTEXT: design -->
1.  **Core Process (`collector.py`):**
    - Initializes shared `ProtocolRegistry`.
    - Initializes a dedicated `TelemetryStore` (potentially extended for persistence in `storage.py`).
    - Initializes shared `CANInterfaceWrapper`, configured to listen on the specified CAN channel.
    - Registers a general handler with `CANInterfaceWrapper` to receive all relevant `STATUS` messages (or potentially all messages).
    - The handler updates the `TelemetryStore` with received data.
    - Starts the `CANInterfaceWrapper`'s processing thread.
    - Starts a separate web server (Flask/FastAPI) for the API endpoints.
2.  **Storage (`storage.py`):**
    - Extends or wraps `TelemetryStore`.
    - Adds methods to initialize/connect to an SQLite database.
    - Modifies `update_state` to write to the database (history table).
    - Adds methods to load recent history from the database on startup (optional).
    - Provides methods needed by the API to retrieve current state and history (possibly querying the DB for history).
3.  **API (`api.py`):**
    - Uses Flask or FastAPI framework.
    - Defines routes specified in Requirements.
    - Interacts with the `TelemetryStore`/`Storage` instance to fetch data.
    - Returns data in JSON format.
4.  **Configuration:**
    - Use environment variables or a simple config file (`config.ini` or `config.yaml`) for settings like CAN interface, baud rate, API port, DB path, log level.

**⚠️ DESIGN REVIEW CHECKPOINT**: Before proceeding to implementation, please confirm this design approach.
<!-- LLM_CONTEXT_END -->

## Implementation Plan
<!-- LLM_CONTEXT: implementation_plan -->
1.  **Setup:**
    - Create directory structure: `telemetry/collector/`.
    - Create initial files: `collector.py`, `api.py`, `storage.py`, `requirements.txt`, `config.ini` (example).
2.  **Storage Implementation (`storage.py`):**
    - Define SQLite schema (e.g., `telemetry_history` table).
    - Implement class (e.g., `PersistentTelemetryStore`) inheriting/wrapping `TelemetryStore`.
    - Add DB connection/initialization logic.
    - Implement `update_state` to write to DB.
    - Implement methods to query current state (from memory) and history (from DB).
3.  **Collector Logic (`collector.py`):**
    - Implement configuration loading.
    - Initialize `ProtocolRegistry`, `PersistentTelemetryStore`, `CANInterfaceWrapper`.
    - Define and register the CAN message handler function (`_handle_message`).
    - Start CAN processing (`can_interface.start_processing()`).
    - Import and run the API server from `api.py`.
    - Add signal handling for graceful shutdown.
4.  **API Implementation (`api.py`):**
    - Set up Flask/FastAPI app.
    - Create endpoints (`/api/state/current`, `/api/state/history`, etc.).
    - Implement endpoint logic to call methods on the `PersistentTelemetryStore` instance passed from `collector.py`.
5.  **Requirements (`requirements.txt`):**
    - Add necessary libraries (e.g., `Flask`, `Flask-SocketIO`, `cffi`, `pysqlite3` if needed, etc.). Ensure versions align with Dashboard if necessary.
6.  **Testing:**
    - Unit tests for `PersistentTelemetryStore` (mocking DB interactions).
    - Integration tests using `vcan0`, sending mock CAN status messages, and verifying API responses.

**⚠️ IMPLEMENTATION REVIEW CHECKPOINT**: After outlining implementation details but before writing code.
<!-- LLM_CONTEXT_END -->

## Technical Constraints
<!-- LLM_CONTEXT: constraints -->
- Must use the shared CAN/Telemetry libraries (`shared/lib/python/*`).
- Must run on Raspberry Pi (Linux).
- Database size should be managed (e.g., capped history).
- API should be non-blocking if possible (especially if using FastAPI).
- Resource usage (CPU/memory) should be monitored.
<!-- LLM_CONTEXT_END -->

## Testing Strategy
<!-- LLM_CONTEXT: testing -->
- **Unit Tests:**
    - `storage.py`: Test database interactions (insert, query), state updates, history capping.
- **Integration Tests:**
    - Set up `vcan0`.
    - Run the collector service.
    - Use `cansend` to send simulated `STATUS` messages.
    - Query the API endpoints (`curl` or `pytest` HTTP client) to verify:
        - Current state is updated correctly.
        - History reflects sent messages.
        - Specific component state retrieval works.
- **Manual Verification:**
    - Run collector alongside the actual dashboard (modified to use the collector's API).
    - Observe real-time updates in the dashboard UI fed from the collector.
    - Check log output (`collector.log`).
<!-- LLM_CONTEXT_END -->

## Acceptance Criteria
<!-- LLM_CONTEXT: acceptance_criteria -->
- [ ] Collector service runs without errors.
- [ ] Service connects to the specified CAN interface.
- [ ] Incoming CAN `STATUS` messages update the internal state.
- [ ] State history is recorded in the SQLite database.
- [ ] API endpoints (`/current`, `/history`, `/component/...`) return correct JSON data.
- [ ] Dashboard (when updated) can successfully retrieve and display data from the collector's API.
- [ ] Tests pass (unit and integration).
- [ ] Code follows Python standards (PEP8, typing).
<!-- LLM_CONTEXT_END -->

## References
<!-- LLM_CONTEXT: references -->
- [Shared CAN Library](shared/lib/python/can/interface.py)
- [Shared Telemetry Library](shared/lib/python/telemetry/store.py)
- [Protocol Definitions](protocol/)
- [Flask Documentation](https://flask.palletsprojects.com/)
- [SQLite Python](https://docs.python.org/3/library/sqlite3.html)
<!-- LLM_CONTEXT_END --> 