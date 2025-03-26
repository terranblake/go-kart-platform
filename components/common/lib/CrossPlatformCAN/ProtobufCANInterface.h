/**
 * ProtobufCANInterface.h - Protocol Buffer over CAN interface
 * Works on both Raspberry Pi and Arduino
 */

#ifndef PROTOBUF_CAN_INTERFACE_H
#define PROTOBUF_CAN_INTERFACE_H

#include "CANInterface.h"
#include "common.pb.h"

// Max number of message handlers
#define MAX_HANDLERS 32

// Animation stream buffer configuration
#define MAX_ANIMATION_BUFFER_SIZE 1024  // Default maximum animation buffer size
#define DEFAULT_FRAME_CHUNK_SIZE 6      // 6 bytes per frame (2 LEDs per message)

// Function pointer type for message handlers
typedef void (*MessageHandler)(kart_common_MessageType, kart_common_ComponentType, 
                             uint8_t, uint8_t, kart_common_ValueType, int32_t);

// Additional handler for animation binary data streams
typedef void (*AnimationStreamHandler)(uint8_t component_id, uint8_t command_id, 
                                     const uint8_t* data, size_t length, bool is_last_chunk);

// Stream state tracking for binary data protocols                             
struct StreamState {
  bool active;                   // Whether a stream is currently active
  uint8_t component_id;          // Component ID for the stream
  uint8_t command_id;            // Command ID for the stream
  uint16_t total_size;           // Total size of the stream (if known)
  uint16_t received_size;        // Size received so far
  uint16_t chunk_count;          // Number of chunks received
  uint8_t* buffer;               // Data buffer (if accumulating)
  uint16_t buffer_size;          // Current buffer allocation
  uint16_t buffer_capacity;      // Maximum buffer capacity
  AnimationStreamHandler handler; // Handler to call for stream data
};

class ProtobufCANInterface {
public:
  /**
   * Constructor
   * 
   * @param nodeId The ID of this node on the CAN network
   * @param bufferSize Maximum buffer size for animation streams (0 for no buffering)
   */
  ProtobufCANInterface(uint32_t nodeId, uint16_t bufferSize = MAX_ANIMATION_BUFFER_SIZE);
  
  /**
   * Destructor - cleans up resources
   */
  ~ProtobufCANInterface();
  
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
   * Register a handler for animation binary data streams
   * 
   * @param component_id Component ID to handle
   * @param command_id Command ID to handle
   * @param handler Function to call when animation data is received
   */
  void registerAnimationHandler(uint8_t component_id, 
                              uint8_t command_id, 
                              AnimationStreamHandler handler);
  
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
   * Send animation data over CAN bus as a stream of messages
   * 
   * @param component_type Type of component
   * @param component_id ID of the component
   * @param command_id Command ID
   * @param data Pointer to binary data
   * @param length Length of data in bytes
   * @param chunk_size Maximum bytes per message (default: 6 bytes per frame)
   * @return true on success, false on failure
   */
  bool sendAnimationData(kart_common_ComponentType component_type,
                        uint8_t component_id, uint8_t command_id,
                        const uint8_t* data, size_t length,
                        uint8_t chunk_size = DEFAULT_FRAME_CHUNK_SIZE);
  
  /**
   * Process incoming messages
   * Should be called regularly in the main loop
   */
  void process();
  
  /**
   * Helper function to pack a header byte
   */
  static uint8_t packHeader(kart_common_MessageType type, 
                           kart_common_ComponentType component,
                           kart_common_AnimationFlag animation_flag = kart_common_AnimationFlag_ANIMATION_NONE);
  
  /**
   * Helper function to unpack a header byte
   */
  static void unpackHeader(uint8_t header, kart_common_MessageType &type, 
                         kart_common_ComponentType &component,
                         kart_common_AnimationFlag &animation_flag);
  
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
  
  struct AnimationHandlerEntry {
    uint8_t component_id;
    uint8_t command_id;
    AnimationStreamHandler handler;
  };

  uint32_t m_nodeId;
  HandlerEntry m_handlers[MAX_HANDLERS];
  int m_numHandlers;
  
  // Animation stream handlers (maximum 8 different animation handlers)
  AnimationHandlerEntry m_animationHandlers[8];
  int m_numAnimationHandlers;
  
  // Stream state for receiving animation data
  StreamState m_streamState;
  
  CANInterface m_canInterface;
  
  // Process an incoming animation stream message
  void processAnimationMessage(kart_common_MessageType message_type, 
                             kart_common_ComponentType component_type,
                             kart_common_AnimationFlag animation_flag,
                             uint8_t component_id, uint8_t command_id,
                             uint32_t value);
  
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
 * Register a handler for animation binary data streams
 * 
 * @param handle Handle to the interface
 * @param component_id Component ID to handle
 * @param command_id Command ID to handle
 * @param handler Function to call when animation data is received
 */
void can_interface_register_animation_handler(
    can_interface_t handle,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(uint8_t, uint8_t, const uint8_t*, size_t, bool)
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
 * Send animation data over CAN bus as a stream of messages
 * 
 * @param handle Handle to the interface
 * @param comp_type Type of component
 * @param component_id ID of the component
 * @param command_id Command ID
 * @param data Pointer to binary data
 * @param length Length of data in bytes
 * @param chunk_size Maximum bytes per message (default: 6 bytes per frame)
 * @return true on success, false on failure
 */
bool can_interface_send_animation_data(
    can_interface_t handle,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    const uint8_t* data,
    size_t length,
    uint8_t chunk_size
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