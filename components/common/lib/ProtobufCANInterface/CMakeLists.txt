cmake_minimum_required(VERSION 3.10)
project(ProtobufCANInterface)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include directories
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Source files
set(SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ProtobufCANInterface.cpp
)

# Add library target
add_library(ProtobufCANInterface ${SOURCES})

# Add executable target for testing
add_executable(test_can_interface 
  ${CMAKE_CURRENT_SOURCE_DIR}/test/test_can_interface.cpp
)

# Link the library to the test executable
target_link_libraries(test_can_interface ProtobufCANInterface)

# Custom command to prepare includes before build
add_custom_target(prepare_includes
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/prepare_includes.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../../..
  COMMENT "Preparing include files for ProtobufCANInterface"
)

# Make the library depend on prepare_includes
add_dependencies(ProtobufCANInterface prepare_includes) 