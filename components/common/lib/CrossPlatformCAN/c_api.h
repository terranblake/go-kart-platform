#ifndef CAN_C_API_H
#define CAN_C_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Include common.pb.h to get the enum declarations
#include "include/common.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations of C++ types for opaque pointer
struct ProtobufCANInterface;
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
 * Register a handler for binary data messages
 * Used for larger payloads like animation frames
 * 
 * @param handle Handle to the interface
 * @param msg_type Type of message to handle (command/status/etc)
 * @param comp_type Component type to handle
 * @param component_id Component ID to handle (or 0xFF for all)
 * @param command_id Command ID to handle
 * @param handler Function to call when matching message is received
 */
void can_interface_register_binary_handler(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(int, int, uint8_t, uint8_t, int, const void*, size_t)
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
 * Send binary data over the CAN bus using multiple frames
 * 
 * @param handle Handle to the interface
 * @param msg_type Type of message (command/status)
 * @param comp_type Type of component
 * @param component_id ID of the component
 * @param command_id Command ID
 * @param value_type Type of value
 * @param data Pointer to binary data
 * @param data_size Size of the binary data in bytes
 * @return true on success, false on failure
 */
bool can_interface_send_binary_data(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    const void* data,
    size_t data_size
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

#endif // CAN_C_API_H 