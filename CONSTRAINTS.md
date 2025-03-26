# Go-Kart Platform Solution Constraints

This document defines required characteristics for solutions and forbidden approaches to prevent workarounds that don't properly solve problems.

## Required Solution Characteristics

<!-- LLM_CODE_MARKER: required_characteristics -->
### Communication Requirements
- All CAN communication MUST use the CrossPlatformCAN library
- All message handlers MUST be registered with proper component and command IDs
- All messages MUST follow the protocol format defined in protocol definitions
- Status updates MUST be sent at appropriate intervals (not too frequent, not too rare)

### Testing Requirements
- All code changes MUST include appropriate tests
- All component tests MUST pass before deployment
- All code MUST include proper error handling
- Tests MUST have clear exit conditions (timeout, max iterations)

### Platform Requirements
- Arduino components MUST be PlatformIO-compatible
- Dashboard code MUST work on Raspberry Pi
- Protocol definitions MUST be backward compatible unless a major version change
- User interfaces MUST follow the existing design pattern
<!-- LLM_CODE_MARKER_END -->

## Implementation Standards

<!-- LLM_CODE_MARKER: implementation_standards -->
### C++ Component Standards
- Use modern C++ (C++11 or later)
- Follow Arduino best practices for microcontroller code
- Use consistent error handling patterns
- Expose a clear public API
- Separate concerns (communication, business logic, hardware)
- Document public methods with proper comments

### Python Dashboard Standards
- Follow PEP 8 style guide
- Use type hints in new code
- Use proper exception handling
- Separate Flask routes from business logic
- Implement proper logging
- Use virtual environments for dependencies

### Protocol Standards
- Define clear message structures
- Use semantic versioning for protocol changes
- Document all message types
- Follow Protocol Buffer best practices
- Keep messages compact (particularly for Arduino)
<!-- LLM_CODE_MARKER_END -->

## Forbidden Approaches

<!-- LLM_CODE_MARKER: forbidden_approaches -->
### Communication Anti-Patterns
- ❌ Bypassing the CrossPlatformCAN library for direct CAN access
- ❌ Creating custom message formats outside the protocol definition (found at protocol/*.proto)
- ❌ Hardcoding message IDs that should come from protocol constants
- ❌ Ignoring error responses from components
- ❌ Polling when event-based approaches are available

### Implementation Anti-Patterns
- ❌ Disabling compiler warnings or error checks
- ❌ Using hardcoded values that should be configurable
- ❌ Implementing workarounds for hardware issues in software
- ❌ Modifying generated protocol code directly
- ❌ Commenting out error handling for convenience
- ❌ Using blocking delays in main loops

### Testing Anti-Patterns
- ❌ Creating infinite or open-ended test loops
- ❌ Skipping test cases that are "hard to implement"
- ❌ Testing only the happy path
- ❌ Ignoring resource cleanup in tests
- ❌ Writing tests that depend on external services without mocking
<!-- LLM_CODE_MARKER_END -->

## Must-Have vs. Nice-to-Have Features

<!-- LLM_CODE_MARKER: feature_prioritization -->
### Must-Have Features
- ✅ Core communication between components
- ✅ Basic light control (on/off, brightness, signals)
- ✅ Component status reporting
- ✅ Error handling and reporting
- ✅ Configuration mechanism
- ✅ Basic telemetry storage

### Nice-to-Have Features
- ⭐ Advanced light animations
- ⭐ Remote telemetry access
- ⭐ Extended protocol features
- ⭐ User authentication for dashboard
- ⭐ Mobile app integration
- ⭐ Analytics dashboard
<!-- LLM_CODE_MARKER_END -->

## Edge Cases to Handle

<!-- LLM_CODE_MARKER: edge_cases -->
### Communication Edge Cases
- Component power loss during communication
- CAN bus overload conditions
- Message collision and retransmission
- Invalid message format handling
- Out-of-sequence message handling

### Hardware Edge Cases
- Voltage fluctuations
- Temperature extremes
- Sensor failures
- LED strip partial failures
- Clock drift between components

### Software Edge Cases
- Long-running operations blocking the main loop
- Flash memory wear with frequent updates
- Memory leaks in long-running processes
- Dashboard browser compatibility issues
- Protocol version mismatches
<!-- LLM_CODE_MARKER_END -->

## Acceptance Criteria for Solutions

<!-- LLM_CODE_MARKER: acceptance_criteria -->
### Functionality Criteria
- Solution completely addresses the specified problem
- Solution works across all defined platforms
- Solution handles defined edge cases
- Solution does not introduce new issues

### Performance Criteria
- Communication remains responsive (< 100ms latency)
- Memory usage stays within appropriate limits
- CPU usage does not significantly increase
- Power consumption is considered

### Code Quality Criteria
- Code passes all linting checks
- Code is well-documented
- Code follows project style guidelines
- Code is testable and includes tests
- Code is maintainable by other developers
<!-- LLM_CODE_MARKER_END -->

## Decision Framework

<!-- LLM_CODE_MARKER: decision_framework -->
When choosing between multiple possible approaches, evaluate them against:

1. **Reliability**: How robust is the solution to failures?
2. **Simplicity**: Is it the simplest solution that solves the problem?
3. **Maintainability**: Will others understand and be able to modify it?
4. **Performance**: Does it operate within resource constraints?
5. **Compatibility**: Does it work with existing components?
6. **Testability**: Can it be thoroughly tested?

Choose the solution that provides the best balance of these factors for the specific context.
<!-- LLM_CODE_MARKER_END --> 