// C API implementation
#include "c_api.h"
#include "CANInterface.h"
#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <map>
#include <string>
#include <vector>
#include <memory>

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

// Global structure to store handlers and received messages
struct {
    std::map<HandlerKey, void(*)(int, int, uint8_t, uint8_t, int, int32_t)> handlers;
    std::vector<std::tuple<int, int, uint8_t, uint8_t, int, int32_t>> received_messages;
} g_can_state;

// Custom message handler that will store messages to be processed later
class CAPIMessageHandler : public MessageHandler {
public:
    void handleMessage(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, 
                      uint8_t component_id, uint8_t command_id, 
                      kart_common_ValueType value_type, int32_t value) override {
        printf("C API: Received message - msg_type=%d, comp_type=%d, component_id=%u, command_id=%u, value_type=%d, value=%d\n",
               static_cast<int>(msg_type), static_cast<int>(comp_type), component_id, command_id, 
               static_cast<int>(value_type), value);
               
        // Store the message to be processed in can_interface_process
        g_can_state.received_messages.push_back(std::make_tuple(
            static_cast<int>(msg_type),
            static_cast<int>(comp_type),
            component_id,
            command_id,
            static_cast<int>(value_type),
            value
        ));
    }
};

// Global instance of our message handler (will be registered with the interface)
std::unique_ptr<CAPIMessageHandler> g_message_handler;

// Constructor and destructor wrappers
can_interface_t can_interface_create(uint32_t node_id) {
    printf("C API: can_interface_create called with node_id=0x%X\n", node_id);
    
    // Create our message handler
    if (!g_message_handler) {
        g_message_handler = std::make_unique<CAPIMessageHandler>();
    }
    
    // Create the interface
    ProtobufCANInterface* interface = new ProtobufCANInterface(node_id);
    
    // Register our message handler for all messages
    interface->registerHandler(g_message_handler.get());
    
    printf("C API: interface created at address %p with global message handler\n", interface);
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
    g_can_state.handlers[key] = handler;
    
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
    
    // Process any received messages and dispatch to registered handlers
    if (!g_can_state.received_messages.empty()) {
        printf("C API: Processing %zu received messages\n", g_can_state.received_messages.size());
        
        for (const auto& msg : g_can_state.received_messages) {
            int msg_type, comp_type, value_type;
            uint8_t component_id, command_id;
            int32_t value;
            
            std::tie(msg_type, comp_type, component_id, command_id, value_type, value) = msg;
            
            // Look for an exact match handler
            HandlerKey key = {comp_type, component_id, command_id};
            auto it = g_can_state.handlers.find(key);
            
            if (it != g_can_state.handlers.end()) {
                printf("C API: Dispatching message to registered handler\n");
                it->second(msg_type, comp_type, component_id, command_id, value_type, value);
            } else {
                printf("C API: No handler registered for this message\n");
            }
        }
        
        // Clear processed messages
        g_can_state.received_messages.clear();
    }
}