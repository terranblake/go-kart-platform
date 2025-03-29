# CrossPlatformCAN Test Suite

# Fetch Google Test if not already available
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.11.0
)
FetchContent_MakeAvailable(googletest)

# Include directories
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../..  # For Protocol Buffer headers
)

# Test executable sources
set(TEST_SOURCES
  test_can_interface.cpp
  test_protobuf_interface.cpp
  test_raw_message_handling.cpp
)

# Create test executable
add_executable(can_tests ${TEST_SOURCES})

# Link against gtest and our library
target_link_libraries(can_tests
  gtest_main
  CrossPlatformCAN
)

# Add tests to CTest
add_test(NAME can_interface_tests COMMAND can_tests)

# Add a custom target for building tests
add_custom_target(tests DEPENDS can_tests)
