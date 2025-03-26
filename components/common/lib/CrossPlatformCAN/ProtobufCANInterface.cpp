/**
 * ProtobufCANInterface.cpp - Implementation of Protocol Buffer over CAN interface
 */

#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <string.h>

// Debug mode
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

// Constructor
ProtobufCANInterface::ProtobufCANInterface(uint32_t nodeId, uint16_t bufferSize)
    : m_nodeId(nodeId), m_numHandlers(0), m_numAnimationHandlers(0)
{
    // Initialize stream state
    memset(&m_streamState, 0, sizeof(m_streamState));
    
    // Allocate buffer for streaming if requested
    if (bufferSize > 0) {
        m_streamState.buffer = new uint8_t[bufferSize];
        m_streamState.buffer_capacity = bufferSize;
    }
}

// Destructor
ProtobufCANInterface::~ProtobufCANInterface()
{
    // Free allocated buffer if it exists
    if (m_streamState.buffer) {
        delete[] m_streamState.buffer;
        m_streamState.buffer = nullptr;
    }
}

// Initialize CAN interface
bool ProtobufCANInterface::begin(long baudRate, const char* canDevice)
{
    return m_canInterface.begin(baudRate, canDevice);
}

void ProtobufCANInterface::registerHandler(kart_common_MessageType msg_type,
                                          kart_common_ComponentType type, 
                                          uint8_t component_id, 
                                          uint8_t command_id, 
                                          MessageHandler handler)
{
    if (m_numHandlers < MAX_HANDLERS) {
        m_handlers[m_numHandlers].msg_type = msg_type;
        m_handlers[m_numHandlers].type = type;
        m_handlers[m_numHandlers].component_id = component_id;
        m_handlers[m_numHandlers].command_id = command_id;
        m_handlers[m_numHandlers].handler = handler;
        m_numHandlers++;
        
        // Always log registrations, not just in debug mode
        logMessage("REGD", msg_type, type, component_id, 
                  command_id, kart_common_ValueType_BOOLEAN, false);
    }
}

void ProtobufCANInterface::registerAnimationHandler(uint8_t component_id, 
                                                  uint8_t command_id, 
                                                  AnimationStreamHandler handler)
{
    if (m_numAnimationHandlers < 8) {  // Maximum 8 animation handlers
        m_animationHandlers[m_numAnimationHandlers].component_id = component_id;
        m_animationHandlers[m_numAnimationHandlers].command_id = command_id;
        m_animationHandlers[m_numAnimationHandlers].handler = handler;
        m_numAnimationHandlers++;
        
#if DEBUG_MODE
        printf("Registered animation handler for component_id=%u, command_id=%u\n", 
               component_id, command_id);
#endif
    }
}

bool ProtobufCANInterface::sendMessage(kart_common_MessageType message_type, 
                                      kart_common_ComponentType component_type,
                                      uint8_t component_id, uint8_t command_id, 
                                      kart_common_ValueType value_type, int32_t value) {
#if DEBUG_MODE
    printf("ProtobufCANInterface: sendMessage called - messageType=%d, componentType=%d, componentId=%u, commandId=%u, valueType=%d, value=%d\n",
           message_type, component_type, component_id, command_id, value_type, value);
#endif
  
#if DEBUG_MODE
    logMessage("SEND", message_type, component_type, component_id, command_id, value_type, value);
#endif

    // Pack first byte (message type, component type, and animation flag)
    uint8_t header = packHeader(message_type, component_type);
    
    // Pack the value based on its type
    uint32_t packed_value = packValue(value_type, value);
    
    // Prepare CAN message (8 bytes)
    CANMessage msg;
    msg.id = m_nodeId;   // Use node ID as CAN ID
    msg.length = 8;      // Always use 8 bytes for consistency
    
    msg.data[0] = header;
    msg.data[1] = 0;                                  // Reserved
    msg.data[2] = component_id;
    msg.data[3] = command_id;
    msg.data[4] = static_cast<uint8_t>(value_type) << 4; // Value type in high nibble
    msg.data[5] = (packed_value >> 16) & 0xFF;           // Value byte 2 (MSB)
    msg.data[6] = (packed_value >> 8) & 0xFF;            // Value byte 1
    msg.data[7] = packed_value & 0xFF;                   // Value byte 0 (LSB)
    
#if DEBUG_MODE
    printf("ProtobufCANInterface: Created CAN frame - ID: 0x%X, Data:", msg.id);
    for (int i = 0; i < msg.length; i++) {
        printf(" %02X", msg.data[i]);
    }
    printf("\n");
#endif
    
    // Send using base class
    bool result = m_canInterface.sendMessage(msg);
#if DEBUG_MODE
    printf("ProtobufCANInterface: sendMessage %s\n", result ? "succeeded" : "failed");
#endif
    return result;
}

bool ProtobufCANInterface::sendAnimationData(kart_common_ComponentType component_type,
                                           uint8_t component_id, uint8_t command_id,
                                           const uint8_t* data, size_t length,
                                           uint8_t chunk_size) 
{
    if (data == nullptr || length == 0 || chunk_size > 6) {
        return false;  // Invalid parameters
    }
    
#if DEBUG_MODE
    printf("ProtobufCANInterface: sendAnimationData - componentType=%d, componentId=%u, commandId=%u, length=%u, chunk_size=%u\n",
           component_type, component_id, command_id, (unsigned int)length, chunk_size);
#endif
    
    // Send animation data in chunks
    size_t sent = 0;
    bool success = true;
    
    while (sent < length && success) {
        kart_common_AnimationFlag flag;
        
        // Set flag based on position in stream
        if (sent == 0) {
            flag = kart_common_AnimationFlag_ANIMATION_START;
        } else if (sent + chunk_size >= length) {
            flag = kart_common_AnimationFlag_ANIMATION_END;
        } else {
            flag = kart_common_AnimationFlag_ANIMATION_FRAME;
        }
        
        // Calculate bytes to send in this chunk
        size_t bytes_to_send = (sent + chunk_size <= length) ? chunk_size : (length - sent);
        
        // Copy chunk into a 32-bit value
        uint32_t chunk_value = 0;
        for (size_t i = 0; i < bytes_to_send && i < 3; i++) {
            chunk_value |= (data[sent + i] << ((2 - i) * 8));
        }
        
        // Send the chunk with appropriate flags
        CANMessage msg;
        msg.id = m_nodeId;
        msg.length = 8;
        
        // Pack header with animation flag
        msg.data[0] = packHeader(kart_common_MessageType_COMMAND, component_type, flag);
        
        // Include the LED index in byte 1 (for animation data)
        msg.data[1] = (sent / 3);  // Each message can carry RGB for 2 LEDs (6 bytes)
        
        msg.data[2] = component_id;
        msg.data[3] = command_id;
        msg.data[4] = static_cast<uint8_t>(kart_common_ValueType_BINARY) << 4;
        
        // Pack binary data into the value bytes
        msg.data[5] = (chunk_value >> 16) & 0xFF;
        msg.data[6] = (chunk_value >> 8) & 0xFF;
        msg.data[7] = chunk_value & 0xFF;
        
        // Send the message
        success = m_canInterface.sendMessage(msg);
        
        if (success) {
            sent += bytes_to_send;
        } else {
#if DEBUG_MODE
            printf("ProtobufCANInterface: Failed to send animation chunk at offset %u\n", 
                   (unsigned int)sent);
#endif
            break;
        }
        
        // Small delay to avoid overwhelming the bus (microseconds)
        #if defined(PLATFORM_ARDUINO)
        delayMicroseconds(100);
        #endif
    }
    
    return success && (sent == length);
}

void ProtobufCANInterface::process()
{
    CANMessage msg;
    
    // Try to receive a message
    if (!m_canInterface.receiveMessage(msg)) {
        return; // No message available
    }
    
    // Message must be 8 bytes for our protocol
    if (msg.length != 8) {
        return;
    }

    // Unpack header
    kart_common_MessageType msg_type;
    kart_common_ComponentType comp_type;
    kart_common_AnimationFlag animation_flag;
    unpackHeader(msg.data[0], msg_type, comp_type, animation_flag);

    // Extract other fields
    uint8_t led_index = msg.data[1];  // Used for animations
    uint8_t component_id = msg.data[2];
    uint8_t command_id = msg.data[3];
    kart_common_ValueType value_type = static_cast<kart_common_ValueType>(msg.data[4] >> 4);

    // Unpack value
    uint32_t packed_value = (static_cast<uint32_t>(msg.data[5]) << 16) |
                           (static_cast<uint32_t>(msg.data[6]) << 8) |
                           msg.data[7];
                           
    // For animation data, handle differently
    if (animation_flag != kart_common_AnimationFlag_ANIMATION_NONE) {
        processAnimationMessage(msg_type, comp_type, animation_flag, component_id, command_id, packed_value);
        return;
    }
    
    // For regular messages, unpack the value
    int32_t value = unpackValue(value_type, packed_value);

#if DEBUG_MODE
    logMessage("RECV", msg_type, comp_type, component_id, command_id, value_type, value);
#endif

    // Find and execute matching handlers - now checking message type as well
    bool handlerFound = false;
    for (int i = 0; i < m_numHandlers; i++) {
        if ((m_handlers[i].msg_type == msg_type) &&
            (m_handlers[i].type == comp_type) &&
            (m_handlers[i].component_id == component_id || m_handlers[i].component_id == 0xFF) &&
            (m_handlers[i].command_id == command_id)) {
            
            handlerFound = true;
            m_handlers[i].handler(msg_type, comp_type, component_id, command_id, value_type, value);
        }
    }

    // Echo status if this was a command (optional)
    if (msg_type == kart_common_MessageType_COMMAND) {
        sendMessage(kart_common_MessageType_STATUS, comp_type, component_id, command_id, value_type, value);
    }
}

void ProtobufCANInterface::processAnimationMessage(kart_common_MessageType message_type, 
                                                 kart_common_ComponentType component_type,
                                                 kart_common_AnimationFlag animation_flag,
                                                 uint8_t component_id, uint8_t command_id,
                                                 uint32_t value)
{
    // Process animation data based on flag
    switch (animation_flag) {
        case kart_common_AnimationFlag_ANIMATION_START:
            // Starting a new stream - initialize state
            m_streamState.active = true;
            m_streamState.component_id = component_id;
            m_streamState.command_id = command_id;
            m_streamState.received_size = 0;
            m_streamState.chunk_count = 0;
            m_streamState.buffer_size = 0;
            
            // Reset buffer if we're using one
            if (m_streamState.buffer) {
                memset(m_streamState.buffer, 0, m_streamState.buffer_capacity);
            }
            
            // Fall through to process the first chunk
            
        case kart_common_AnimationFlag_ANIMATION_FRAME:
        case kart_common_AnimationFlag_ANIMATION_END:
            // Add the current chunk to our state
            if (m_streamState.active && 
                m_streamState.component_id == component_id && 
                m_streamState.command_id == command_id) {
                
                // Extract binary data from the value
                uint8_t chunk[3];
                chunk[0] = (value >> 16) & 0xFF;
                chunk[1] = (value >> 8) & 0xFF;
                chunk[2] = value & 0xFF;
                
                // If we have a buffer, add to it
                if (m_streamState.buffer && 
                    m_streamState.buffer_size + 3 <= m_streamState.buffer_capacity) {
                    
                    // Copy data to buffer
                    memcpy(m_streamState.buffer + m_streamState.buffer_size, chunk, 3);
                    m_streamState.buffer_size += 3;
                }
                
                // Track progress
                m_streamState.received_size += 3;
                m_streamState.chunk_count++;
                
                // If this is the end, deliver the complete buffer if we have one
                if (animation_flag == kart_common_AnimationFlag_ANIMATION_END) {
                    // Find and call matching animation handler
                    for (int i = 0; i < m_numAnimationHandlers; i++) {
                        if ((m_animationHandlers[i].component_id == component_id || 
                             m_animationHandlers[i].component_id == 0xFF) &&
                            (m_animationHandlers[i].command_id == command_id)) {
                            
                            // Call with complete buffer if we have one
                            if (m_streamState.buffer) {
                                m_animationHandlers[i].handler(component_id, command_id, 
                                                             m_streamState.buffer, 
                                                             m_streamState.buffer_size, 
                                                             true);
                            } 
                            // Or call with just this chunk directly if no buffer
                            else {
                                m_animationHandlers[i].handler(component_id, command_id, 
                                                             chunk, 3, 
                                                             true);
                            }
                        }
                    }
                    
                    // Reset state
                    m_streamState.active = false;
                } 
                // For non-end chunks, deliver immediately for modes without a buffer
                else if (!m_streamState.buffer) {
                    // Find and call matching animation handler with just this chunk
                    for (int i = 0; i < m_numAnimationHandlers; i++) {
                        if ((m_animationHandlers[i].component_id == component_id || 
                             m_animationHandlers[i].component_id == 0xFF) &&
                            (m_animationHandlers[i].command_id == command_id)) {
                            
                            m_animationHandlers[i].handler(component_id, command_id, 
                                                         chunk, 3, 
                                                         false);
                        }
                    }
                }
            }
            break;
            
        default:
            // Unknown animation flag - ignore
            break;
    }
}

uint8_t ProtobufCANInterface::packHeader(kart_common_MessageType type, 
                                        kart_common_ComponentType component,
                                        kart_common_AnimationFlag animation_flag)
{
    return (static_cast<uint8_t>(type) << 6) | 
           ((static_cast<uint8_t>(component) & 0x07) << 3) |
           (static_cast<uint8_t>(animation_flag) & 0x07);
}

void ProtobufCANInterface::unpackHeader(uint8_t header, 
                                       kart_common_MessageType &type, 
                                       kart_common_ComponentType &component,
                                       kart_common_AnimationFlag &animation_flag)
{
    type = static_cast<kart_common_MessageType>((header >> 6) & 0x03);
    component = static_cast<kart_common_ComponentType>((header >> 3) & 0x07);
    animation_flag = static_cast<kart_common_AnimationFlag>(header & 0x07);
}

uint32_t ProtobufCANInterface::packValue(kart_common_ValueType type, int32_t value)
{
    // Truncate and mask based on kart_common_ValueType
    switch (type) {
    case kart_common_ValueType_BOOLEAN:
        return value ? 1 : 0;
    case kart_common_ValueType_INT8:
        return static_cast<uint8_t>(value) & 0xFF;
    case kart_common_ValueType_UINT8:
        return static_cast<uint8_t>(value) & 0xFF;
    case kart_common_ValueType_INT16:
        return static_cast<uint16_t>(value) & 0xFFFF;
    case kart_common_ValueType_UINT16:
        return static_cast<uint16_t>(value) & 0xFFFF;
    case kart_common_ValueType_INT24:
    case kart_common_ValueType_UINT24:
    default:
        return static_cast<uint32_t>(value) & 0xFFFFFF;
    }
}

int32_t ProtobufCANInterface::unpackValue(kart_common_ValueType type, uint32_t packed_value)
{
    // Sign extend and interpret based on kart_common_ValueType
    switch (type) {
    case kart_common_ValueType_BOOLEAN:
        return packed_value & 0x01;
    case kart_common_ValueType_INT8:
        return (packed_value & 0x80) ? (packed_value | 0xFFFFFF00) : packed_value;
    case kart_common_ValueType_UINT8:
        return packed_value & 0xFF;
    case kart_common_ValueType_INT16:
        return (packed_value & 0x8000) ? (packed_value | 0xFFFF0000) : packed_value;
    case kart_common_ValueType_UINT16:
        return packed_value & 0xFFFF;
    case kart_common_ValueType_INT24:
        return (packed_value & 0x800000) ? (packed_value | 0xFF000000) : packed_value;
    case kart_common_ValueType_UINT24:
    default:
        return packed_value & 0xFFFFFF;
    }
}

void ProtobufCANInterface::logMessage(const char* prefix, kart_common_MessageType message_type, 
                                     kart_common_ComponentType component_type,
                                     uint8_t component_id, uint8_t command_id, 
                                     kart_common_ValueType value_type, int32_t value)
{
#ifdef PLATFORM_ARDUINO
    char buffer[128];
    snprintf(buffer, sizeof(buffer), 
             "%s: Type=%d, Comp=%d, ID=%d, Cmd=%d, ValType=%d, Val=%ld", 
             prefix, (int)message_type, (int)component_type, 
             component_id, command_id, (int)value_type, (long)value);
    Serial.println(buffer);
#else
    printf("%s: Type=%d, Comp=%d, ID=%d, Cmd=%d, ValType=%d, Val=%d\n", 
           prefix, (int)message_type, (int)component_type, 
           component_id, command_id, (int)value_type, value);
#endif
} 
