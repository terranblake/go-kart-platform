/**
 * ProtobufCANInterface.cpp - Implementation of Protocol Buffer over CAN interface
 */

#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <string.h>

#ifdef PLATFORM_ARDUINO
#include <Arduino.h>
#endif

// Debug mode
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

// Constructor
ProtobufCANInterface::ProtobufCANInterface(uint32_t nodeId, int csPin, int intPin)
    : m_nodeId(nodeId), m_numHandlers(0), m_csPin(csPin), m_intPin(intPin), m_canInterface(csPin, intPin)
{
#if DEBUG_MODE
    printf("ProtobufCANInterface: Constructor called with nodeId=%d maxHandlers=%d\n", nodeId, MAX_HANDLERS);
#endif
}

// Initialize CAN interface
bool ProtobufCANInterface::begin(long baudRate, const char* canDevice)
{
#if DEBUG_MODE
    printf("ProtobufCANInterface: begin called with baudRate=%ld, canDevice=%s\n", baudRate, canDevice);
#endif
    return m_canInterface.begin(baudRate, canDevice, m_csPin, m_intPin);
}

void ProtobufCANInterface::registerHandler(kart_common_MessageType msg_type,
                                          kart_common_ComponentType type, 
                                          uint8_t component_id, 
                                          uint8_t command_id, 
                                          MessageHandler handler)
{
    if (m_numHandlers >= MAX_HANDLERS) {
        logMessage("MAX_HANDLERS_ERROR", msg_type, type, component_id, command_id, kart_common_ValueType_BOOLEAN, 0);
        return;
    }

    m_handlers[m_numHandlers].msg_type = msg_type;
    m_handlers[m_numHandlers].type = type;
    m_handlers[m_numHandlers].component_id = component_id;
    m_handlers[m_numHandlers].command_id = command_id;
    m_handlers[m_numHandlers].handler = handler;
    m_numHandlers++;
    
    // Always log registrations, not just in debug mode
#if DEBUG_MODE
    logMessage("REGD", msg_type, type, component_id, command_id, kart_common_ValueType_BOOLEAN, false);
#endif
}

bool ProtobufCANInterface::sendMessage(kart_common_MessageType message_type, 
                                      kart_common_ComponentType component_type,
                                      uint8_t component_id, uint8_t command_id, 
                                      kart_common_ValueType value_type, int32_t value) {
  
#if CAN_LOGGING_ENABLED
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
    
#if CAN_LOGGING_ENABLED
    printf("ProtobufCANInterface: Created CAN frame - ID: 0x%X, Data:", msg.id);
    for (int i = 0; i < msg.length; i++) {
        printf(" %02X", msg.data[i]);
    }
    printf("\n");
#endif
    
    // Send using base class
    bool result = m_canInterface.sendMessage(msg);
#if CAN_LOGGING_ENABLED
    printf("ProtobufCANInterface: sendMessage result: %d (%s)\n", result, result ? "succeeded" : "failed"); 
#endif
    return result;
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
    uint8_t component_id = msg.data[2];
    uint8_t command_id = msg.data[3];
    kart_common_ValueType value_type = static_cast<kart_common_ValueType>(msg.data[4] >> 4);

    // Unpack value
    uint32_t packed_value = (static_cast<uint32_t>(msg.data[5]) << 16) |
                           (static_cast<uint32_t>(msg.data[6]) << 8) |
                           msg.data[7];
    int32_t value = unpackValue(value_type, packed_value);

#ifdef PLATFORM_ARDUINO
    // Remove filtering of STATUS messages - we'll check message types in handler matching
    if (msg_type == kart_common_MessageType_STATUS) {
        return;
    }
#endif

#if CAN_LOGGING_ENABLED
    logMessage("RECV", msg_type, comp_type, component_id, command_id, value_type, value);
#endif

    // Find and execute matching handlers - now checking message type as well
    bool handlerFound = false;
    for (int i = 0; i < m_numHandlers; i++) {
        if (!matchesHandler(m_handlers[i], msg_type, comp_type, component_id, command_id)) {
            continue;
        }

        handlerFound = true;
        m_handlers[i].handler(msg_type, comp_type, component_id, command_id, value_type, value);
    }

    // Echo status if this was a command (optional)
    if (handlerFound && msg_type == kart_common_MessageType_COMMAND) {
        sendMessage(kart_common_MessageType_STATUS, comp_type, component_id, command_id, value_type, value);
    }
}

uint8_t ProtobufCANInterface::packHeader(kart_common_MessageType type, kart_common_ComponentType component)
{
    uint8_t type_bits = static_cast<uint8_t>(type) << 6;
    uint8_t comp_bits = (static_cast<uint8_t>(component) & 0x07) << 3;
    uint8_t result = type_bits | comp_bits; 
    return result;
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

// Helper function to check if a message matches a handler's criteria
bool ProtobufCANInterface::matchesHandler(const HandlerEntry& handler,
                                        kart_common_MessageType msg_type,
                                        kart_common_ComponentType comp_type,
                                        uint8_t component_id,
                                        uint8_t command_id) {
    return (handler.msg_type == msg_type) &&
           (handler.type == comp_type) &&
           (handler.component_id == component_id || handler.component_id == 0xFF || handler.component_id == 255) &&
           (handler.command_id == command_id);
} 
