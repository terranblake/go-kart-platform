# Go-Kart Platform TODOs

## Animation Support Fixes

### CrossPlatformCAN Library Issues
- ~~**Fix C API Type Conversion Issues**: The C API wrapper in `components/common/lib/CrossPlatformCAN/c_api.cpp` has type conversion issues between lambda functions and function pointers. This prevents the `can_interface_send_binary_data` function from being properly compiled into the shared library.~~
  - ✅ Fixed the type conversion issues by defining proper handler types that match the expected function signatures
  - ✅ Improved callback handling to ensure compatibility between C and C++ code

### Animation Protocol Improvements
- ~~**Protocol Registry Updates**: Ensure that the animation commands (ANIMATION_START, ANIMATION_FRAME, ANIMATION_END, ANIMATION_STOP) are properly registered in the protocol registry.~~
  - ✅ Modified protocol registry to check and ensure animation commands are registered during initialization
- **Animation Data Implementation**: Improve the binary data sending mechanism for animation frames to ensure proper delivery of animation data to lighting components.
  - ✅ Added automated tests to verify binary data transmission

### End-to-End Testing
- **Animation Playback Test**: Complete end-to-end testing of animation playback from the dashboard server to the LED controllers.
- **Animation Editor**: Implement a web-based animation editor in the dashboard UI to create and modify animations.

### Documentation
- ✅ **Animation API Documentation**: Updated documentation in README.md with details on animation system improvements
- **Protocol Updates**: Update the CAN protocol documentation to include animation-related message types and formats.

## General Improvements

### Library Compilation Fixes
- **Improve Build Process**: Update the build scripts to properly compile and link the CrossPlatformCAN library with all required functions.
- **CMake Integration**: Ensure the CMake build system correctly includes all source files and exports the necessary symbols.

### Test Coverage
- ✅ **Animation Test Coverage**: Added tests for verifying animation protocol functionality in `dashboard/server/test/test_animation_protocol.py`
- **Protocol Registry Tests**: Add tests to verify protocol registry behavior, particularly for components with the 'ALL' component ID.

### Infrastructure
- **CI/CD Pipeline**: Update the CI/CD pipeline to build and test the CrossPlatformCAN library on both Linux and Raspberry Pi environments.
- **Dependency Management**: Improve management of dependencies, especially for Protocol Buffer-related libraries. 