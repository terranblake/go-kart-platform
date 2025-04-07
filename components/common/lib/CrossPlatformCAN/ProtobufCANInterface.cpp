/**
 * ProtobufCANInterface.cpp - Implementation of Protocol Buffer over CAN interface
 */

#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifdef PLATFORM_ARDUINO
#include <Arduino.h>
#endif

// Debug mode
#ifndef DEBUG_MODE
#define DEBUG_MODE 0
#endif

// Constructor
ProtobufCANInterface::ProtobufCANInterface(uint32_t nodeId)
    : m_nodeId(nodeId), m_numHandlers(0)
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
    return m_canInterface.begin(baudRate, canDevice);
}

void ProtobufCANInterface::registerHandler(kart_common_MessageType msg_type,
                                          kart_common_ComponentType type, 
                                          uint8_t component_id, 
                                          uint8_t command_id, 
                                          MessageHandler handler)
{
    if (m_numHandlers >= MAX_HANDLERS) {
        logMessage("MAX_HANDLERS_ERROR", 0, 0);
        return;
    }

    m_handlers[m_numHandlers].msg_type = msg_type;
    m_handlers[m_numHandlers].type = type;
    m_handlers[m_numHandlers].component_id = component_id;
    m_handlers[m_numHandlers].command_id = command_id;
    m_handlers[m_numHandlers].handler = handler;
    m_numHandlers++;
    
#if DEBUG_MODE
    logMessage("REGD", packMessageId(msg_type, type, component_id, command_id, kart_common_ValueType_BOOLEAN), 0);
#endif
}

bool ProtobufCANInterface::sendMessage(kart_common_MessageType message_type, 
                                      kart_common_ComponentType component_type,
                                      uint8_t component_id, uint8_t command_id, 
                                      kart_common_ValueType value_type, int32_t value) {
  
    // Pack message ID into CAN ID
    uint16_t message_id = packMessageId(message_type, component_type, component_id, command_id, value_type);
    
    // Prepare CAN message
    CANMessage msg;
    msg.id = message_id;
    
    // Pack value into data field
    packValue(value_type, value, msg.data, msg.length);
    
#if DEBUG_MODE
    logMessage("SEND", message_id, value);
    printf("ProtobufCANInterface: Created CAN frame - ID: 0x%X, Length: %d, Data:", msg.id, msg.length);
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

void ProtobufCANInterface::process()
{
    CANMessage msg;
    
    // Try to receive a message
    if (!m_canInterface.receiveMessage(msg)) {
        return; // No message available
    }

#ifdef PLATFORM_ARDUINO
    // On Arduino, drop all status messages immediately
    if ((msg.id >> MSG_TYPE_SHIFT) & MSG_TYPE_MASK) { // If MSB is 1, it's a status message
        return;
    }
#endif
    
    // Unpack message ID
    kart_common_MessageType msg_type;
    kart_common_ComponentType comp_type;
    uint8_t component_id;
    uint8_t command_id;
    kart_common_ValueType value_type;
    unpackMessageId(msg.id, msg_type, comp_type, component_id, command_id, value_type);
    
    // Unpack value
    int32_t value = unpackValue(value_type, msg.data, msg.length);
    
#if DEBUG_MODE
    logMessage("RECV", msg.id, value);
#endif

    // Find and execute matching handlers
    bool handlerFound = false;
    for (int i = 0; i < m_numHandlers; i++) {
        if (matchesHandler(m_handlers[i], msg_type, comp_type, component_id, command_id)) {
            handlerFound = true;
            m_handlers[i].handler(value);
        }
    }

    // Echo status if this was a command (optional)
    if (handlerFound && msg_type == kart_common_MessageType_COMMAND) {
        sendMessage(kart_common_MessageType_STATUS, comp_type, component_id, command_id, value_type, value);
    }
}

uint16_t ProtobufCANInterface::packMessageId(kart_common_MessageType msg_type,
                                           kart_common_ComponentType comp_type,
                                           uint8_t component_id,
                                           uint8_t command_id,
                                           kart_common_ValueType value_type)
{
    uint16_t id = 0;
    id |= (static_cast<uint16_t>(msg_type) & MSG_TYPE_MASK) << MSG_TYPE_SHIFT;
    id |= (static_cast<uint16_t>(comp_type) & COMP_TYPE_MASK) << COMP_TYPE_SHIFT;
    id |= (static_cast<uint16_t>(component_id) & COMP_ID_MASK) << COMP_ID_SHIFT;
    id |= (static_cast<uint16_t>(command_id) & CMD_ID_MASK) << CMD_ID_SHIFT;
    id |= (static_cast<uint16_t>(value_type) & VALUE_TYPE_MASK) << VALUE_TYPE_SHIFT;
    return id;
}

void ProtobufCANInterface::unpackMessageId(uint16_t message_id,
                                         kart_common_MessageType& msg_type,
                                         kart_common_ComponentType& comp_type,
                                         uint8_t& component_id,
                                         uint8_t& command_id,
                                         kart_common_ValueType& value_type)
{
    msg_type = static_cast<kart_common_MessageType>((message_id >> MSG_TYPE_SHIFT) & MSG_TYPE_MASK);
    comp_type = static_cast<kart_common_ComponentType>((message_id >> COMP_TYPE_SHIFT) & COMP_TYPE_MASK);
    component_id = static_cast<uint8_t>((message_id >> COMP_ID_SHIFT) & COMP_ID_MASK);
    command_id = static_cast<uint8_t>((message_id >> CMD_ID_SHIFT) & CMD_ID_MASK);
    value_type = static_cast<kart_common_ValueType>((message_id >> VALUE_TYPE_SHIFT) & VALUE_TYPE_MASK);
}

uint8_t ProtobufCANInterface::getValueLength(kart_common_ValueType type)
{
    switch (type) {
        case kart_common_ValueType_BOOLEAN:
            return 1;
        case kart_common_ValueType_UINT8:
            return 1;
        case kart_common_ValueType_UINT16:
            return 2;
        case kart_common_ValueType_FLOAT16:
            return 2;
        default:
            return 0;
    }
}

void ProtobufCANInterface::packValue(kart_common_ValueType type, int32_t value, uint8_t* data, uint8_t& length)
{
    length = getValueLength(type);
    switch (type) {
        case kart_common_ValueType_BOOLEAN:
            data[0] = value ? 1 : 0;
            break;
        case kart_common_ValueType_UINT8:
            data[0] = static_cast<uint8_t>(value);
            break;
        case kart_common_ValueType_UINT16:
            data[0] = (value >> 8) & 0xFF;
            data[1] = value & 0xFF;
            break;
        case kart_common_ValueType_FLOAT16: {
            // Convert float to 16-bit fixed point
            int16_t fixed = static_cast<int16_t>(value * 100.0f);
            data[0] = (fixed >> 8) & 0xFF;
            data[1] = fixed & 0xFF;
            break;
        }
        default:
            length = 0;
            break;
    }
}

int32_t ProtobufCANInterface::unpackValue(kart_common_ValueType type, const uint8_t* data, uint8_t length)
{
    switch (type) {
        case kart_common_ValueType_BOOLEAN:
            return data[0] ? 1 : 0;
        case kart_common_ValueType_UINT8:
            return static_cast<uint8_t>(data[0]);
        case kart_common_ValueType_UINT16:
            return (static_cast<uint16_t>(data[0]) << 8) | data[1];
        case kart_common_ValueType_FLOAT16: {
            // Convert 16-bit fixed point to float
            int16_t fixed = (static_cast<int16_t>(data[0]) << 8) | data[1];
            return static_cast<int32_t>(fixed / 100.0f);
        }
        default:
            return 0;
    }
}

void ProtobufCANInterface::logMessage(const char *prefix, uint16_t message_id, int32_t value)
{
#ifdef PLATFORM_ARDUINO
    char buffer[128];
    snprintf(buffer, sizeof(buffer), 
             "%s: ID=0x%04X, Val=%ld", 
             prefix, static_cast<unsigned int>(message_id), static_cast<long>(value));
    Serial.println(buffer);
#else
    printf("%s: ID=0x%04X, Val=%d\n", 
           prefix, static_cast<unsigned int>(message_id), static_cast<int>(value));
#endif
}

bool ProtobufCANInterface::matchesHandler(const HandlerEntry& handler,
                                        kart_common_MessageType msg_type,
                                        kart_common_ComponentType comp_type,
                                        uint8_t component_id,
                                        uint8_t command_id)
{
    // Check if message type matches
    if (handler.msg_type != msg_type) {
        return false;
    }

    // Check if component type matches
    if (handler.type != comp_type) {
        return false;
    }

    // Check if component ID matches (0xFF means match all)
    if (handler.component_id != 0xFF && handler.component_id != component_id) {
        return false;
    }

    // Check if command ID matches (0xFF means match all)
    if (handler.command_id != 0xFF && handler.command_id != command_id) {
        return false;
    }

    return true;
}
