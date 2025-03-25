/**
 * header_test.cpp - Test the packHeader and unpackHeader functions
 * 
 * This test verifies that:
 * 1. packHeader correctly encodes message types and component types
 * 2. unpackHeader correctly decodes these values
 * 3. The fix for the component type masking issue (& 3 vs & 7) works
 *
 * Compilation:
 * g++ -o header_test header_test.cpp ../ProtobufCANInterface.cpp ../CANInterface.cpp \
 *     -I.. -I../include -DPLATFORM_LINUX -std=c++11
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "ProtobufCANInterface.h"

// Mock CAN interface for testing
class MockCANInterface : public CANInterface {
public:
    MockCANInterface() {}
    
    bool begin(long baudRate, const char* device = nullptr) override { return true; }
    bool sendMessage(const CANMessage& msg) override { return true; }
    bool receiveMessage(CANMessage& msg) override { return false; }
};

// Extend ProtobufCANInterface to expose protected methods for testing
class TestableProtobufCANInterface : public ProtobufCANInterface {
public:
    TestableProtobufCANInterface() : ProtobufCANInterface(1) {}
    
    // Make packHeader and unpackHeader accessible
    using ProtobufCANInterface::packHeader;
    using ProtobufCANInterface::unpackHeader;
    
    // Override to use our mock CAN interface
    bool begin(long baudRate = 500000, const char* device = nullptr) override {
        return true;
    }
};

// Helper function to print binary representation
void printBinary(uint8_t value) {
    for (int i = 7; i >= 0; i--) {
        printf("%d", (value >> i) & 1);
    }
}

// Test that packHeader correctly encodes message type and component type
void testPackHeader() {
    TestableProtobufCANInterface interface;
    
    printf("Testing packHeader function...\n");
    
    // Test all message types (2 bits)
    const kart_common_MessageType messageTypes[] = {
        kart_common_MessageType_COMMAND,   // 0
        kart_common_MessageType_STATUS,    // 1
        kart_common_MessageType_ACK,       // 2
        kart_common_MessageType_ERROR      // 3
    };
    
    // Test all component types (3 bits)
    const kart_common_ComponentType componentTypes[] = {
        kart_common_ComponentType_LIGHTS,   // 0
        kart_common_ComponentType_MOTORS,   // 1
        kart_common_ComponentType_SENSORS,  // 2
        kart_common_ComponentType_BATTERY,  // 3
        kart_common_ComponentType_CONTROLS  // 4
        // Add more as needed up to 7
    };
    
    // Test all combinations
    for (auto msgType : messageTypes) {
        for (auto compType : componentTypes) {
            uint8_t header = interface.packHeader(msgType, compType);
            
            // Extract the message type (top 2 bits)
            uint8_t extractedMsgType = (header >> 6) & 0x03;
            
            // Extract the component type (bits 3-5)
            uint8_t extractedCompType = (header >> 3) & 0x07;
            
            printf("Message Type: %d, Component Type: %d => Header: ", 
                   msgType, compType);
            printBinary(header);
            printf(" (0x%02X)\n", header);
            
            // Verify message type extraction
            assert(extractedMsgType == msgType);
            
            // Verify component type extraction (this would fail with the bug)
            assert(extractedCompType == compType);
            
            // Verify with unpackHeader
            kart_common_MessageType outMsgType;
            kart_common_ComponentType outCompType;
            interface.unpackHeader(header, outMsgType, outCompType);
            
            assert(outMsgType == msgType);
            assert(outCompType == compType);
        }
    }
    
    printf("packHeader tests passed!\n\n");
}

// Test round trip from pack to unpack
void testHeaderRoundTrip() {
    TestableProtobufCANInterface interface;
    
    printf("Testing header round-trip (pack->unpack)...\n");
    
    // Focus on the two component types that had the issue
    kart_common_ComponentType lights = kart_common_ComponentType_LIGHTS;   // 0
    kart_common_ComponentType controls = kart_common_ComponentType_CONTROLS; // 4
    
    // Pack headers
    uint8_t lightsHeader = interface.packHeader(kart_common_MessageType_COMMAND, lights);
    uint8_t controlsHeader = interface.packHeader(kart_common_MessageType_COMMAND, controls);
    
    printf("LIGHTS header:   ");
    printBinary(lightsHeader);
    printf(" (0x%02X)\n", lightsHeader);
    
    printf("CONTROLS header: ");
    printBinary(controlsHeader);
    printf(" (0x%02X)\n", controlsHeader);
    
    // Headers should be different
    assert(lightsHeader != controlsHeader);
    
    // Unpack and verify
    kart_common_MessageType lightsMsgType, controlsMsgType;
    kart_common_ComponentType lightsCompType, controlsCompType;
    
    interface.unpackHeader(lightsHeader, lightsMsgType, lightsCompType);
    interface.unpackHeader(controlsHeader, controlsMsgType, controlsCompType);
    
    assert(lightsCompType == lights);
    assert(controlsCompType == controls);
    
    printf("Header round-trip tests passed!\n\n");
}

// Test the complete message flow
void testFullMessageEncoding() {
    TestableProtobufCANInterface interface;
    
    printf("Testing full message encoding...\n");
    
    // Create some test messages
    struct TestMessage {
        kart_common_MessageType msgType;
        kart_common_ComponentType compType;
        uint8_t compId;
        uint8_t cmdId;
        kart_common_ValueType valueType;
        int32_t value;
    };
    
    TestMessage testMessages[] = {
        // Message for LIGHTS
        {
            kart_common_MessageType_COMMAND,
            kart_common_ComponentType_LIGHTS,
            0,  // Front lights
            0,  // MODE command
            kart_common_ValueType_UINT8,
            1   // ON
        },
        // Message for CONTROLS
        {
            kart_common_MessageType_COMMAND,
            kart_common_ComponentType_CONTROLS,
            8,  // DIAGNOSTIC
            3,  // MODE command
            kart_common_ValueType_UINT8,
            6   // TEST mode
        }
    };
    
    for (const auto& msg : testMessages) {
        // Manually build the CAN message as ProtobufCANInterface::sendMessage would
        uint8_t header = interface.packHeader(msg.msgType, msg.compType);
        uint32_t packed_value = msg.value; // Simplified for test
        
        // Prepare CAN message (8 bytes)
        uint8_t data[8];
        data[0] = header;
        data[1] = 0;  // Reserved
        data[2] = msg.compId;
        data[3] = msg.cmdId;
        data[4] = static_cast<uint8_t>(msg.valueType) << 4; // Value type in high nibble
        data[5] = (packed_value >> 16) & 0xFF;  // Value byte 2 (MSB)
        data[6] = (packed_value >> 8) & 0xFF;   // Value byte 1
        data[7] = packed_value & 0xFF;          // Value byte 0 (LSB)
        
        printf("CAN message for %s: ", 
               msg.compType == kart_common_ComponentType_LIGHTS ? "LIGHTS" : "CONTROLS");
        for (int i = 0; i < 8; i++) {
            printf("%02X ", data[i]);
        }
        printf("\n");
        
        // Now unpack and verify
        kart_common_MessageType outMsgType;
        kart_common_ComponentType outCompType;
        interface.unpackHeader(data[0], outMsgType, outCompType);
        
        uint8_t outCompId = data[2];
        uint8_t outCmdId = data[3];
        kart_common_ValueType outValueType = static_cast<kart_common_ValueType>(data[4] >> 4);
        
        uint32_t outPackedValue = (static_cast<uint32_t>(data[5]) << 16) |
                                 (static_cast<uint32_t>(data[6]) << 8) |
                                 data[7];
        
        printf("Unpacked: MsgType=%d, CompType=%d, CompId=%d, CmdId=%d, ValueType=%d, Value=%d\n",
               outMsgType, outCompType, outCompId, outCmdId, outValueType, outPackedValue);
        
        // Verify correct unpacking
        assert(outMsgType == msg.msgType);
        assert(outCompType == msg.compType);
        assert(outCompId == msg.compId);
        assert(outCmdId == msg.cmdId);
        assert(outValueType == msg.valueType);
        assert(outPackedValue == msg.value);
    }
    
    printf("Full message encoding tests passed!\n\n");
}

// Entry point
int main() {
    printf("====== Header Encoding Tests ======\n\n");
    
    // Run the tests
    testPackHeader();
    testHeaderRoundTrip();
    testFullMessageEncoding();
    
    printf("All tests passed successfully!\n");
    return 0;
} 