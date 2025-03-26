<!-- LLM_TASK_META
id: TASK-001
title: Real-time Animation Pipeline for LED Control
priority: high
estimated_time: 8 hours
component: dashboard/server
dependencies: CrossPlatformCAN, components/lights, dashboard/server/api
-->

# Task: Real-time Animation Pipeline for LED Control

## Overview
<!-- LLM_CONTEXT: task_overview -->
Integrate the LED animator tool into the existing server architecture to enable real-time control of physical LED strips connected to Arduino components. The system should allow users to create animations using the web interface, preview them in the browser, and simultaneously display them on the physical LED strips through the CAN network.
<!-- LLM_CONTEXT_END -->

## Requirements
<!-- LLM_CONTEXT: requirements -->
- **CrossPlatformCAN Protocol Extension**:
  - Implement binary data streaming capability over CAN
  - Support start/frame/end message states for data streams
  - Enable configurable memory footprint
  - Allow both sender/receiver to track stream progress

- **Server API Enhancements**:
  - Create endpoints for receiving animation frame data
  - Validate and format frame data appropriately
  - Stream binary data over CAN using updated protocol
  - Implement WebSocket for real-time status updates

- **Arduino Light Controller Updates**:
  - Implement binary protocol reception
  - Buffer and process animation frames efficiently
  - Control LED strips based on received animation data
  - Monitor RAM/flash usage closely

- **Frontend Animation UI**:
  - Integrate animator dashboard with main UI
  - Enable loading/saving animation JSON files
  - Add real-time preview of animations on web UI
  - Implement frame-by-frame transmission to backend
<!-- LLM_CONTEXT_END -->

## Design
<!-- LLM_CONTEXT: design -->
✅ **Design Complete**: See the detailed [Animation Pipeline Design Document](DESIGN.md).

The design outlines:
- Binary protocol extension with animation flags
- LED data packing for efficient transmission
- Memory-optimized Arduino implementation
- Frame-based animation transmission approach
- Performance considerations and optimizations

The design has been reviewed and approved, balancing performance requirements (30 FPS target) with hardware constraints (2KB RAM limit on Arduino, 1Mbps CAN bus).
<!-- LLM_CONTEXT_END -->

## Implementation Plan
<!-- LLM_CONTEXT: implementation_plan -->
### 1. CrossPlatformCAN Protocol Extension
1. Add binary stream message type to protocol definitions
   - Create new message type for binary data streams
   - Define state flags and metadata fields
   - Update protocol documentation

2. Implement C++ binary protocol handler
   - Add stream state tracking functionality
   - Create buffer management for fragmented data
   - Implement sender and receiver interfaces

3. Create Python binding for binary protocol
   - Extend CANInterfaceWrapper to support binary data
   - Add stream management functions
   - Implement progress tracking and callbacks

**⚠️ IMPLEMENTATION REVIEW CHECKPOINT**: The binary protocol modifications involve extending both C++ and Python interfaces. Does this approach maintain backward compatibility?

### 2. Server API Enhancement
1. Create WebSocket handler for animation control
   - Implement animation state management
   - Add socket events for status updates
   - Create frame transmission queue

2. Develop REST endpoints for animation management
   - Add animation upload/storage endpoint
   - Create animation playback control endpoint
   - Implement animation status query endpoint

3. Build CAN transmission layer
   - Implement frame chunking logic
   - Add error handling and retransmission
   - Create progress tracking

**⚠️ IMPLEMENTATION REVIEW CHECKPOINT**: The server API will need to handle both WebSocket and REST interfaces. Is this approach maintainable and scalable?

### 3. Arduino Lights Component Update
1. Extend protocol handler for binary data
   - Add stream reassembly functionality
   - Implement frame buffering
   - Create state tracking

2. Optimize LED control for animation data
   - Implement efficient frame processing
   - Add animation transitions
   - Create memory management strategies

3. Add diagnostic capabilities
   - Implement memory usage tracking
   - Add error reporting
   - Create performance metrics

**⚠️ IMPLEMENTATION REVIEW CHECKPOINT**: Arduino implementation must be memory-efficient. Are there additional optimizations needed for this implementation?

### 4. Frontend Animation UI Integration
1. Integrate animator tool with dashboard UI
   - Add animator component to main UI
   - Create navigation and control elements
   - Implement animation preview

2. Implement animation transmission
   - Add WebSocket connection for real-time control
   - Create frame serialization
   - Implement progress tracking

3. Build animation library management
   - Add save/load functionality
   - Create animation presets
   - Implement animation editor

**⚠️ IMPLEMENTATION REVIEW CHECKPOINT**: The frontend will need to handle both the visual preview and transmission to hardware. Is there a better approach to separate these concerns?
<!-- LLM_CONTEXT_END -->

## Technical Constraints
<!-- LLM_CONTEXT: constraints -->
- **CAN Bus Limitations**:
  - 8-byte maximum message size per frame
  - 500 kbps maximum throughput
  - Limited bandwidth availability for other operations

- **Arduino Resource Constraints**:
  - Limited RAM (2KB-8KB depending on model)
  - Flash memory endurance concerns
  - Processing power limitations

- **Timing Requirements**:
  - Animation playback needs to be synchronized
  - Minimize latency between web preview and physical display
  - Handle variable network conditions

- **Protocol Constraints**:
  - Maintain backward compatibility with existing CAN messages
  - Error handling must be robust
  - Must handle connection interruptions gracefully

- **Security Considerations**:
  - Validate all animation data from web clients
  - Prevent buffer overflow attacks
  - Manage resource consumption to prevent DoS
<!-- LLM_CONTEXT_END -->

## Testing Strategy
<!-- LLM_CONTEXT: testing -->
### CrossPlatformCAN Protocol Tests
- **Unit Tests**:
  - Test binary message creation and parsing
  - Test stream state management
  - Test buffer handling and memory management
  - Test error recovery scenarios

- **Integration Tests**:
  - Test binary transmission between C++ and Python implementations
  - Test performance under various load conditions
  - Test recovery from interrupted streams

### Server API Tests
- **Unit Tests**:
  - Test WebSocket event handling
  - Test animation data validation
  - Test frame chunking logic

- **Integration Tests**:
  - Test end-to-end animation transmission
  - Test concurrent animation streams
  - Test error handling and recovery

### Arduino Implementation Tests
- **Unit Tests**:
  - Test binary message reception and parsing
  - Test LED control with animation data
  - Test memory usage patterns

- **Hardware Tests**:
  - Test with actual LED strips
  - Measure performance metrics
  - Validate RAM and flash usage

### Frontend Tests
- **Unit Tests**:
  - Test animation rendering
  - Test WebSocket communication
  - Test UI state management

- **End-to-End Tests**:
  - Test animator to LED strip workflow
  - Test animation library management
  - Test under various network conditions
<!-- LLM_CONTEXT_END -->

## Acceptance Criteria
<!-- LLM_CONTEXT: acceptance_criteria -->
- [ ] **Protocol Enhancement**:
  - [ ] CrossPlatformCAN can transmit arbitrary binary data
  - [ ] Protocol correctly handles start/frame/end states
  - [ ] Memory usage is configurable and efficient
  - [ ] Full backward compatibility is maintained

- [ ] **Server Implementation**:
  - [ ] WebSocket provides real-time animation status
  - [ ] REST API enables animation management
  - [ ] Animation data is correctly chunked and transmitted
  - [ ] Error conditions are properly handled and reported

- [ ] **Arduino Implementation**:
  - [ ] Animation frames are correctly received and parsed
  - [ ] LED strips display animations accurately
  - [ ] RAM usage stays below 70% of available memory
  - [ ] Flash usage is optimized

- [ ] **Frontend Implementation**:
  - [ ] Animator tool is accessible from dashboard
  - [ ] Animations preview correctly in browser
  - [ ] Animation transmission to hardware works reliably
  - [ ] UI provides meaningful feedback on animation status

- [ ] **Performance**:
  - [ ] Animation playback is visually synchronized between web and LEDs
  - [ ] System handles animations of at least 30 FPS
  - [ ] CAN bus utilization remains below 50% during animation
<!-- LLM_CONTEXT_END -->

## References
<!-- LLM_CONTEXT: references -->
- [Animation Pipeline Design Document](../DESIGN.md)
- [Animator HTML Tool](tools/animations/animator.html)
- [CrossPlatformCAN Library](components/common/lib/CrossPlatformCAN/README.md)
- [Lights Component Implementation](components/lights/README.md)
- [Dashboard Server README](dashboard/server/README.md)
- [CAN Protocol Documentation](CONTEXT.md#core-concepts)
- [Terminal Operations Guide](TERMINAL.md#background-process-management)
- [Testing Guide](TESTING.md#component-test-protocol)
<!-- LLM_CONTEXT_END -->