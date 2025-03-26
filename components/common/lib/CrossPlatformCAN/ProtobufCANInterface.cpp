/**
 * ProtobufCANInterface.cpp - Implementation of Protocol Buffer over CAN interface
 */

#include "ProtobufCANInterface.h"

// Debug mode
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

// Constructor
ProtobufCANInterface::ProtobufCANInterface(uint32_t nodeId)
    : m_nodeId(nodeId), m_numHandlers(0), m_binaryInProgress(false), m_binarySize(0)
{
    // Constructor implementation
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
        m_handlers[m_numHandlers].binary_handler = nullptr;
        m_handlers[m_numHandlers].is_binary = false;
        m_numHandlers++;
        
        // Always log registrations, not just in debug mode
        logMessage("REGD", msg_type, type, component_id, 
                  command_id, kart_common_ValueType_BOOLEAN, false);
    }
}

void ProtobufCANInterface::registerBinaryHandler(kart_common_MessageType msg_type,
                                               kart_common_ComponentType type, 
                                               uint8_t component_id, 
                                               uint8_t command_id, 
                                               BinaryDataHandler handler)
{
    if (m_numHandlers < MAX_HANDLERS) {
        m_handlers[m_numHandlers].msg_type = msg_type;
        m_handlers[m_numHandlers].type = type;
        m_handlers[m_numHandlers].component_id = component_id;
        m_handlers[m_numHandlers].command_id = command_id;
        m_handlers[m_numHandlers].handler = nullptr;
        m_handlers[m_numHandlers].binary_handler = handler;
        m_handlers[m_numHandlers].is_binary = true;
        m_numHandlers++;
        
        // Always log registrations, not just in debug mode
        logMessage("REGB", msg_type, type, component_id, 
                  command_id, kart_common_ValueType_BOOLEAN, false);
    }
}

bool ProtobufCANInterface::sendMessage(kart_common_MessageType message_type, 
                                      kart_common_ComponentType component_type,
                                      uint8_t component_id, uint8_t command_id, 
                                      kart_common_ValueType value_type, int32_t value) {
    printf("ProtobufCANInterface: sendMessage called - messageType=%d, componentType=%d, componentId=%u, commandId=%u, valueType=%d, value=%d\n",
           message_type, component_type, component_id, command_id, value_type, value);
  
#if DEBUG_MODE
    logMessage("SEND", message_type, component_type, component_id, command_id, value_type, value);
#endif

    // Pack first byte (message type and component type)
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
    
    printf("ProtobufCANInterface: Created CAN frame - ID: 0x%X, Data:", msg.id);
    for (int i = 0; i < msg.length; i++) {
        printf(" %02X", msg.data[i]);
    }
    printf("\n");
    
    // Send using base class
    bool result = m_canInterface.sendMessage(msg);
    printf("ProtobufCANInterface: sendMessage %s\n", result ? "succeeded" : "failed");
    return result;
}

bool ProtobufCANInterface::sendBinaryData(kart_common_MessageType message_type,
                                         kart_common_ComponentType component_type,
                                         uint8_t component_id, uint8_t command_id,
                                         kart_common_ValueType value_type,
                                         const void* data, size_t data_size) {
    // Check if data is too large
    if (data_size > MAX_BINARY_SIZE) {
        printf("ProtobufCANInterface: Binary data too large (%zu > %zu bytes)\n", 
              data_size, (size_t)MAX_BINARY_SIZE);
        return false;
    }
    
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    
    // Calculate how many frames we need
    // First frame can hold 3 bytes of data (bytes 5-7)
    // Subsequent frames can hold 5 bytes of data (bytes 3-7)
    const size_t FIRST_FRAME_DATA_SIZE = 3;
    const size_t NEXT_FRAME_DATA_SIZE = 5;
    
    size_t total_frames = 1;  // At least one frame
    size_t remaining_data = data_size;
    
    if (remaining_data > FIRST_FRAME_DATA_SIZE) {
        remaining_data -= FIRST_FRAME_DATA_SIZE;
        total_frames += (remaining_data + NEXT_FRAME_DATA_SIZE - 1) / NEXT_FRAME_DATA_SIZE;
    }
    
    printf("ProtobufCANInterface: Sending %zu bytes of binary data in %zu frames\n", 
          data_size, total_frames);
    
    // Pack first byte (message type and component type)
    uint8_t header = packHeader(message_type, component_type);
    
    // Send first frame with start flag and sequence 0
    CANMessage msg;
    msg.id = m_nodeId;
    msg.length = 8;
    
    msg.data[0] = header;
    msg.data[1] = 0x80; // Start flag in bit 7, sequence 0
    msg.data[2] = component_id;
    msg.data[3] = command_id;
    msg.data[4] = (static_cast<uint8_t>(value_type) << 4) | 
                 (total_frames > 15 ? 15 : total_frames); // Value type + total frames
    
    // Calculate how many bytes to send in first frame
    size_t first_frame_size = data_size < FIRST_FRAME_DATA_SIZE ? 
                             data_size : FIRST_FRAME_DATA_SIZE;
    
    // Copy data into first frame
    for (size_t i = 0; i < first_frame_size; i++) {
        msg.data[5 + i] = bytes[i];
    }
    
    // Zero-fill any unused bytes
    for (size_t i = first_frame_size; i < FIRST_FRAME_DATA_SIZE; i++) {
        msg.data[5 + i] = 0;
    }
    
    // Send first frame
    printf("ProtobufCANInterface: Sending binary frame 0/%zu (start)\n", total_frames-1);
    if (!m_canInterface.sendMessage(msg)) {
        printf("ProtobufCANInterface: Failed to send first binary frame\n");
        return false;
    }
    
    // Send subsequent frames if needed
    size_t bytes_sent = first_frame_size;
    uint8_t seq_num = 1;
    
    while (bytes_sent < data_size) {
        // Prepare next frame
        msg.data[0] = header;
        msg.data[1] = (seq_num % 16); // Sequence number (0-15)
        
        // Set end flag if this is the last frame
        if (bytes_sent + NEXT_FRAME_DATA_SIZE >= data_size) {
            msg.data[1] |= 0x40; // End flag in bit 6
        }
        
        msg.data[2] = component_id;
        msg.data[3] = command_id;
        
        // Calculate how many bytes to send in this frame
        size_t frame_size = (data_size - bytes_sent) < NEXT_FRAME_DATA_SIZE ? 
                           (data_size - bytes_sent) : NEXT_FRAME_DATA_SIZE;
        
        // Store frame size in byte 4
        msg.data[4] = frame_size & 0xFF;
        
        // Copy data into frame
        for (size_t i = 0; i < frame_size; i++) {
            msg.data[5 + i] = bytes[bytes_sent + i];
        }
        
        // Zero-fill any unused bytes
        for (size_t i = frame_size; i < NEXT_FRAME_DATA_SIZE; i++) {
            msg.data[5 + i] = 0;
        }
        
        // Send frame
        printf("ProtobufCANInterface: Sending binary frame %u/%zu\n", seq_num, total_frames-1);
        if (!m_canInterface.sendMessage(msg)) {
            printf("ProtobufCANInterface: Failed to send binary frame %u\n", seq_num);
            return false;
        }
        
        bytes_sent += frame_size;
        seq_num++;
    }
    
    printf("ProtobufCANInterface: Successfully sent %zu bytes in %u frames\n", data_size, seq_num);
    return true;
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
    unpackHeader(msg.data[0], msg_type, comp_type);

    // Extract other fields
    uint8_t flags_seq = msg.data[1];
    uint8_t component_id = msg.data[2];
    uint8_t command_id = msg.data[3];
    kart_common_ValueType value_type = static_cast<kart_common_ValueType>(msg.data[4] >> 4);

    // Check if this is a binary data frame (has start or continuation flag)
    bool is_start_frame = (flags_seq & 0x80) != 0;
    bool is_end_frame = (flags_seq & 0x40) != 0;
    uint8_t seq_num = flags_seq & 0x0F;
    
    if (is_start_frame) {
        // Start of binary data
        m_binaryInProgress = true;
        m_binarySize = 0;
        m_binaryMsgType = msg_type;
        m_binaryCompType = comp_type;
        m_binaryComponentId = component_id;
        m_binaryCommandId = command_id;
        m_binaryValueType = value_type;
        
        // First frame can have up to 3 bytes of data (bytes 5-7)
        size_t data_size = 3; // Fixed size for first frame
        
        // Copy data to buffer
        for (size_t i = 0; i < data_size && m_binarySize < MAX_BINARY_SIZE; i++) {
            m_binaryBuffer[m_binarySize++] = msg.data[5 + i];
        }
        
        printf("ProtobufCANInterface: Started binary data reception, frame 0\n");
        
        // If this is also the end frame, process it now
        if (is_end_frame) {
            // Find and execute matching binary handlers
            for (int i = 0; i < m_numHandlers; i++) {
                if (m_handlers[i].is_binary &&
                    m_handlers[i].msg_type == m_binaryMsgType &&
                    m_handlers[i].type == m_binaryCompType &&
                    (m_handlers[i].component_id == m_binaryComponentId || m_handlers[i].component_id == 0xFF) &&
                    m_handlers[i].command_id == m_binaryCommandId) {
                    
                    m_handlers[i].binary_handler(m_binaryMsgType, m_binaryCompType, 
                                               m_binaryComponentId, m_binaryCommandId, 
                                               m_binaryValueType, m_binaryBuffer, m_binarySize);
                }
            }
            
            // Reset binary reception
            m_binaryInProgress = false;
            printf("ProtobufCANInterface: Completed binary data reception, %zu bytes\n", m_binarySize);
        }
    }
    else if (m_binaryInProgress) {
        // Continuation frame for binary data
        
        // Check if this frame belongs to the current binary message
        if (m_binaryComponentId == component_id && m_binaryCommandId == command_id) {
            // Extract frame size from byte 4
            size_t frame_size = msg.data[4] & 0xFF;
            
            // Copy data to buffer (bytes 5-7 have data in continuation frames)
            for (size_t i = 0; i < frame_size && m_binarySize < MAX_BINARY_SIZE; i++) {
                m_binaryBuffer[m_binarySize++] = msg.data[5 + i];
            }
            
            printf("ProtobufCANInterface: Received binary frame %u, %zu bytes so far\n", 
                  seq_num, m_binarySize);
            
            // If this is the end frame, process the complete binary data
            if (is_end_frame) {
                // Find and execute matching binary handlers
                for (int i = 0; i < m_numHandlers; i++) {
                    if (m_handlers[i].is_binary &&
                        m_handlers[i].msg_type == m_binaryMsgType &&
                        m_handlers[i].type == m_binaryCompType &&
                        (m_handlers[i].component_id == m_binaryComponentId || m_handlers[i].component_id == 0xFF) &&
                        m_handlers[i].command_id == m_binaryCommandId) {
                        
                        m_handlers[i].binary_handler(m_binaryMsgType, m_binaryCompType, 
                                                   m_binaryComponentId, m_binaryCommandId, 
                                                   m_binaryValueType, m_binaryBuffer, m_binarySize);
                    }
                }
                
                // Reset binary reception
                m_binaryInProgress = false;
                printf("ProtobufCANInterface: Completed binary data reception, %zu bytes\n", m_binarySize);
            }
        }
        else {
            // This frame is not part of the current binary message
            // Just ignore it for now
            printf("ProtobufCANInterface: Received unrelated frame during binary reception\n");
        }
    }
    else {
        // Regular non-binary message
        
        // Unpack value
        uint32_t packed_value = (static_cast<uint32_t>(msg.data[5]) << 16) |
                               (static_cast<uint32_t>(msg.data[6]) << 8) |
                               msg.data[7];
        int32_t value = unpackValue(value_type, packed_value);

    #if DEBUG_MODE
        logMessage("RECV", msg_type, comp_type, component_id, command_id, value_type, value);
    #endif

        // Find and execute matching handlers - now checking message type as well
        bool handlerFound = false;
        for (int i = 0; i < m_numHandlers; i++) {
            if (!m_handlers[i].is_binary &&
                m_handlers[i].msg_type == msg_type &&
                m_handlers[i].type == comp_type &&
                (m_handlers[i].component_id == component_id || m_handlers[i].component_id == 0xFF) &&
                m_handlers[i].command_id == command_id) {
                
                handlerFound = true;
                m_handlers[i].handler(msg_type, comp_type, component_id, command_id, value_type, value);
            }
        }

        // Echo status if this was a command (optional)
        if (msg_type == kart_common_MessageType_COMMAND) {
            sendMessage(kart_common_MessageType_STATUS, comp_type, component_id, command_id, value_type, value);
        }
    }
}

uint8_t ProtobufCANInterface::packHeader(kart_common_MessageType type, kart_common_ComponentType component)
{
    return (static_cast<uint8_t>(type) << 6) | ((static_cast<uint8_t>(component) & 0x07) << 3);
}

void ProtobufCANInterface::unpackHeader(uint8_t header, kart_common_MessageType &type, kart_common_ComponentType &component)
{
    type = static_cast<kart_common_MessageType>((header >> 6) & 0x03);
    component = static_cast<kart_common_ComponentType>((header >> 3) & 0x07);
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
