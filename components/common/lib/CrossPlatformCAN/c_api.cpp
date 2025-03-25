// C API implementation
#include "c_api.h"
#include "CANInterface.h"
#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <map>
#include <string>

// Global registry for C-style handlers
struct HandlerKey {
    int comp_type;
    uint8_t component_id;
    uint8_t command_id;
    
    // For use as map key
    bool operator<(const HandlerKey& other) const {
        if (comp_type != other.comp_type) return comp_type < other.comp_type;
        if (component_id != other.component_id) return component_id < other.component_id;
        return command_id < other.command_id;
    }
};

std::map<HandlerKey, void(*)(int, int, uint8_t, uint8_t, int, int32_t)> g_handlers;

// Constructor and destructor wrappers
can_interface_t can_interface_create(uint32_t node_id) {
    printf("C API: can_interface_create called with node_id=0x%X\n", node_id);
    ProtobufCANInterface* interface = new ProtobufCANInterface(node_id);
    printf("C API: interface created at address %p\n", interface);
    return interface;
}

void can_interface_destroy(can_interface_t handle) {
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
bool can_interface_begin(can_interface_t handle, long baudrate, const char* device) {
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

void can_interface_register_handler(
    can_interface_t handle,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)
) {
    printf("C API: can_interface_register_handler called with handle=%p, comp_type=%d, component_id=%u, command_id=%u\n", 
           handle, comp_type, component_id, command_id);
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_register_handler\n");
        return;
    }
    
    // Store the handler in our global registry
    HandlerKey key = {comp_type, component_id, command_id};
    g_handlers[key] = handler;
    
    // We'll use our own handler processing in the process() function instead of
    // trying to register with the interface directly
    printf("C API: Handler registered in global registry\n");
}

bool can_interface_send_message(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    int32_t value
) {
    printf("C API: can_interface_send_message called - handle=%p, msg_type=%d, comp_type=%d, component_id=%u, command_id=%u, value_type=%d, value=%d\n", 
           handle, msg_type, comp_type, component_id, command_id, value_type, value);
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_send_message\n");
        return false;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    bool result = interface->sendMessage(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id,
        command_id,
        static_cast<kart_common_ValueType>(value_type),
        value
    );
    printf("C API: can_interface_send_message %s\n", result ? "succeeded" : "failed");
    return result;
}

void can_interface_process(can_interface_t handle) {
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_process\n");
        return;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    
    // Process messages from the CAN bus
    interface->process();
    
    // In a complete implementation, we would check for received messages here
    // and call the appropriate handlers from our global registry
    // This is simplified, but provides a framework for implementing the full functionality
}