cmake_minimum_required(VERSION 3.10)
project(CrossPlatformCAN)

# Set C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to build shared library (needed for Python CFFI)
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" ON)

# Platform detection
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Building for Linux/Raspberry Pi")
    add_definitions(-D__linux__)
    set(PLATFORM_LINUX TRUE)
elseif(DEFINED ARDUINO)
    message(STATUS "Building for Arduino")
    add_definitions(-DARDUINO)
    set(PLATFORM_ARDUINO TRUE)
else()
    message(WARNING "Unknown platform, defaulting to Linux build")
    add_definitions(-D__linux__)
    set(PLATFORM_LINUX TRUE)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../..  # For Protocol Buffer headers
)

# Source files
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/CANInterface.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ProtobufCANInterface.cpp
)

# Add library target
add_library(CrossPlatformCAN ${SOURCES})

# Set visibility flags to ensure symbols are exported properly
if(BUILD_SHARED_LIBS)
    # Ensure symbols are properly exported from the shared library
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(CrossPlatformCAN PRIVATE -fvisibility=default)
    endif()
    
    # Force export of C API symbols - add version script for Linux
    if(PLATFORM_LINUX)
        # Create a version script to export our C API symbols
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/exports.map 
            "{ global: can_interface_*; local: *; };"
        )
        # Apply version script to linker flags
        set_target_properties(CrossPlatformCAN PROPERTIES
            LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_BINARY_DIR}/exports.map"
        )
    endif()
    
    # Set other shared library properties
    set_target_properties(CrossPlatformCAN PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        VERSION 1.0.0
        SOVERSION 1
    )
endif()

# Add example executables if on Linux
if(PLATFORM_LINUX)
    # Raspberry Pi example
    add_executable(rpi_example 
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/rpi_example/rpi_example.cpp
    )
    target_link_libraries(rpi_example CrossPlatformCAN)
    
    # Test executables
    add_executable(test_protobuf_can
        ${CMAKE_CURRENT_SOURCE_DIR}/test/test_protobuf_can.cpp
    )
    target_link_libraries(test_protobuf_can CrossPlatformCAN)
    
    add_executable(test_message_type_handlers
        ${CMAKE_CURRENT_SOURCE_DIR}/test/test_message_type_handlers.cpp
    )
    target_link_libraries(test_message_type_handlers CrossPlatformCAN)
endif()

# Installation rules
install(TARGETS CrossPlatformCAN
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/CANInterface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ProtobufCANInterface.h
    DESTINATION include/CrossPlatformCAN
)

# Install Python module
if(PLATFORM_LINUX)
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python/
            DESTINATION lib/python3/dist-packages/CrossPlatformCAN
            FILES_MATCHING PATTERN "*.py")
endif() 