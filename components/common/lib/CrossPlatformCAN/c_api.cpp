// C API implementation
#include "c_api.h"
#include "CANInterface.h"
#include "ProtobufCANInterface.h"
#include <stdio.h>

// Define EXPORT macro for visibility
#define EXPORT __attribute__((visibility("default")))

// Store pointers to handler functions
struct HandlerWrapper {
    void (*handler)(int, int, uint8_t, uint8_t, int, int32_t);
};

#define MAX_WRAPPERS 128
static HandlerWrapper g_handlers[MAX_WRAPPERS];
static int g_num_handlers = 0;

// Global wrapper function for message handlers
static void global_message_handler(kart_common_MessageType msg_type,
                               kart_common_ComponentType comp_type,
                               uint8_t comp_id,
                               uint8_t cmd_id,
                               kart_common_ValueType val_type,
                               int32_t val) {
    // Find and call the appropriate handler
    for (int i = 0; i < g_num_handlers; i++) {
        if (g_handlers[i].handler) {
            g_handlers[i].handler(
                static_cast<int>(msg_type),
                static_cast<int>(comp_type),
                comp_id,
                cmd_id,
                static_cast<int>(val_type),
                val
            );
            break;
        }
    }
}

extern "C" {

// Constructor and destructor wrappers
EXPORT can_interface_t can_interface_create(uint32_t node_id) {
    printf("C API: can_interface_create called with node_id=0x%X\n", node_id);
    ProtobufCANInterface* interface = new ProtobufCANInterface(node_id);
    printf("C API: interface created at address %p\n", interface);
    return interface;
}

EXPORT void can_interface_destroy(can_interface_t handle) {
    printf("C API: can_interface_destroy called with handle=%p\n", handle);
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
    printf("C API: can_interface_begin called with handle=%p, baudrate=%ld, device=%s\n", 
           handle, baudrate, device ? device : "null");
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_begin\n");
        return false;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    bool result = interface->begin(baudrate, device);
    printf("C API: can_interface_begin %s\n", result ? "succeeded" : "failed");
    return result;
}

EXPORT void can_interface_register_handler(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)
) {
    printf("C API: can_interface_register_handler called with handle=%p, msg_type=%d, comp_type=%d, component_id=%u, command_id=%u\n", 
           handle, msg_type, comp_type, component_id, command_id);
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_register_handler\n");
        return;
    }
    
    // Store the handler in our global array
    if (g_num_handlers < MAX_WRAPPERS) {
        g_handlers[g_num_handlers].handler = handler;
        g_num_handlers++;
    } else {
        printf("C API ERROR: Too many handlers registered\n");
        return;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    
    interface->registerHandler(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id,
        command_id,
        global_message_handler
    );
    printf("C API: handler registered successfully for msg_type=%d, comp_type=%d, component_id=%u, command_id=%u\n", 
           msg_type, comp_type, component_id, command_id);
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