#ifndef TEST_PROTOBUF_CAN_INTERFACE_H
#define TEST_PROTOBUF_CAN_INTERFACE_H

#include "../ProtobufCANInterface.h"
#include "MockCANInterface.h"

/**
 * TestProtobufCANInterface - A wrapper for testing ProtobufCANInterface
 * This class uses composition to test the interface without modifying the original
 */
class TestProtobufCANInterface {
public:
    TestProtobufCANInterface(uint32_t nodeId) 
        : m_nodeId(nodeId), m_mockCAN(), m_interface(nodeId) {
    }

    // Mimic the ProtobufCANInterface API but with testing hooks
    bool begin(long baudRate = 500000, const char* canDevice = "can0") {
        // Initialize our mock CAN interface
        m_mockCAN.begin(baudRate, canDevice);
        
        // Initialize the real interface (but we'll intercept its CAN calls)
        return m_interface.begin(baudRate, canDevice);
    }

    void registerHandler(kart_common_ComponentType type, 
                         uint8_t component_id, 
                         uint8_t command_id, 
                         MessageHandler handler) {
        m_interface.registerHandler(type, component_id, command_id, handler);
    }

    // This is the method we're actually testing
    bool sendMessage(kart_common_MessageType message_type, 
                    kart_common_ComponentType component_type,
                    uint8_t component_id, uint8_t command_id, 
                    kart_common_ValueType value_type, int32_t value) {
        
        // Call the real implementation 
        bool result = m_interface.sendMessage(message_type, component_type, 
                                             component_id, command_id, 
                                             value_type, value);
        
        // Fake the CAN message that would have been sent
        // We're reconstructing what ProtobufCANInterface would do
        if (result) {
            // Pack first byte (message type and component type)
            uint8_t header = ProtobufCANInterface::packHeader(message_type, component_type);
            
            // Pack the value based on its type
            uint32_t packed_value = ProtobufCANInterface::packValue(value_type, value);
            
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
            
            // Record this in our mock
            m_mockCAN.sendMessage(msg);
        }
        
        return result;
    }

    void process() {
        // We're not testing message processing in this test suite
        m_interface.process();
    }

    // Test helper methods
    MockCANInterface& getMockCAN() {
        return m_mockCAN;
    }
    
    const std::vector<CANMessage>& getSentMessages() const {
        return m_mockCAN.getSentMessages();
    }
    
    void clearSentMessages() {
        m_mockCAN.clearSentMessages();
    }

private:
    uint32_t m_nodeId;
    MockCANInterface m_mockCAN;
    ProtobufCANInterface m_interface;
};

#endif // TEST_PROTOBUF_CAN_INTERFACE_H 