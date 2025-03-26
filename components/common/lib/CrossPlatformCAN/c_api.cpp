// C API implementation
// Skip this entire file for Arduino platform
#if !defined(PLATFORM_ARDUINO) && !defined(ARDUINO)

#include "c_api.h"
#include "CANInterface.h"
#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <vector>
#include <string.h>

// Define EXPORT macro for visibility
#define EXPORT __attribute__((visibility("default")))

// Constants for animation commands
#define CMD_ANIMATION_START 10
#define CMD_ANIMATION_FRAME 11
#define CMD_ANIMATION_END   12
#define CMD_ANIMATION_STOP  13

// Constants for message types and component types
#define MSG_TYPE_COMMAND    0
#define COMP_TYPE_LIGHTS    0
#define COMP_ID_ALL         255
#define VALUE_TYPE_UINT8    1

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

// Global pointer for current handler wrapper
static CHandlerWrapper* g_current_handler_wrapper = nullptr;

// Global pointer for current binary handler wrapper
static CBinaryHandlerWrapper* g_current_binary_handler_wrapper = nullptr;

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

// Standard message handler function
void standard_message_handler(kart_common_MessageType msg_t, kart_common_ComponentType comp_t, 
                           uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType val_t, int32_t val) {
    if (g_current_handler_wrapper) {
        cpp_handler_bridge(msg_t, comp_t, comp_id, cmd_id, val_t, val, g_current_handler_wrapper);
    }
}

// Binary message handler function
void binary_message_handler(kart_common_MessageType msg_t, kart_common_ComponentType comp_t, 
                         uint8_t comp_id, uint8_t cmd_id, kart_common_ValueType val_t, 
                         const void* data, size_t data_size) {
    if (g_current_binary_handler_wrapper) {
        cpp_binary_handler_bridge(msg_t, comp_t, comp_id, cmd_id, val_t, data, data_size, g_current_binary_handler_wrapper);
    }
}

// Simple implementation of a CAN interface
struct SimpleCANInterface {
    uint32_t node_id;
    CANInterface* can;
    bool initialized;
};

// Create a new CAN interface
EXPORT can_interface_t can_interface_create(uint32_t node_id) {
    printf("Creating CAN interface with node_id=0x%X\n", (unsigned int)node_id);
    
    SimpleCANInterface* interface = new SimpleCANInterface();
    interface->node_id = node_id;
    interface->can = new CANInterface();
    interface->initialized = false;
    
    return interface;
}

// Initialize the CAN interface
EXPORT bool can_interface_begin(can_interface_t handle, long baudrate, const char* device) {
    printf("Initializing CAN interface with baudrate=%ld, device=%s\n", 
           baudrate, device ? device : "NULL");
    
    if (!handle) {
        printf("Error: NULL handle in can_interface_begin\n");
        return false;
    }
    
    SimpleCANInterface* interface = static_cast<SimpleCANInterface*>(handle);
    bool result = interface->can->begin(baudrate, device);
    interface->initialized = result;
    
    printf("CAN interface initialization %s\n", result ? "succeeded" : "failed");
    return result;
}

// Clean up and destroy the CAN interface
EXPORT void can_interface_destroy(can_interface_t handle) {
    printf("Destroying CAN interface\n");
    
    if (!handle) {
        printf("Error: NULL handle in can_interface_destroy\n");
        return;
    }
    
    SimpleCANInterface* interface = static_cast<SimpleCANInterface*>(handle);
    delete interface->can;
    delete interface;
}

// Send a simple message
EXPORT bool can_interface_send_message(
    can_interface_t handle,
    uint8_t msg_type,
    uint8_t comp_type,
    uint8_t component_id,
    uint8_t command_id,
    uint8_t value_type,
    int32_t value
) {
    if (!handle) {
        printf("Error: NULL handle in can_interface_send_message\n");
        return false;
    }
    
    SimpleCANInterface* interface = static_cast<SimpleCANInterface*>(handle);
    if (!interface->initialized) {
        printf("Error: CAN interface not initialized\n");
        return false;
    }
    
    // Create a CAN message
    CANMessage msg;
    msg.id = interface->node_id;
    msg.length = 8;
    
    // Pack header: message type and component type
    msg.data[0] = (msg_type << 6) | (comp_type << 3);
    msg.data[1] = 0;  // Sequence number (not used for simple messages)
    msg.data[2] = component_id;
    msg.data[3] = command_id;
    msg.data[4] = (value_type << 4);  // Value type in upper 4 bits
    
    // Pack value based on value type
    // We'll stick with a simple approach here
    msg.data[5] = (value & 0xFF);
    msg.data[6] = ((value >> 8) & 0xFF);
    msg.data[7] = ((value >> 16) & 0xFF);
    
    return interface->can->sendMessage(msg);
}

// Start an animation sequence
EXPORT bool can_interface_animation_start(can_interface_t handle) {
    printf("Starting animation\n");
    return can_interface_send_message(
        handle, 
        MSG_TYPE_COMMAND, 
        COMP_TYPE_LIGHTS, 
        COMP_ID_ALL, 
        CMD_ANIMATION_START, 
        VALUE_TYPE_UINT8, 
        1  // Value = 1 to start
    );
}

// Helper function to send binary data in chunks
static bool send_binary_data_chunks(
    SimpleCANInterface* interface, 
    uint8_t command_id,
    const uint8_t* data, 
    size_t data_size
) {
    const uint8_t MAX_DATA_PER_FRAME = 6;  // Max data bytes per CAN frame
    bool is_first = true;
    bool is_last = false;
    uint8_t seq_num = 0;
    size_t bytes_sent = 0;
    
    while (bytes_sent < data_size) {
        // Determine how many bytes we can send in this frame
        size_t bytes_to_send = data_size - bytes_sent;
        if (bytes_to_send > MAX_DATA_PER_FRAME) {
            bytes_to_send = MAX_DATA_PER_FRAME;
        }
        
        // Check if this is the last frame
        is_last = (bytes_sent + bytes_to_send >= data_size);
        
        // Create a CAN message
        CANMessage msg;
        msg.id = interface->node_id;
        msg.length = 8;
        
        // Pack header
        msg.data[0] = (MSG_TYPE_COMMAND << 6) | (COMP_TYPE_LIGHTS << 3);
        
        // Set flags in seq_num byte
        uint8_t flags = 0;
        if (is_first) flags |= 0x80;  // Start flag
        if (is_last) flags |= 0x40;   // End flag
        msg.data[1] = flags | (seq_num & 0x3F);  // Flags + sequence number
        
        msg.data[2] = COMP_ID_ALL;
        msg.data[3] = command_id;
        msg.data[4] = (VALUE_TYPE_UINT8 << 4);
        
        // Copy data
        for (size_t i = 0; i < bytes_to_send; i++) {
            msg.data[5 + i] = data[bytes_sent + i];
        }
        
        // Zero-fill any unused bytes
        for (size_t i = bytes_to_send; i < MAX_DATA_PER_FRAME; i++) {
            msg.data[5 + i] = 0;
        }
        
        // Send the frame
        if (!interface->can->sendMessage(msg)) {
            printf("Error: Failed to send binary data frame\n");
            return false;
        }
        
        // Update state for next frame
        bytes_sent += bytes_to_send;
        is_first = false;
        seq_num = (seq_num + 1) & 0x3F;  // Keep sequence number in 6-bit range
    }
    
    return true;
}

// Send an animation frame
EXPORT bool can_interface_animation_frame(
    can_interface_t handle,
    uint8_t frame_index,
    const uint8_t* led_data,
    uint8_t led_count
) {
    if (!handle) {
        printf("Error: NULL handle in can_interface_animation_frame\n");
        return false;
    }
    
    SimpleCANInterface* interface = static_cast<SimpleCANInterface*>(handle);
    if (!interface->initialized) {
        printf("Error: CAN interface not initialized\n");
        return false;
    }
    
    printf("Sending animation frame %u with %u LEDs\n", frame_index, led_count);
    
    // Create the frame data packet
    // Format: [frame_index, led_count, led_data]
    // Each LED is 5 bytes: x, y, r, g, b
    size_t data_size = 2 + (led_count * 5);
    uint8_t* frame_data = new uint8_t[data_size];
    
    // Set frame header
    frame_data[0] = frame_index;
    frame_data[1] = led_count;
    
    // Copy LED data
    memcpy(frame_data + 2, led_data, led_count * 5);
    
    // Send the frame data
    bool result = send_binary_data_chunks(interface, CMD_ANIMATION_FRAME, frame_data, data_size);
    
    // Clean up
    delete[] frame_data;
    
    return result;
}

// End an animation sequence
EXPORT bool can_interface_animation_end(can_interface_t handle, uint8_t total_frames) {
    printf("Ending animation with %u total frames\n", total_frames);
    return can_interface_send_message(
        handle, 
        MSG_TYPE_COMMAND, 
        COMP_TYPE_LIGHTS, 
        COMP_ID_ALL, 
        CMD_ANIMATION_END, 
        VALUE_TYPE_UINT8, 
        total_frames
    );
}

// Stop the current animation
EXPORT bool can_interface_animation_stop(can_interface_t handle) {
    printf("Stopping animation\n");
    return can_interface_send_message(
        handle, 
        MSG_TYPE_COMMAND, 
        COMP_TYPE_LIGHTS, 
        COMP_ID_ALL, 
        CMD_ANIMATION_STOP, 
        VALUE_TYPE_UINT8, 
        0  // Value = 0 to stop
    );
}

// Process incoming messages
EXPORT void can_interface_process(can_interface_t handle) {
    if (!handle) {
        return;
    }
    
    SimpleCANInterface* interface = static_cast<SimpleCANInterface*>(handle);
    if (!interface->initialized) {
        return;
    }
    
    // Process any incoming messages
    CANMessage msg;
    while (interface->can->receiveMessage(msg)) {
        // We're not doing anything with incoming messages in this simplified implementation
        // Just printing them for debugging
        printf("Received CAN message: ID=0x%X, length=%d, data=[", msg.id, msg.length);
        for (int i = 0; i < msg.length; i++) {
            printf("%02X", msg.data[i]);
            if (i < msg.length - 1) printf(" ");
        }
        printf("]\n");
    }
}

#endif // !defined(PLATFORM_ARDUINO) && !defined(ARDUINO)