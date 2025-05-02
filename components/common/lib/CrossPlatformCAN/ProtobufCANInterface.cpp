/**
 * ProtobufCANInterface.cpp - Implementation of Protocol Buffer over CAN interface
 */

#include "ProtobufCANInterface.h"
#include <stdio.h>
#include <string.h>
#include <chrono> // Already included via .h implicitly, but good practice
#include "common.pb.h" // Include generated protobuf definitions
#include "system_monitor.pb.h" // Include system monitor definitions for CommandId

#ifdef PLATFORM_ARDUINO
#include <Arduino.h>
#endif

#ifdef PLATFORM_ESP32 // Guard for ESP32-specific time functions
#include <sys/time.h> // Required for timeval, settimeofday, gettimeofday
#include <time.h>     // Required for time_t
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
                                      kart_common_ValueType value_type, int32_t value,
                                      int8_t delay_override,
                                      uint32_t destination_node_id /* Added */) {

    uint8_t final_delta_byte = 0;

    // Check if this is a PING command with a valid override
    bool is_ping_with_override = (message_type == kart_common_MessageType_COMMAND &&
                                component_type == kart_common_ComponentType_SYSTEM_MONITOR &&
                                command_id == kart_system_monitor_SystemMonitorCommandId_PING &&
                                delay_override >= 0);

    if (is_ping_with_override) {
        // Use the provided override for PING messages
        final_delta_byte = static_cast<uint8_t>(delay_override);
#if CAN_LOGGING_ENABLED || DEBUG_MODE
        printf("[sendMessage] Using delay_override %d for PING data[1].\n", final_delta_byte);
#endif
    } else {
        // Calculate Timestamp Delta based on internal state for other messages
        uint64_t nowMs = getCurrentTimeMs();
        uint64_t deltaMs = 0;

        if (m_lastSyncTimeMs > 0) {
             if (nowMs >= m_lastSyncTimeMs) {
                 deltaMs = nowMs - m_lastSyncTimeMs;
             } else {
                 // Clock potentially went backwards or very large delta - clamp to 0?
                 deltaMs = 0; 
#if CAN_LOGGING_ENABLED || DEBUG_MODE
                 printf("[sendMessage] WARNING: nowMs < m_lastSyncTimeMs! Clamping delta to 0. now=%llu, last=%llu\n", 
                        (unsigned long long)nowMs, (unsigned long long)m_lastSyncTimeMs);
#endif
             }
        } 

        final_delta_byte = static_cast<uint8_t>(deltaMs & 0xFF);
#if CAN_LOGGING_ENABLED || DEBUG_MODE
        printf("[sendMessage] Calculated delta %u for data[1]. now=%llu, last=%llu\n", 
            final_delta_byte, (unsigned long long)nowMs, (unsigned long long)m_lastSyncTimeMs);
#endif
    }

    // Pack first byte (message type and component type)
    uint8_t header = packHeader(message_type, component_type);
    
    // Pack the value based on its type
    uint32_t packed_value = packValue(value_type, value);
    
    // Prepare CAN message (8 bytes)
    CANMessage msg;
    
    // --- Set CAN ID based on destination --- ADDED
    if (destination_node_id == UINT32_MAX) {
        msg.id = m_nodeId; // Use sender's ID if no specific destination
    } else {
        msg.id = destination_node_id; // Use the provided destination ID
    }
    // --- END ADDED ---
    
    msg.length = 8;      // Always use 8 bytes for consistency
    
    msg.data[0] = header;
    msg.data[1] = final_delta_byte; // Use the determined delta byte
    msg.data[2] = component_id;
    msg.data[3] = command_id;
    msg.data[4] = static_cast<uint8_t>(value_type) << 4; // Value type in high nibble
    msg.data[5] = (packed_value >> 16) & 0xFF;           // Value byte 2 (MSB)
    msg.data[6] = (packed_value >> 8) & 0xFF;            // Value byte 1
    msg.data[7] = packed_value & 0xFF;                   // Value byte 0 (LSB)
    
#if CAN_LOGGING_ENABLED
    printf("ProtobufCANInterface: Final CAN frame - ID: 0x%X, Data:", msg.id);
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

    // Peek at message type and command ID first for special handling
    kart_common_MessageType msg_type;
    kart_common_ComponentType comp_type;
    unpackHeader(msg.data[0], msg_type, comp_type);
    uint8_t command_id = msg.data[3];

    // --- Handle Time Sync Commands --- 
    if (comp_type == kart_common_ComponentType_SYSTEM_MONITOR) {
        if (msg_type == kart_common_MessageType_COMMAND && command_id == kart_system_monitor_SystemMonitorCommandId_PING) {
            return _handlePing(msg);
        }
        else if (msg_type == kart_common_MessageType_COMMAND && command_id == kart_system_monitor_SystemMonitorCommandId_SET_TIME) {
            return _handleSetTime(msg);
        }
    }
    // --- End Time Sync Handling ---

    // --- If not a handled time sync command, continue with regular message processing ---
    uint32_t source_node_id = msg.id; // Get source node ID
    uint8_t component_id = msg.data[2];
    uint8_t timestamp_delta = msg.data[1];
    kart_common_ValueType value_type = static_cast<kart_common_ValueType>(msg.data[4] >> 4);

    // Unpack value
    uint32_t packed_value = (static_cast<uint32_t>(msg.data[5]) << 16) |
                           (static_cast<uint32_t>(msg.data[6]) << 8) |
                           msg.data[7];
    int32_t value = unpackValue(value_type, packed_value);

#if CAN_LOGGING_ENABLED
    logMessage("RECV", msg_type, comp_type, component_id, command_id, value_type, value);
#endif

    // Find and execute matching handlers
    bool handlerFound = false;
    for (int i = 0; i < m_numHandlers; i++) {
        if (!matchesHandler(m_handlers[i], msg_type, comp_type, component_id, command_id)) {
            continue;
        }

        handlerFound = true;
        m_handlers[i].handler(source_node_id, msg_type, comp_type, component_id, command_id, value_type, value, timestamp_delta);
    }

    // Echo status if this was a command (optional, and NOT for SET_TIME/PING)
    // This echo logic might need refinement depending on desired behavior.
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

// Implementation of the ping handling helper function
bool ProtobufCANInterface::_handlePing(const CANMessage& msg) {
    // Extract the timestamp value sent by the collector (RPi)
    uint32_t packed_ping_value = (static_cast<uint32_t>(msg.data[5]) << 16) |
                                 (static_cast<uint32_t>(msg.data[6]) << 8) |
                                 msg.data[7];
    int32_t ping_timestamp_value = unpackValue(kart_common_ValueType_UINT24, packed_ping_value);

#if DEBUG_MODE || CAN_LOGGING_ENABLED
    printf("ProtobufCANInterface: Received PING from Node 0x%X with value %d. Sending PONG.\n",
           msg.id, ping_timestamp_value);
#endif

    // Send PONG back immediately, echoing the original PING value
    uint8_t pong_component_id = 0; // Placeholder - Should ideally be this device's component ID if applicable
    return sendMessage(kart_common_MessageType_STATUS,
                kart_common_ComponentType_SYSTEM_MONITOR,
                pong_component_id, // ID of this device
                kart_system_monitor_SystemMonitorCommandId_PONG,
                kart_common_ValueType_UINT24, // Echo same type
                ping_timestamp_value);        // Echo original value
}

// Implementation of the SET_TIME command handler
bool ProtobufCANInterface::_handleSetTime(const CANMessage& msg) {
    // Extract the target timestamp value sent by the collector
    uint32_t packed_target_time = (static_cast<uint32_t>(msg.data[5]) << 16) |
                                 (static_cast<uint32_t>(msg.data[6]) << 8) |
                                 msg.data[7];
    int32_t target_time_ms_24bit = unpackValue(kart_common_ValueType_UINT24, packed_target_time);

#if DEBUG_MODE || CAN_LOGGING_ENABLED
    printf("ProtobufCANInterface: Received SET_TIME command from Node 0x%X with target_time (24-bit ms) %d.\n",
           msg.id, target_time_ms_24bit);
#endif

#ifdef PLATFORM_ESP32
    struct timeval current_esp_tv;
    if (gettimeofday(&current_esp_tv, NULL) != 0) {

#if DEBUG_MODE || CAN_LOGGING_ENABLED
        printf("ProtobufCANInterface: WARNING - Failed to get current ESP32 time (gettimeofday failed) in SET_TIME handler.\n");
#endif

        return false;
    }

    // Create the new timeval: Keep current seconds, adjust microseconds
    struct timeval new_esp_tv;
    new_esp_tv.tv_sec = current_esp_tv.tv_sec; // Keep the current second
    // Adjust microseconds based on the fractional second part of target_time_ms_24bit
    new_esp_tv.tv_usec = (target_time_ms_24bit % 1000) * 1000;

    // Set the ESP32's time
    if (settimeofday(&new_esp_tv, NULL) != 0) {
#if DEBUG_MODE || CAN_LOGGING_ENABLED
        printf("ProtobufCANInterface: WARNING - Failed to set ESP32 time via SET_TIME command.\n");
#endif
        // If setting time failed, DO NOT update m_lastSyncTimeMs
        return false; // Indicate failure
    } else {
#if DEBUG_MODE || CAN_LOGGING_ENABLED
        printf("ProtobufCANInterface: Successfully set ESP32 time via SET_TIME - Sec: %ld, uSec: %06ld (from target_ms %d).\n",
                (long)new_esp_tv.tv_sec, (long)new_esp_tv.tv_usec, target_time_ms_24bit);
#endif
        // *** Update m_lastSyncTimeMs with the full time that was SET ***
        uint64_t full_set_time_ms = (uint64_t)new_esp_tv.tv_sec * 1000 + (new_esp_tv.tv_usec / 1000);
        m_lastSyncTimeMs = full_set_time_ms;
        return true; // Indicate success
    }
#else // Not PLATFORM_ESP32
    // Ignore SET_TIME command on non-ESP32 platforms
    return false;
#endif // PLATFORM_ESP32
}

// --- Helper Function Implementation ---
uint64_t ProtobufCANInterface::getCurrentTimeMs() {
#ifdef PLATFORM_ESP32
    // Use gettimeofday for ESP32 as we are setting it via PING+delay
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        return (uint64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
    } else {
        // Fallback or error handling
        #if DEBUG_MODE || CAN_LOGGING_ENABLED
            printf("ProtobufCANInterface: WARNING - Failed to get ESP32 time via gettimeofday in getCurrentTimeMs! Falling back to millis().\n");
        #endif
        #ifdef PLATFORM_ARDUINO // Check if Arduino functions are available
             return millis(); 
        #else
             return 0; // Or another error indicator if no millis()
        #endif
    }
#elif defined(PLATFORM_ARDUINO) 
    // Use Arduino's millis() for non-ESP32 Arduino platforms
    return millis();
#else
    // Use std::chrono for Linux/macOS etc.
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch(); 
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
#endif
} 
