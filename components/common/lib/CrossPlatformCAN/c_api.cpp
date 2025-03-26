// C API implementation
// Skip this entire file for Arduino platform
#if !defined(PLATFORM_ARDUINO) && !defined(ARDUINO)

#include "c_api.h"
#include "CANInterface.h"
#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <vector>

// Define EXPORT macro for visibility
#define EXPORT __attribute__((visibility("default")))

// Helper struct to store function pointers for C handlers
struct CHandlerWrapper {
    void (*handler)(int, int, uint8_t, uint8_t, int, int32_t);
};

// Helper struct to store function pointers for C binary handlers
struct CBinaryHandlerWrapper {
    void (*handler)(int, int, uint8_t, uint8_t, int, const void*, size_t);
};

// Storage for registered handler wrappers
std::vector<CHandlerWrapper*> g_handlers;
int g_num_handlers = 0;

// Storage for registered binary handler wrappers
std::vector<CBinaryHandlerWrapper*> g_binary_handlers;
int g_num_binary_handlers = 0;

// Helper function to bridge C++ callbacks to C callbacks
void cpp_handler_bridge(kart_common_MessageType msg_type, 
                       kart_common_ComponentType comp_type,
                       uint8_t component_id, uint8_t command_id,
                       kart_common_ValueType value_type, int32_t value, 
                       CHandlerWrapper* wrapper) {
    if (wrapper && wrapper->handler) {
        wrapper->handler(
            static_cast<int>(msg_type),
            static_cast<int>(comp_type),
            component_id,
            command_id,
            static_cast<int>(value_type),
            value
        );
    }
}

// Helper function to bridge C++ binary callbacks to C binary callbacks
void cpp_binary_handler_bridge(kart_common_MessageType msg_type, 
                             kart_common_ComponentType comp_type,
                             uint8_t component_id, uint8_t command_id,
                             kart_common_ValueType value_type, 
                             const void* data, size_t data_size,
                             CBinaryHandlerWrapper* wrapper) {
    if (wrapper && wrapper->handler) {
        wrapper->handler(
            static_cast<int>(msg_type),
            static_cast<int>(comp_type),
            component_id,
            command_id,
            static_cast<int>(value_type),
            data,
            data_size
        );
    }
}

extern "C" {

// Constructor and destructor wrappers
EXPORT can_interface_t can_interface_create(uint32_t node_id) {
    printf("C API: can_interface_create called with node_id=0x%lX\n", (unsigned long)node_id);
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
    
    // Create a wrapper to store the C handler
    CHandlerWrapper* wrapper = new CHandlerWrapper{handler};
    g_handlers.push_back(wrapper);
    g_num_handlers++;
    
    // Register the C++ handler
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    
    // Use a compatible lambda signature for the handler function
    auto cpp_callback = [wrapper](kart_common_MessageType msg_t, kart_common_ComponentType comp_t, uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType val_t, int32_t val) {
        cpp_handler_bridge(msg_t, comp_t, comp_id, cmd_id, val_t, val, wrapper);
    };
    
    interface->registerHandler(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id,
        command_id,
        cpp_callback
    );
    
    printf("C API: handler registered successfully for msg_type=%d, comp_type=%d, component_id=%u, command_id=%u\n", 
           msg_type, comp_type, component_id, command_id);
}

EXPORT void can_interface_register_binary_handler(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(int, int, uint8_t, uint8_t, int, const void*, size_t)
) {
    printf("C API: can_interface_register_binary_handler called with handle=%p, msg_type=%d, comp_type=%d, component_id=%u, command_id=%u\n", 
           handle, msg_type, comp_type, component_id, command_id);
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_register_binary_handler\n");
        return;
    }
    
    // Create a wrapper to store the C binary handler
    CBinaryHandlerWrapper* wrapper = new CBinaryHandlerWrapper{handler};
    g_binary_handlers.push_back(wrapper);
    g_num_binary_handlers++;
    
    // Register the C++ binary handler with explicit type conversion
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    
    // Define handler type that matches the expected signature in ProtobufCANInterface::registerBinaryHandler
    typedef void (*BinaryDataHandlerType)(kart_common_MessageType, kart_common_ComponentType, uint8_t, uint8_t, kart_common_ValueType, const void*, size_t);
    
    // Create a compatible callback function that will match the expected BinaryDataHandler type
    BinaryDataHandlerType cpp_callback = [wrapper](kart_common_MessageType msg_t, kart_common_ComponentType comp_t, uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType val_t, const void* data, size_t data_size) {
        cpp_binary_handler_bridge(msg_t, comp_t, comp_id, cmd_id, val_t, data, data_size, wrapper);
    };
    
    interface->registerBinaryHandler(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id,
        command_id,
        cpp_callback
    );
    
    printf("C API: binary handler registered successfully for msg_type=%d, comp_type=%d, component_id=%u, command_id=%u\n", 
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
    printf("C API: can_interface_send_message called - handle=%p, msg_type=%d, comp_type=%d, component_id=%u, command_id=%u, value_type=%d, value=%ld\n", 
           handle, msg_type, comp_type, component_id, command_id, value_type, (long)value);
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

EXPORT bool can_interface_send_binary_data(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    const void* data,
    size_t data_size
) {
    printf("C API: can_interface_send_binary_data called - handle=%p, msg_type=%d, comp_type=%d, component_id=%u, command_id=%u, value_type=%d, data_size=%zu\n", 
           handle, msg_type, comp_type, component_id, command_id, value_type, data_size);
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_send_binary_data\n");
        return false;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    bool result = interface->sendBinaryData(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id,
        command_id,
        static_cast<kart_common_ValueType>(value_type),
        data,
        data_size
    );
    printf("C API: can_interface_send_binary_data %s\n", result ? "succeeded" : "failed");
    return result;
}

EXPORT void can_interface_process(can_interface_t handle) {
    if (!handle) {
        printf("C API ERROR: Null handle in can_interface_process\n");
        return;
    }
    
    ProtobufCANInterface* interface = static_cast<ProtobufCANInterface*>(handle);
    interface->process();
}

} // extern "C"

#endif // !defined(PLATFORM_ARDUINO) && !defined(ARDUINO)