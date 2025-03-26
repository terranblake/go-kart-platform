/**
 * ProtobufCANInterface.cpp - Implementation of Protocol Buffer over CAN interface
 */

#include "ProtobufCANInterface.h"

// Include the Protocol Buffer definitions - make sure path is relative to build system
// Different build systems might include files from different base paths
#ifdef ARDUINO
#include "protocol/generated/nanopb/lights.pb.h"
#else
#include "../../../protocol/generated/nanopb/lights.pb.h"
#endif

// Debug mode
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

// Constructor
ProtobufCANInterface::ProtobufCANInterface(uint32_t nodeId)
    : m_nodeId(nodeId), m_numHandlers(0), m_numRGBHandlers(0)
{
    // Initialize RGB frame buffer
    m_rgbFrameBuffer.received_segments = 0;
    m_rgbFrameBuffer.total_segments = 0;
    m_rgbFrameBuffer.frame_in_progress = false;
    m_rgbFrameBuffer.last_update_time = 0;
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

void ProtobufCANInterface::registerRGBStreamHandler(uint8_t component_id, RGBStreamHandler handler)
{
    if (m_numRGBHandlers < MAX_HANDLERS) {
        m_rgbStreamHandlers[m_numRGBHandlers].component_id = component_id;
        m_rgbStreamHandlers[m_numRGBHandlers].handler = handler;
        m_numRGBHandlers++;
        
        // Log registration
        printf("Registered RGB Stream handler for component %d\n", component_id);
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

bool ProtobufCANInterface::sendRGBStream(uint8_t component_id, const uint8_t* rgbData, uint16_t numLeds)
{
    if (numLeds > RGB_MAX_SEGMENTS * 2) { // Each segment holds 2 pixels (6 bytes)
        printf("ProtobufCANInterface: Too many LEDs for stream (%d), max is %d\n", 
               numLeds, RGB_MAX_SEGMENTS * 2);
        return false;
    }
    
    // Calculate number of segments needed
    uint8_t total_segments = (numLeds + 1) / 2; // Round up division
    
    for (uint8_t segment = 0; segment < total_segments; segment++) {
        // Prepare CAN message (8 bytes)
        CANMessage msg;
        msg.id = m_nodeId;
        msg.length = 8;
        
        // Header byte: [2 bits MessageType][3 bits ComponentType][3 bits reserved]
        msg.data[0] = packHeader(kart_common_MessageType_COMMAND, kart_common_ComponentType_LIGHTS);
        
        // Byte 1: Protocol version / flags (0 for now)
        msg.data[1] = 0;
        
        // Byte 2: Component ID
        msg.data[2] = component_id;
        
        // Byte 3: RGB_STREAM command ID
        msg.data[3] = kart_lights_LightCommandId_RGB_STREAM;
        
        // Byte 4: [4 bits segment index][4 bits total segments]
        msg.data[4] = ((segment & 0x0F) << 4) | (total_segments & 0x0F);
        
        // Bytes 5-7: RGB data for up to 2 LEDs (6 bytes)
        uint16_t dataOffset = segment * 6; // 6 bytes per message (2 RGB pixels)
        
        // Copy RGB data, handling the case where we might not have a full 6 bytes at the end
        for (uint8_t i = 0; i < 6 && (dataOffset + i) < (numLeds * 3); i++) {
            msg.data[5 + i % 3] = rgbData[dataOffset + i];
        }
        
        // Send the message
        if (!m_canInterface.sendMessage(msg)) {
            printf("ProtobufCANInterface: Failed to send RGB stream segment %d\n", segment);
            return false;
        }
        
#if DEBUG_MODE
        printf("Sent RGB segment %d/%d for component %d\n", 
               segment, total_segments, component_id);
#endif
    }
    
    return true;
}

void ProtobufCANInterface::process()
{
    CANMessage msg;
    
    // Check for completion of RGB frames due to timeout
    checkRGBFrameComplete();
    
    // Process all pending messages
    while (m_canInterface.messageAvailable() && m_canInterface.receiveMessage(msg)) {
        kart_common_MessageType msgType;
        kart_common_ComponentType compType;
        
        // Unpack header
        unpackHeader(msg.data[0], msgType, compType);
        
        // Extract fields
        uint8_t componentId = msg.data[2];
        uint8_t commandId = msg.data[3];
        kart_common_ValueType valueType = static_cast<kart_common_ValueType>(msg.data[4] >> 4);
        
        // Check if this is an RGB stream message
        if (compType == kart_common_ComponentType_LIGHTS && 
            commandId == kart_lights_LightCommandId_RGB_STREAM) {
            
            // Extract segment info from byte 4
            uint8_t segment_index = (msg.data[4] >> 4) & 0x0F;
            uint8_t total_segments = msg.data[4] & 0x0F;
            
            // Process the RGB stream segment
            processRGBStreamSegment(componentId, segment_index, total_segments, 
                                  &msg.data[5], 3);
                                  
            continue; // Skip regular message processing
        }
        
        // Unpack value
        uint32_t packed_value = ((uint32_t)msg.data[5] << 16) | 
                              ((uint32_t)msg.data[6] << 8) | 
                              msg.data[7];
        int32_t value = unpackValue(valueType, packed_value);
        
#if DEBUG_MODE
        logMessage("RECV", msgType, compType, componentId, commandId, valueType, value);
#endif
        
        // Find and call matching handlers
        for (int i = 0; i < m_numHandlers; i++) {
            if (m_handlers[i].msg_type == msgType &&
                m_handlers[i].type == compType &&
                (m_handlers[i].component_id == componentId || m_handlers[i].component_id == 0xFF) &&
                m_handlers[i].command_id == commandId) {
                
                // Call the handler
                m_handlers[i].handler(msgType, compType, componentId, commandId, valueType, value);
            }
        }
    }
}

void ProtobufCANInterface::processRGBStreamSegment(uint8_t component_id, 
                                                uint8_t segment_index, 
                                                uint8_t total_segments,
                                                const uint8_t* data, 
                                                uint8_t data_length)
{
    unsigned long currentTime = millis();
    
    // Check if this is a new frame or continuation
    if (!m_rgbFrameBuffer.frame_in_progress || 
        m_rgbFrameBuffer.component_id != component_id ||
        m_rgbFrameBuffer.total_segments != total_segments ||
        (currentTime - m_rgbFrameBuffer.last_update_time) > RGB_SEGMENT_TIMEOUT) {
        
        // Start a new frame
        m_rgbFrameBuffer.frame_in_progress = true;
        m_rgbFrameBuffer.component_id = component_id;
        m_rgbFrameBuffer.total_segments = total_segments;
        m_rgbFrameBuffer.received_segments = 0;
        
        // Clear buffer
        memset(m_rgbFrameBuffer.data, 0, sizeof(m_rgbFrameBuffer.data));
    }
    
    // Update the timestamp
    m_rgbFrameBuffer.last_update_time = currentTime;
    
    // Copy data to the appropriate position in the buffer
    uint16_t offset = segment_index * RGB_CHUNK_SIZE;
    if (offset + data_length <= sizeof(m_rgbFrameBuffer.data)) {
        memcpy(m_rgbFrameBuffer.data + offset, data, data_length);
        
        // Mark this segment as received
        m_rgbFrameBuffer.received_segments |= (1 << segment_index);
        
        // Check if we have all segments
        if (m_rgbFrameBuffer.received_segments == ((1 << total_segments) - 1)) {
            // All segments received, call the handler
            for (int i = 0; i < m_numRGBHandlers; i++) {
                if (m_rgbStreamHandlers[i].component_id == component_id ||
                    m_rgbStreamHandlers[i].component_id == 0xFF) {
                    
                    uint16_t numLeds = total_segments * 2; // 2 LEDs per segment
                    m_rgbStreamHandlers[i].handler(m_rgbFrameBuffer.data, numLeds);
                }
            }
            
            // Reset frame state
            m_rgbFrameBuffer.frame_in_progress = false;
        }
    }
}

void ProtobufCANInterface::checkRGBFrameComplete()
{
    // Check for timeout on a partially received frame
    if (m_rgbFrameBuffer.frame_in_progress) {
        unsigned long currentTime = millis();
        if ((currentTime - m_rgbFrameBuffer.last_update_time) > RGB_SEGMENT_TIMEOUT) {
            // Frame timed out, reset state
            m_rgbFrameBuffer.frame_in_progress = false;
            
#if DEBUG_MODE
            printf("RGB frame timeout, resetting buffer\n");
#endif
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
    // Unpack based on kart_common_ValueType
    switch (type) {
    case kart_common_ValueType_BOOLEAN:
        return packed_value ? 1 : 0;
    case kart_common_ValueType_INT8:
        return static_cast<int8_t>(packed_value & 0xFF);
    case kart_common_ValueType_UINT8:
        return static_cast<uint8_t>(packed_value & 0xFF);
    case kart_common_ValueType_INT16:
        return static_cast<int16_t>(packed_value & 0xFFFF);
    case kart_common_ValueType_UINT16:
        return static_cast<uint16_t>(packed_value & 0xFFFF);
    case kart_common_ValueType_INT24:
        // Sign extend 24-bit value to 32-bit
        {
            uint32_t val = packed_value & 0xFFFFFF;
            return (val & 0x800000) ? (val | 0xFF000000) : val;
        }
    case kart_common_ValueType_UINT24:
    default:
        return packed_value & 0xFFFFFF;
    }
}

void ProtobufCANInterface::logMessage(const char *prefix, kart_common_MessageType message_type,
                                    kart_common_ComponentType component_type,
                                    uint8_t component_id, uint8_t command_id,
                                    kart_common_ValueType value_type, int32_t value)
{
    // Simple log message to console
    printf("%s: Type=%d, Comp=%d, ID=%d, Cmd=%d, VType=%d, Val=%ld\n",
           prefix, message_type, component_type, component_id, command_id, value_type, value);
}

// C API implementation

can_interface_t can_interface_create(uint32_t node_id)
{
    return new ProtobufCANInterface(node_id);
}

void can_interface_destroy(can_interface_t handle)
{
    delete static_cast<ProtobufCANInterface*>(handle);
}

bool can_interface_begin(can_interface_t handle, long baudrate, const char* device)
{
    return static_cast<ProtobufCANInterface*>(handle)->begin(baudrate, device);
}

void can_interface_register_handler(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    void (*handler)(int, int, uint8_t, uint8_t, int, int32_t))
{
    // Create a C++ wrapper for the C function
    auto cpp_handler = [handler](kart_common_MessageType msg_type,
                               kart_common_ComponentType comp_type,
                               uint8_t component_id, uint8_t command_id,
                               kart_common_ValueType value_type, int32_t value) {
        handler(msg_type, comp_type, component_id, command_id, value_type, value);
    };
    
    static_cast<ProtobufCANInterface*>(handle)->registerHandler(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id, command_id, cpp_handler);
}

void can_interface_register_rgb_stream_handler(
    can_interface_t handle,
    uint8_t component_id,
    void (*handler)(uint8_t*, uint16_t))
{
    // Create a C++ wrapper for the C function
    auto cpp_handler = [handler](uint8_t* data, uint16_t num_leds) {
        handler(data, num_leds);
    };
    
    static_cast<ProtobufCANInterface*>(handle)->registerRGBStreamHandler(
        component_id, cpp_handler);
}

bool can_interface_send_message(
    can_interface_t handle,
    int msg_type,
    int comp_type,
    uint8_t component_id,
    uint8_t command_id,
    int value_type,
    int32_t value)
{
    return static_cast<ProtobufCANInterface*>(handle)->sendMessage(
        static_cast<kart_common_MessageType>(msg_type),
        static_cast<kart_common_ComponentType>(comp_type),
        component_id, command_id,
        static_cast<kart_common_ValueType>(value_type),
        value);
}

bool can_interface_send_rgb_stream(
    can_interface_t handle,
    uint8_t component_id,
    const uint8_t* rgb_data,
    uint16_t num_leds)
{
    return static_cast<ProtobufCANInterface*>(handle)->sendRGBStream(
        component_id, rgb_data, num_leds);
}

void can_interface_process(can_interface_t handle)
{
    static_cast<ProtobufCANInterface*>(handle)->process();
} 
