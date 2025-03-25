# CrossPlatformCAN Tests

This directory contains tests for the CrossPlatformCAN library, focusing on the Protocol Buffer over CAN implementation.

## Test Structure

- `MockCANInterface.h`: A mock implementation of the CANInterface for testing
- `TestProtobufCANInterface.h`: A wrapper around ProtobufCANInterface that uses the mock interface
- `CANTestWrapper.h/cpp`: Helper for injecting the mock interface
- `test_protobuf_can.cpp`: The main test runner

## Current Test Coverage

1. `sendMessage` functionality with:
   - Message type: COMMAND (testing light control)
   - Message type: STATUS (testing control status)

## Running the Tests

To build and run the tests:

```bash
make
make run
```

## Adding More Tests

To add more tests:

1. Create new test functions in `test_protobuf_can.cpp`
2. Add the function calls to the `main()` function

## Future Test Improvements

- Add tests for message reception and handler invocation
- Test different value types and data ranges
- Test error conditions and edge cases
- Add integration tests with real protocol buffer messages 