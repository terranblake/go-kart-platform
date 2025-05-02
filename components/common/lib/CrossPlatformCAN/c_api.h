#ifndef C_API_H
#define C_API_H

#include <stdint.h>
#include <stdbool.h>

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
void can_interface_register_handler(
    can_interface_t handle,
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(uint32_t, kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, int32_t, uint8_t)
);
bool can_interface_send_message(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    int32_t value
);
void can_interface_process(can_interface_t handle);

#ifdef __cplusplus
}
#endif

#endif // C_API_H 