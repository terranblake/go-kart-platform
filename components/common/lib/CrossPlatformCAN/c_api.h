#ifndef C_API_H
#define C_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // for size_t

// Include common.pb.h to get the enum declarations
#include "include/common.pb.h"

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations of C++ types for opaque pointer
struct ProtobufCANInterface;
typedef struct ProtobufCANInterface* can_interface_t;

// Constructor and destructor
can_interface_t can_interface_create(uint32_t node_id);
void can_interface_destroy(can_interface_t handle);

// Member function wrappers
bool can_interface_begin(can_interface_t handle, long baudrate, const char* device);

// Regular message handler registration
void can_interface_register_handler(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)
);

// Animation handler registration
void can_interface_register_animation_handler(
    can_interface_t handle,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(uint8_t, uint8_t, const uint8_t*, size_t, bool)
);

// Regular message sending
bool can_interface_send_message(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    int32_t value
);

// Animation data streaming
bool can_interface_send_animation_data(
    can_interface_t handle,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    const uint8_t* data,
    size_t length,
    uint8_t chunk_size
);

// Message processing
void can_interface_process(can_interface_t handle);

#ifdef __cplusplus
}
#endif

#endif // C_API_H 