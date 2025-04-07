/**
 * ProtobufCANInterface.h - Protocol Buffer over CAN interface
 * Works on both Raspberry Pi and Arduino
 */

#ifndef PROTOBUF_CAN_INTERFACE_H
#define PROTOBUF_CAN_INTERFACE_H

#include "CANInterface.h"
#include "common.pb.h"

// Max number of message handlers
#ifdef PLATFORM_ARDUINO || PLATFORM_EMBEDDED
#define MAX_HANDLERS 32
#else
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
};

// C API for Python CFFI
#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to the C++ ProtobufCANInterface
typedef struct ProtobufCANInterface* can_interface_t;

/**
 * Create a new CAN interface
 * 
 * @param node_id The ID of this node on the CAN network
 * @return Handle to the interface or NULL on failure
 */
can_interface_t can_interface_create(uint32_t node_id);

/**
 * Destroy a CAN interface
 * 
 * @param handle Handle to the interface
 */
void can_interface_destroy(can_interface_t handle);

/**
 * Initialize the CAN interface
 * 
 * @param handle Handle to the interface
 * @param baudrate The CAN bus speed (typically 500000 for 500kbps)
 * @param device The CAN device name (used only on Linux, e.g., "can0")
 * @return true on success, false on failure
 */
bool can_interface_begin(can_interface_t handle, long baudrate, const char* device);

/**
 * Register a handler for specific message types
 * 
 * @param handle Handle to the interface
 * @param msg_type Type of message to handle (command/status/etc)
 * @param comp_type Component type to handle
 * @param component_id Component ID to handle (or 0xFF for all)
 * @param command_id Command ID to handle
 * @param handler Function to call when matching message is received
 */
void can_interface_register_handler(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)
);

/**
 * Send a message over the CAN bus
 * 
 * @param handle Handle to the interface
 * @param msg_type Type of message (command/status)
 * @param comp_type Type of component
 * @param component_id ID of the component
 * @param command_id Command ID
 * @param value_type Type of value
 * @param value Value to send
 * @return true on success, false on failure
 */
bool can_interface_send_message(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    int32_t value
);

/**
 * Process incoming messages
 * Should be called regularly in the main loop
 * 
 * @param handle Handle to the interface
 */
void can_interface_process(can_interface_t handle);

#ifdef __cplusplus
}
#endif

#endif // PROTOBUF_CAN_INTERFACE_H 