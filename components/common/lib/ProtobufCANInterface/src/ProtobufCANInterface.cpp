#include "ProtobufCANInterface.h"

#if IS_ARDUINO_ENV
#include "CAN.h"
#include "common.pb.h"
#endif

// Constructor
ProtobufCANInterface::ProtobufCANInterface(uint32_t nodeId)
    : nodeId(nodeId) 
{
#if IS_ARDUINO_ENV
    // Arduino-specific initialization
#else
    DEBUG_PRINTLN("ProtobufCANInterface initialized with node ID: " + std::to_string(nodeId));
#endif
}

// Initialize CAN interface
bool ProtobufCANInterface::begin(long baudrate)
{
#if IS_ARDUINO_ENV
    if (!CAN.begin(baudrate))
    {
        Serial.println("CAN initialization failure.");
        return false;
    }

#if defined(DEBUG_MODE) && DEBUG_MODE
    CAN.loopback();
#endif

    Serial.println("CAN initialization success.");
    return true;
#else
    // Non-Arduino implementation
    DEBUG_PRINTLN("CAN initialization with baudrate: " + std::to_string(baudrate));
    return true;
#endif
}

void ProtobufCANInterface::registerHandler(kart_common_ComponentType type, uint8_t component_id, uint8_t command_id, MessageHandler handler)
{
    if (num_handlers < MAX_HANDLERS)
    {
        handlers[num_handlers] = {type, component_id, command_id, handler};
        num_handlers++;
        
#if defined(DEBUG_MODE) && DEBUG_MODE
        logMessage("REGD", kart_common_MessageType_COMMAND, type, component_id, command_id, kart_common_ValueType_BOOLEAN, false);
#endif
    }
    else
    {
#if IS_ARDUINO_ENV
        Serial.println(F("ERROR: Cannot register handler, MAX_HANDLERS reached"));
#else
        DEBUG_PRINTLN("ERROR: Cannot register handler, MAX_HANDLERS reached");
#endif
    }
}

bool ProtobufCANInterface::sendMessage(kart_common_MessageType message_type, kart_common_ComponentType component_type,
                                       uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value)
{
#if defined(DEBUG_MODE) && DEBUG_MODE
    logMessage("SEND", message_type, component_type, component_id, command_id, value_type, value);
#endif

    // Pack first two bytes (kart_common_MessageType and kart_common_ComponentType)
    uint8_t header = packHeader(message_type, component_type);

    // Pack the value based on its type
    uint32_t packed_value = packValue(value_type, value);

#if IS_ARDUINO_ENV
    // Prepare CAN message (8 bytes)
    uint8_t data[8];
    data[0] = header;
    data[1] = 0; // Reserved
    data[2] = component_id;
    data[3] = command_id;
    data[4] = static_cast<uint8_t>(value_type) << 4; // kart_common_ValueType in high nibble
    data[5] = (packed_value >> 16) & 0xFF;           // Value byte 2 (MSB)
    data[6] = (packed_value >> 8) & 0xFF;            // Value byte 1
    data[7] = packed_value & 0xFF;                   // Value byte 0 (LSB)

    // Send the CAN message
    CAN.beginPacket(0); // Use ID 0 for simplicity
    CAN.write(data, 8);
    bool result = CAN.endPacket();
    
    if (!result) {
        Serial.println(F("ERROR: Failed to send CAN message"));
    }
    
    return result;
#else
    // Non-Arduino implementation - just log the message
    DEBUG_PRINTLN("CAN message: header=" + std::to_string(header) + 
                 ", component_id=" + std::to_string(component_id) + 
                 ", command_id=" + std::to_string(command_id) + 
                 ", value_type=" + std::to_string(static_cast<int>(value_type)) + 
                 ", value=" + std::to_string(value) +
                 ", packed_value=" + std::to_string(packed_value));
    return true;
#endif
}

void ProtobufCANInterface::process()
{
#if IS_ARDUINO_ENV
    if (!CAN.parsePacket())
    {
        // No packets available
        return;
    }

    // Read incoming message
    uint8_t data[8];
    for (int i = 0; i < 8 && CAN.available(); i++)
    {
        data[i] = CAN.read();
    }

    // Unpack header
    kart_common_MessageType msg_type;
    kart_common_ComponentType comp_type;
    unpackHeader(data[0], msg_type, comp_type);

    // Extract other fields
    uint8_t component_id = data[2];
    uint8_t command_id = data[3];
    kart_common_ValueType value_type = static_cast<kart_common_ValueType>(data[4] >> 4);

    // Unpack value
    uint32_t packed_value = (static_cast<uint32_t>(data[5]) << 16) |
                            (static_cast<uint32_t>(data[6]) << 8) |
                            data[7];
    int32_t value = unpackValue(value_type, packed_value);

    if (msg_type == kart_common_MessageType_STATUS) {
        return;
    }

#if defined(DEBUG_MODE) && DEBUG_MODE
    logMessage("RECV", msg_type, comp_type, component_id, command_id, value_type, value);
#endif

    // Find and execute matching handlers
    bool handlerFound = false;
    for (int i = 0; i < num_handlers; i++)
    {
        if ((handlers[i].type == comp_type) &&
            (handlers[i].component_id == component_id || handlers[i].component_id == 0xFF) &&
            (handlers[i].command_id == command_id))
        {
            Serial.println(F("Executing handler"));
            handlerFound = true;
            handlers[i].handler(msg_type, comp_type, component_id, command_id, value_type, value);
        }
    }

    if (!handlerFound)
        Serial.println(F("No matching handler found"));

    // Echo status if this was a command
    if (msg_type == kart_common_MessageType_COMMAND)
        sendMessage(kart_common_MessageType_STATUS, comp_type, component_id, command_id, value_type, value);
#else
    // Non-Arduino implementation - just a stub
    // In a real implementation, we would check for messages from a CAN interface
#endif
}

uint8_t ProtobufCANInterface::packHeader(kart_common_MessageType type, kart_common_ComponentType component)
{
    return (static_cast<uint8_t>(type) << 6) | (static_cast<uint8_t>(component) & 0x3F);
}

void ProtobufCANInterface::unpackHeader(uint8_t header, kart_common_MessageType &type, kart_common_ComponentType &component)
{
    type = static_cast<kart_common_MessageType>((header >> 6) & 0x03);
    component = static_cast<kart_common_ComponentType>(header & 0x3F);
}

uint32_t ProtobufCANInterface::packValue(kart_common_ValueType type, int32_t value)
{
    // Truncate and mask based on kart_common_ValueType
    switch (type)
    {
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
    switch (type)
    {
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

#if defined(DEBUG_MODE) && DEBUG_MODE
void ProtobufCANInterface::logMessage(const char* prefix, kart_common_MessageType message_type, 
                                     kart_common_ComponentType component_type,
                                     uint8_t component_id, uint8_t command_id, 
                                     kart_common_ValueType value_type, int32_t value)
{
#if IS_ARDUINO_ENV
    // Log message type name
    Serial.print(prefix);
    Serial.print(F(" - Type: "));
    switch(message_type) {
        case kart_common_MessageType_COMMAND: Serial.print(F("COMMAND")); break;
        case kart_common_MessageType_STATUS: Serial.print(F("STATUS")); break;
        case kart_common_MessageType_ACK: Serial.print(F("ACK")); break;
        case kart_common_MessageType_ERROR: Serial.print(F("ERROR")); break;
        default: Serial.print(static_cast<int>(message_type)); break;
    }
    
    // Log component type name
    Serial.print(F(", CompType: "));
    switch(component_type) {
        case kart_common_ComponentType_LIGHTS: Serial.print(F("LIGHTS")); break;
        case kart_common_ComponentType_MOTORS: Serial.print(F("MOTORS")); break;
        case kart_common_ComponentType_SENSORS: Serial.print(F("SENSORS")); break;
        case kart_common_ComponentType_BATTERY: Serial.print(F("BATTERY")); break;
        case kart_common_ComponentType_CONTROLS: Serial.print(F("CONTROLS")); break;
        default: Serial.print(static_cast<int>(component_type)); break;
    }
    
    // Log component ID and rest of the data
    Serial.print(F(", CompID: "));
    Serial.print(component_id);
    Serial.print(F(", CmdID: "));
    Serial.print(command_id);
    Serial.print(F(", ValueType: "));
    Serial.print(static_cast<int>(value_type));
    Serial.print(F(", Value: "));
    Serial.println(value);
#else
    // Non-Arduino implementation
    std::string msg_type_str;
    switch(message_type) {
        case kart_common_MessageType_COMMAND: msg_type_str = "COMMAND"; break;
        case kart_common_MessageType_STATUS: msg_type_str = "STATUS"; break;
        case kart_common_MessageType_ACK: msg_type_str = "ACK"; break;
        case kart_common_MessageType_ERROR: msg_type_str = "ERROR"; break;
        default: msg_type_str = std::to_string(static_cast<int>(message_type)); break;
    }
    
    std::string comp_type_str;
    switch(component_type) {
        case kart_common_ComponentType_LIGHTS: comp_type_str = "LIGHTS"; break;
        case kart_common_ComponentType_MOTORS: comp_type_str = "MOTORS"; break;
        case kart_common_ComponentType_SENSORS: comp_type_str = "SENSORS"; break;
        case kart_common_ComponentType_BATTERY: comp_type_str = "BATTERY"; break;
        case kart_common_ComponentType_CONTROLS: comp_type_str = "CONTROLS"; break;
        default: comp_type_str = std::to_string(static_cast<int>(component_type)); break;
    }
    
    std::string log_message = std::string(prefix) + 
                             " - Type: " + msg_type_str +
                             ", CompType: " + comp_type_str +
                             ", CompID: " + std::to_string(component_id) +
                             ", CmdID: " + std::to_string(command_id) +
                             ", ValueType: " + std::to_string(static_cast<int>(value_type)) +
                             ", Value: " + std::to_string(value);
                             
    DEBUG_PRINTLN(log_message);
#endif
}
#endif