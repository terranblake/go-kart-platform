/*
 * ProtobufCANInterface.h - Generic interface for Protocol Buffer messaging over CAN
 */

#ifndef PROTOBUF_CAN_INTERFACE_H
#define PROTOBUF_CAN_INTERFACE_H

#include <stdint.h>
#include <iostream>
#include <string>
#include "common.pb.h"

// Maintain C++ class definition for Arduino
#ifdef __cplusplus

class ProtobufCANInterface
{
public:
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

#endif // __cplusplus

// C API for Python CFFI
#ifdef __cplusplus
extern "C" {
#endif

// Define opaque pointer type for C
typedef struct ProtobufCANInterface ProtobufCANInterface;

// Constants accessible from C
#define MAX_HANDLERS 64
#define CAN_BAUD_RATE 500000

// Constructor/destructor
ProtobufCANInterface* create_can_interface(uint32_t nodeId);
void destroy_can_interface(ProtobufCANInterface* instance);

// Member function wrappers
bool can_interface_begin(ProtobufCANInterface* instance, long baudrate);
void can_interface_register_handler(ProtobufCANInterface* instance, 
                                   int component_type, uint8_t component_id, 
                                   uint8_t command_id, 
                                   void (*handler)(int, int, uint8_t, uint8_t, int, int32_t));
bool can_interface_send_message(ProtobufCANInterface* instance, 
                               int message_type, int component_type, 
                               uint8_t component_id, uint8_t command_id, 
                               int value_type, int32_t value);
void can_interface_process(ProtobufCANInterface* instance);

// Static function wrappers
uint8_t can_interface_pack_header(int type, int component);
void can_interface_unpack_header(uint8_t header, int* type, int* component);
uint32_t can_interface_pack_value(int type, int32_t value);
int32_t can_interface_unpack_value(int type, uint32_t packed_value);

#ifdef __cplusplus
}
#endif

#endif // PROTOBUF_CAN_INTERFACE_H