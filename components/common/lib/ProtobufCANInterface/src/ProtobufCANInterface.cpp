#include "ProtobufCANInterface.h"
#include "CAN.h"
#include "common.pb.h"
#include <string.h>

// todo: conditionally include component_type specific pb files based on defined component type, 
//       then use all of the loaded pb files to validate the incoming/outgoing messages

// Constructor
ProtobufCANInterface::ProtobufCANInterface(uint32_t nodeId)
    : nodeId(nodeId), num_handlers(0)
{
    // Constructor implementation
}

// Initialize CAN interface
bool ProtobufCANInterface::begin(long baudrate)
{
    if (!CAN.begin(baudrate)) {
        return false;
    }

#if defined(DEBUG_MODE) && DEBUG_MODE
    CAN.loopback();
#endif

    return true;
}

void ProtobufCANInterface::registerHandler(kart_common_ComponentType type, 
                                          uint8_t component_id, 
                                          uint8_t command_id, 
                                          MessageHandler handler)
{
    if (num_handlers < MAX_HANDLERS) {
        handlers[num_handlers].type = type;
        handlers[num_handlers].component_id = component_id;
        handlers[num_handlers].command_id = command_id;
        handlers[num_handlers].handler = handler;
        num_handlers++;
        
#if defined(DEBUG_MODE) && DEBUG_MODE
        logMessage("REGD", kart_common_MessageType_COMMAND, type, component_id, 
                  command_id, kart_common_ValueType_BOOLEAN, false);
#endif
    }
}

bool ProtobufCANInterface::sendMessage(kart_common_MessageType message_type, 
                                      kart_common_ComponentType component_type,
                                      uint8_t component_id, 
                                      uint8_t command_id, 
                                      kart_common_ValueType value_type, 
                                      int32_t value)
{
#if defined(DEBUG_MODE) && DEBUG_MODE
    logMessage("SEND", message_type, component_type, component_id, command_id, value_type, value);
#endif

    // Pack first byte (message type and component type)
    uint8_t header = packHeader(message_type, component_type);

    // Pack the value based on its type
    uint32_t packed_value = packValue(value_type, value);

    // Prepare CAN message (8 bytes)
    uint8_t data[8];
    data[0] = header;
    data[1] = 0; // Reserved
    data[2] = component_id;
    data[3] = command_id;
    data[4] = static_cast<uint8_t>(value_type) << 4; // Value type in high nibble
    data[5] = (packed_value >> 16) & 0xFF;           // Value byte 2 (MSB)
    data[6] = (packed_value >> 8) & 0xFF;            // Value byte 1
    data[7] = packed_value & 0xFF;                   // Value byte 0 (LSB)

    // Send the CAN message using CAN library
    CAN.beginPacket(0); // Use ID 0 for simplicity
    CAN.write(data, 8);
    return CAN.endPacket();
}

void ProtobufCANInterface::process()
{
    if (!CAN.parsePacket()) {
        return; // No packets available
    }

    // Read incoming message
    uint8_t data[8];
    for (int i = 0; i < 8 && CAN.available(); i++) {
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

    // Ignore status messages to prevent loops
    if (msg_type == kart_common_MessageType_STATUS) {
        return;
    }

#if defined(DEBUG_MODE) && DEBUG_MODE
    logMessage("RECV", msg_type, comp_type, component_id, command_id, value_type, value);
#endif

    // Find and execute matching handlers
    bool handlerFound = false;
    for (int i = 0; i < num_handlers; i++) {
        if ((handlers[i].type == comp_type) &&
            (handlers[i].component_id == component_id || handlers[i].component_id == 0xFF) &&
            (handlers[i].command_id == command_id)) {
            
            handlerFound = true;
            handlers[i].handler(msg_type, comp_type, component_id, command_id, value_type, value);
        }
    }

    // Echo status if this was a command
    if (msg_type == kart_common_MessageType_COMMAND)
        sendMessage(kart_common_MessageType_STATUS, comp_type, component_id, command_id, value_type, value);
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

#if defined(DEBUG_MODE) && DEBUG_MODE
void ProtobufCANInterface::logMessage(const char* prefix, kart_common_MessageType message_type, 
                                     kart_common_ComponentType component_type,
                                     uint8_t component_id, uint8_t command_id, 
                                     kart_common_ValueType value_type, int32_t value)
{
    // Implement logging for debug mode
}
#endif

// C API implementations for CFFI
extern "C" {

ProtobufCANInterface* create_can_interface(uint32_t nodeId) {
    return new ProtobufCANInterface(nodeId);
}

void destroy_can_interface(ProtobufCANInterface* instance) {
    delete instance;
}

bool can_interface_begin(ProtobufCANInterface* instance, long baudrate) {
    return instance->begin(baudrate);
}

void can_interface_register_handler(ProtobufCANInterface* instance, 
                                   int component_type, uint8_t component_id, 
                                   uint8_t command_id, 
                                   void (*handler)(int, int, uint8_t, uint8_t, int, int32_t)) {
    instance->registerHandler(
        static_cast<kart_common_ComponentType>(component_type),
        component_id,
        command_id,
        reinterpret_cast<ProtobufCANInterface::MessageHandler>(handler)
    );
}

bool can_interface_send_message(ProtobufCANInterface* instance, 
                               int message_type, int component_type, 
                               uint8_t component_id, uint8_t command_id, 
                               int value_type, int32_t value) {
    return instance->sendMessage(
        static_cast<kart_common_MessageType>(message_type),
        static_cast<kart_common_ComponentType>(component_type),
        component_id,
        command_id,
        static_cast<kart_common_ValueType>(value_type),
        value
    );
}

void can_interface_process(ProtobufCANInterface* instance) {
    instance->process();
}

uint8_t can_interface_pack_header(int type, int component) {
    return ProtobufCANInterface::packHeader(
        static_cast<kart_common_MessageType>(type),
        static_cast<kart_common_ComponentType>(component)
    );
}

void can_interface_unpack_header(uint8_t header, int* type, int* component) {
    kart_common_MessageType msg_type;
    kart_common_ComponentType comp_type;
    
    ProtobufCANInterface::unpackHeader(header, msg_type, comp_type);
    
    *type = static_cast<int>(msg_type);
    *component = static_cast<int>(comp_type);
}

uint32_t can_interface_pack_value(int type, int32_t value) {
    return ProtobufCANInterface::packValue(
        static_cast<kart_common_ValueType>(type),
        value
    );
}

int32_t can_interface_unpack_value(int type, uint32_t packed_value) {
    return ProtobufCANInterface::unpackValue(
        static_cast<kart_common_ValueType>(type),
        packed_value
    );
}

} // extern "C"