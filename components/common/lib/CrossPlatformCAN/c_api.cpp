// C API implementation
#include "c_api.h"
#include "CANInterface.h"
#include "ProtobufCANInterface.h"
#include <stdio.h>

// Define EXPORT macro for visibility
#define EXPORT __attribute__((visibility("default")))

// Store pointers to handler functions
struct HandlerWrapper {
    can_message_handler_t handler;
};

#define MAX_WRAPPERS 128
static HandlerWrapper g_handlers[MAX_WRAPPERS];
static int g_num_handlers = 0;

// Global wrapper function for message handlers
static void global_message_handler(uint16_t message_id, int32_t value) {
#ifdef PLATFORM_LINUX
    printf("C API: global_message_handler called with message_id=0x%X, value=%d\n", 
           message_id, value);
#endif

    // Find and call the appropriate handler
    for (int i = 0; i < g_num_handlers; i++) {
        // compare the message_id with the handler's message_id using protobuf_can_interface.cpp::matchesHandler
        // need to unpack the message_id into the components
        kart_common_MessageType msg_type;
        kart_common_ComponentType comp_type;
        uint8_t component_id;
        uint8_t command_id;
        kart_common_ValueType value_type;
        unpackMessageId(message_id, msg_type, comp_type, component_id, command_id, value_type);
        
        if (!ProtobufCANInterface::matchesHandler(g_handlers[i].handler, msg_type, comp_type, component_id, command_id)) {
            continue;
        }

        g_handlers[i].handler(message_id, value);
        break;
    }
}

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
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    can_message_handler_t handler
) {
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_register_handler\n");
        return;
    }

    if (g_num_handlers >= MAX_WRAPPERS) {
        printf("C API ERROR: Maximum number of handlers reached\n");
        return;
    }

    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    
    // Store the handler
    g_handlers[g_num_handlers].handler = handler;
    g_num_handlers++;
    
    interface->registerHandler(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id,
        command_id,
        global_message_handler
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