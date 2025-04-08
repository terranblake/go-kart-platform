// C API implementation
#include "c_api.h"
#include "CANInterface.h"
#include "ProtobufCANInterface.h"
#include <stdio.h>

// Define EXPORT macro for visibility
#define EXPORT __attribute__((visibility("default")))

// Store pointers to handler functions

extern "C" {

// Constructor and destructor wrappers
EXPORT can_interface_t can_interface_create(uint32_t node_id) {
    ProtobufCANInterface* interface = new ProtobufCANInterface(node_id);
    return interface;
}

EXPORT void can_interface_destroy(can_interface_t handle) {
    if (handle) {
        ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
        delete interface;
        printf("C API: interface destroyed\n");
    } else {
        printf("C API ERROR: Attempt to destroy null handle\n");
    }
}

// Member function wrappers
EXPORT bool can_interface_begin(can_interface_t handle, long baudrate, const char* device) {
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_begin\n");
        return false;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    return interface->begin(baudrate, device);
}

EXPORT void can_interface_register_handler(
    can_interface_t handle,
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, int32_t)
) {
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_register_handler\n");
        return;
    }

    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    interface->registerHandler(
        msg_type,
        comp_type,
        component_id,
        command_id,
        handler
    );
}

EXPORT bool can_interface_send_message(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    int32_t value
) {
    if (!handle) {
        printf("Error: Invalid CAN interface handle\n");
        return false;
    }
    
    // Call the C++ method
    ProtobufCANInterface* interface = reinterpret_cast<ProtobufCANInterface*>(handle);
    return interface->sendMessage(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id,
        command_id,
        static_cast<kart_common_ValueType>(value_type),
        value
    );
}

EXPORT void can_interface_process(can_interface_t handle) {
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_process\n");
        return;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    interface->process();
}

}  // extern "C"