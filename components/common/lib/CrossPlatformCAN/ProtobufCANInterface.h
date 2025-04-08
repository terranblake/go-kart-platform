/**
 * ProtobufCANInterface.h - Protocol Buffer over CAN interface
 * Works on both Raspberry Pi and Arduino
 */

#ifndef PROTOBUF_CAN_INTERFACE_H
#define PROTOBUF_CAN_INTERFACE_H

#include "CANInterface.h"
#include "common.pb.h"

// Max number of message handlers
// todo: switch to using PLATFORM_EMBEDDED
#if defined(PLATFORM_ARDUINO)
#define MAX_HANDLERS 32
#elif defined(PLATFORM_LINUX)
#define MAX_HANDLERS 128
#endif

// Function pointer type for message handlers
typedef void (*MessageHandler)(kart_common_MessageType, kart_common_ComponentType, 
                             uint8_t, uint8_t, kart_common_ValueType, int32_t);

class ProtobufCANInterface {
public:
  /**
   * Constructor
   * 
   * @param nodeId The ID of this node on the CAN network
   */
  ProtobufCANInterface(uint32_t nodeId);
  
  /**
   * Initialize the CAN interface
   * 
   * @param baudRate The CAN bus speed (typically 500000 for 500kbps)
   * @param canDevice The CAN device name (used only on Linux, e.g., "can0")
   * @return true on success, false on failure
   */
  bool begin(long baudRate = 500000, const char* canDevice = "can0");
  
  /**
   * Register a handler for specific message types
   * 
   * @param msg_type Type of message to handle (command/status/etc)
   * @param type Component type to handle
   * @param component_id Component ID to handle (or 0xFF for all)
   * @param command_id Command ID to handle
   * @param handler Function to call when matching message is received
   */
  void registerHandler(kart_common_MessageType msg_type,
                      kart_common_ComponentType type, 
                      uint8_t component_id, 
                      uint8_t command_id, 
                      MessageHandler handler);
  
  /**
   * Send a message over the CAN bus
   * 
   * @param message_type Type of message (command/status)
   * @param component_type Type of component
   * @param component_id ID of the component
   * @param command_id Command ID
   * @param value_type Type of value
   * @param value Value to send
   * @return true on success, false on failure
   */
  bool sendMessage(kart_common_MessageType message_type, 
                  kart_common_ComponentType component_type,
                  uint8_t component_id, uint8_t command_id, 
                  kart_common_ValueType value_type, int32_t value);
  
  /**
   * Process incoming messages
   * Should be called regularly in the main loop
   */
  void process();
  
  /**
   * Helper function to pack a header byte
   */
  static uint8_t packHeader(kart_common_MessageType type, kart_common_ComponentType component);
  
  /**
   * Helper function to unpack a header byte
   */
  static void unpackHeader(uint8_t header, kart_common_MessageType &type, 
                         kart_common_ComponentType &component);
  
  /**
   * Helper function to pack a value based on its type
   */
  static uint32_t packValue(kart_common_ValueType type, int32_t value);
  
  /**
   * Helper function to unpack a value based on its type
   */
  static int32_t unpackValue(kart_common_ValueType type, uint32_t packed_value);

private:
  struct HandlerEntry {
    kart_common_MessageType msg_type;
    kart_common_ComponentType type;
    uint8_t component_id;
    uint8_t command_id;
    MessageHandler handler;
  };

  uint32_t m_nodeId;
  HandlerEntry m_handlers[MAX_HANDLERS];
  int m_numHandlers;
  CANInterface m_canInterface;
  
  // Debug logging helper
  void logMessage(const char *prefix, kart_common_MessageType message_type,
                 kart_common_ComponentType component_type,
                 uint8_t component_id, uint8_t command_id,
                 kart_common_ValueType value_type, int32_t value);

  // Helper function to check if a message matches a handler's criteria
  bool matchesHandler(const HandlerEntry& handler,
                     kart_common_MessageType msg_type,
                     kart_common_ComponentType comp_type,
                     uint8_t component_id,
                     uint8_t command_id);
};


#endif // PROTOBUF_CAN_INTERFACE_H 