/*
 * ProtobufCANInterface.h - Generic interface for Protocol Buffer messaging over CAN
 * For Kart Control System
 * 
 * Compilation note:
 * When compiling for non-Arduino environments, ensure that the following directories
 * are in your include path:
 * - protocol/nanopb (for pb.h)
 * - protocol/generated/nanopb (for protocol buffer headers)
 * 
 * This can be done by:
 * 1. Setting appropriate include paths in your build system
 * 2. Using a pre-build script to copy necessary files to a location in the include path
 * 3. Setting up symbolic links if your system supports them
 */

#ifndef PROTOBUF_CAN_INTERFACE_H
#define PROTOBUF_CAN_INTERFACE_H

#include <stdint.h>

// Platform detection
#if defined(ARDUINO) || defined(ESP32) || defined(ESP8266)
  #define IS_ARDUINO_ENV 1
#else
  #define IS_ARDUINO_ENV 0
#endif

// Platform-specific includes
#if IS_ARDUINO_ENV
  #include <Arduino.h>
  #include <CAN.h>       // Arduino CAN library
  #define DEBUG_PRINT(msg) Serial.print(msg)
  #define DEBUG_PRINTLN(msg) Serial.println(msg)
#else
  #include <iostream>
  #include <string>
  #include <vector>
  #define DEBUG_PRINT(msg) std::cout << msg
  #define DEBUG_PRINTLN(msg) std::cout << msg << std::endl
#endif

// Protocol buffer includes - try to handle both environments
#if IS_ARDUINO_ENV
  #include <pb_encode.h> // Protocol Buffers encode functions
  #include <pb_decode.h> // Protocol Buffers decode functions
  
  // component-based proto-definition imports
  // depends-on: build flags being set my pio
  #include "common.pb.h"
  #include "controls.pb.h"

  #if COMPONENT_TYPE == COMPONENT_LIGHTS
  #include "lights.pb.h"
  #endif
#else
  // For non-Arduino environment, include the nanopb header and protocol buffer headers
  // Expect these to be in the include path during compilation
  #include "pb.h"
  #include "common.pb.h"
  #include "controls.pb.h"
  #include "lights.pb.h"
#endif

class ProtobufCANInterface
{
  typedef void (*MessageHandler)(kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, int32_t);

private:
  struct HandlerEntry
  {
    kart_common_ComponentType type;
    uint8_t component_id;
    uint8_t command_id;
    MessageHandler handler;
  };

  uint32_t nodeId;

  static const int MAX_HANDLERS = 64;
  HandlerEntry handlers[MAX_HANDLERS];
  int num_handlers = 0;
  static const long CAN_BAUD_RATE = 500E3;

public:
  ProtobufCANInterface(uint32_t nodeId);

  bool begin(long baudrate = CAN_BAUD_RATE);
  void registerHandler(kart_common_ComponentType type, uint8_t component_id, uint8_t command_id, MessageHandler handler);
  bool sendMessage(kart_common_MessageType message_type, kart_common_ComponentType component_type,
                   uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value);
  void process();

  // Helper functions
  static uint8_t packHeader(kart_common_MessageType type, kart_common_ComponentType component);
  static void unpackHeader(uint8_t header, kart_common_MessageType &type, kart_common_ComponentType &component);

  static uint32_t packValue(kart_common_ValueType type, int32_t value);
  static int32_t unpackValue(kart_common_ValueType type, uint32_t packed_value);

#if defined(DEBUG_MODE) && DEBUG_MODE
  void logMessage(const char *prefix, kart_common_MessageType message_type,
                  kart_common_ComponentType component_type,
                  uint8_t component_id, uint8_t command_id,
                  kart_common_ValueType value_type, int32_t value);
#endif
};

#endif // PROTOBUF_CAN_INTERFACE_H