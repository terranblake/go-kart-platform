# Animation Pipeline Design Document (Revised)

<!-- LLM_CONTEXT: animation_pipeline_overview -->
## Overview

This document outlines the design for integrating a real-time animation pipeline for LED control into the Go-Kart Platform. It replaces any previous design proposals for this task.

The core approach is a **hybrid protocol model**:
1.  Utilize the existing Protobuf-based `KartMessage` structure for control and configuration messages (start/end stream, set config).
2.  Introduce raw CAN frames with dedicated CAN IDs for efficient, high-throughput transmission of animation data chunks.
3.  Modify the `CrossPlatformCAN` library (C++ and Python) to support both Protobuf and raw CAN message handling.
4.  Implement server-side logic for chunking animation data and orchestrating the transmission sequence.
5.  Implement Arduino-side logic for reassembling raw data chunks into complete animation frames using a memory-efficient buffering strategy.

This design aims to meet performance targets (e.g., 30 FPS) while respecting hardware constraints (CAN bandwidth, Arduino RAM/Flash).
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: hybrid_protocol_design -->
## Hybrid Protocol Design

### 1. Control Messages (Protobuf `KartMessage`)

- **Structure:** Uses the standard `kart.common.KartMessage` defined in `common.proto`.
- **Target:** `ComponentType = LIGHTS`.
- **New Command IDs (in `lights.proto`):**
    ```protobuf
    // Animation Stream Commands
    ANIMATION_STREAM_START = 10; // Value: uint24 total_bytes (Total size of the animation frame)
    ANIMATION_STREAM_END = 11;   // Value: uint24 checksum/status (Optional metadata, TBD)
    ANIMATION_SET_CONFIG = 12; // Value: Encoded config (e.g., buffer size, target FPS, TBD)
    ```
- **Purpose:** Initiate streams, signal completion, configure receiver behavior.

### 2. Raw Data Frames (Raw CAN)

- **CAN ID Allocation:**
    - A dedicated range `0x700 - 0x70F` (standard 11-bit IDs) is reserved for raw animation data.
    - **Initial Implementation:** Use `0x700` for all raw animation data chunks.
    - These IDs are *not* processed by the Protobuf layer.
- **Frame Format (8 bytes):**
    ```
    | Byte 0         | Bytes 1-7        |
    |----------------|------------------|
    | Sequence Number| Data Payload     |
    | (0-255)        | (7 bytes)        |
    ```
- **Purpose:** Efficiently transmit chunks of animation frame data (e.g., packed RGB values).

### 3. Transmission Workflow

1.  **Initiation:** Server sends `ANIMATION_STREAM_START` (`KartMessage`, Protobuf) to target Light Component ID(s). The `value` field contains the total expected bytes for the frame.
2.  **Data Transfer:** Server chunks the full animation frame data into 7-byte payloads. For each chunk, it sends a raw CAN frame using ID `0x700`, with the incrementing sequence number in Byte 0 and the payload in Bytes 1-7.
3.  **Completion:** Server sends `ANIMATION_STREAM_END` (`KartMessage`, Protobuf) to target Light Component ID(s).

<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: crossplatformcan_modifications -->
## CrossPlatformCAN Library Modifications

The core `CANInterface` (raw CAN) remains unchanged. Modifications target `ProtobufCANInterface` (C++) and its Python wrapper.

### C++ Layer (`ProtobufCANInterface.h/.cpp`)

- **Raw Sending:**
    - Add public method: `bool sendRawMessage(uint32_t can_id, const uint8_t* data, uint8_t length);` (Calls `m_canInterface.sendMessage`).
    - Add C API function: `bool can_interface_send_raw_message(can_interface_t handle, uint32_t can_id, const uint8_t* data, uint8_t length);`
- **Raw Receiving:**
    - Add handler type: `typedef void (*RawMessageHandler)(uint32_t can_id, const uint8_t* data, uint8_t length);`
    - Add registration: `void registerRawHandler(uint32_t can_id, RawMessageHandler handler);` (Stores handlers separately, keyed by raw `can_id`).
    - Add C API function: `void can_interface_register_raw_handler(can_interface_t handle, uint32_t can_id, void (*handler)(uint32_t, const uint8_t*, uint8_t));`
- **Processing Logic (`process()` method):**
    - On receiving a raw `CANMessage`:
        1. Check if the `can_id` matches a registered `RawMessageHandler`.
        2. If yes, call the raw handler and stop processing this message.
        3. If no, proceed with existing Protobuf decoding and `MessageHandler` dispatch.

### Python Wrapper (`dashboard/server/lib/can/interface.py`)

- **Update CFFI (`ffi.cdef`):** Add definitions for the new C API functions (`can_interface_send_raw_message`, `can_interface_register_raw_handler`).
- **Raw Sending:**
    - Add method: `send_raw_message(self, can_id, data: bytes)` (Calls C API via FFI).
- **Raw Receiving:**
    - Add method: `register_raw_handler(self, can_id, handler: Callable[[int, bytes], None])`
    - Creates CFFI callback, stores reference, calls C API via FFI.
    - The internal Python callback unpacks C data (`ffi.unpack`) before calling the user's Python handler.

<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: server_implementation -->
## Server-side Implementation (`dashboard/server/`)

- **CAN Interaction:** Use `CANInterfaceWrapper`.
    - Call `send_command` for `ANIMATION_STREAM_START` / `ANIMATION_STREAM_END`.
    - Call `send_raw_message` for data chunks using ID `0x700`.
- **Animation Logic (e.g., `AnimationManager` class):**
    - Receive animation data/commands from frontend (via WebSocket/REST).
    - Chunk animation frame data into 7-byte payloads.
    - Manage stream state (sequence numbers, target component ID).
    - Orchestrate the Start -> Data -> End transmission sequence.
- **API:** Expose WebSocket/REST endpoints for frontend control (load, play, stop animations).

<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: arduino_implementation -->
## Arduino Implementation (`components/lights/`)

### Buffering Strategy (Static Buffer + Bitmap)

- **Goal:** Memory efficiency (~300-400 bytes for 100 LEDs), reliable reassembly.
- **Configuration (`Config.h`):** Use `#define` for `MAX_LEDS`, `BYTES_PER_LED`, calculate `MAX_ANIMATION_FRAME_SIZE`, `MAX_ANIMATION_CHUNKS`, `RECEIVED_CHUNK_BITMAP_SIZE`.
- **Static Allocation:**
    ```c++
    static uint8_t animation_buffer[MAX_ANIMATION_FRAME_SIZE]; 
    static uint8_t received_chunk_bitmap[RECEIVED_CHUNK_BITMAP_SIZE]; 
    enum StreamState { IDLE, RECEIVING, COMPLETE, ERROR };
    static StreamState current_stream_state = IDLE;
    static uint16_t expected_total_bytes = 0; 
    static uint16_t received_chunk_count = 0; 
    static uint8_t expected_chunk_count = 0; 
    ```
- **Bitmap Helpers:** `set_chunk_received()`, `is_chunk_received()`, `clear_received_bitmap()`.

### Handler Logic

- **`ANIMATION_STREAM_START` Handler (Protobuf):**
    - Set state to `RECEIVING`.
    - Clear bitmap, reset counters.
    - Store `expected_total_bytes` from message value.
    - Calculate `expected_chunk_count`.
    - Check if `expected_total_bytes` exceeds `MAX_ANIMATION_FRAME_SIZE`.
    - Start optional timeout.
- **Raw Data Handler (`RawMessageHandler` for `0x700`):**
    - Ignore if not `RECEIVING`.
    - Extract `sequence_number` and `payload`.
    - Validate `sequence_number` against `expected_chunk_count`.
    - Ignore if chunk already received (check bitmap).
    - Calculate `offset` and `bytes_to_copy` (handle last partial chunk using `expected_total_bytes`).
    - Check buffer bounds.
    - `memcpy` payload into `animation_buffer` at `offset`.
    - Set bit in `received_chunk_bitmap`.
    - Increment `received_chunk_count`.
    - If `received_chunk_count == expected_chunk_count`, set state to `COMPLETE`.
- **`ANIMATION_STREAM_END` Handler (Protobuf):**
    - Ignore if not `RECEIVING`.
    - If `received_chunk_count == expected_chunk_count`, set state to `COMPLETE`.
    - Otherwise, set state to `ERROR` (stream ended prematurely/chunks lost).
    - Cancel optional timeout.

### Main Loop (`loop()`)

- If `current_stream_state == COMPLETE`:
    - Process `animation_buffer` (e.g., `FastLED.show()`).
    - Set state back to `IDLE`.
- If `current_stream_state == ERROR`:
    - Handle error.
    - Set state back to `IDLE`.
- Check optional stream timeout.

<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: frontend_implementation -->
## Frontend Implementation

- Use existing `tools/animations/animator.html` as a base.
- Implement WebSocket connection to the server backend.
- Send animation data (e.g., JSON representation) to the server for storage/playback.
- Provide UI controls to trigger `play` commands on the server, targeting specific light components.
- Display status feedback received from the server via WebSocket.
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: performance_considerations -->
## Performance & Constraints

- **CAN Bus:** Raw frames maximize data payload (7 bytes vs 3 in Protobuf). Target ID `0x700` allows filtering. Estimated utilization for 60 LEDs @ 30 FPS is manageable (~25-30% assuming efficient packing).
- **Arduino Memory:** Static buffer strategy keeps RAM usage low (~300-400 bytes for 100 LEDs).
- **Arduino CPU:** Hardware filtering (if available/used) minimizes load from irrelevant CAN traffic. Reassembly logic needs to be efficient. `memcpy` is generally fast.
- **Timing:** Server drives the frame rate. Arduino processes frames as they complete. Potential latency between web UI preview and physical LEDs needs testing.
<!-- LLM_CONTEXT_END -->

<!-- LLM_CONTEXT: next_steps -->
## Next Steps

Implementation Plan (from `ANIMATION_PIPELINE.md`, adapted for this design):

1.  **CrossPlatformCAN Protocol Extension:**
    *   Modify `lights.proto` with new command IDs. Run `./protocol/build.sh`.
    *   Implement C++ changes in `ProtobufCANInterface` (raw send/receive methods, C API, modified `process()`). Build `libcaninterface.so`.
    *   Implement Python wrapper changes in `interface.py` (CFFI updates, new methods).
2.  **Server API Enhancement:**
    *   Implement WebSocket handler for receiving animation data/commands from frontend.
    *   Implement REST endpoints (if needed beyond WebSocket).
    *   Build CAN transmission logic (chunking, calling Protobuf/raw send methods).
3.  **Arduino Lights Component Update:**
    *   Implement buffer logic (static buffer, bitmap).
    *   Register Protobuf handlers for START/END messages.
    *   Register Raw handler for data ID `0x700`.
    *   Implement reassembly logic.
    *   Integrate with LED control library (e.g., FastLED).
4.  **Frontend Animation UI Integration:**
    *   Integrate animator tool.
    *   Implement WebSocket communication with server.
    *   Add controls for sending animations and triggering playback.

Testing priorities remain as outlined in `ANIMATION_PIPELINE.md`.
<!-- LLM_CONTEXT_END -->
