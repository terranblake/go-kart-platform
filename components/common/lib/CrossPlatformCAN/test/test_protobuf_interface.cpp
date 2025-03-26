#include <gtest/gtest.h>
#include "ProtobufCANInterface.h"

// Tests for the helper functions in ProtobufCANInterface
TEST(ProtobufCANInterfaceTest, HeaderFunctions) {
    // Test header packing
    uint8_t header = ProtobufCANInterface::packHeader(
        kart_common_MessageType_COMMAND, 
        kart_common_ComponentType_LIGHTS
    );
    
    // Test header unpacking
    kart_common_MessageType msg_type;
    kart_common_ComponentType comp_type;
    ProtobufCANInterface::unpackHeader(header, msg_type, comp_type);
    
    EXPECT_EQ(msg_type, kart_common_MessageType_COMMAND);
    EXPECT_EQ(comp_type, kart_common_ComponentType_LIGHTS);
}

TEST(ProtobufCANInterfaceTest, ValueFunctions) {
    // Test various value types
    
    // Integer value
    int32_t int_value = 12345;
    uint32_t packed_int = ProtobufCANInterface::packValue(
        kart_common_ValueType_INT24, 
        int_value
    );
    int32_t unpacked_int = ProtobufCANInterface::unpackValue(
        kart_common_ValueType_INT24, 
        packed_int
    );
    EXPECT_EQ(int_value, unpacked_int);
    
    // Boolean value - true
    int32_t bool_value_true = 1;
    uint32_t packed_bool_true = ProtobufCANInterface::packValue(
        kart_common_ValueType_BOOLEAN, 
        bool_value_true
    );
    int32_t unpacked_bool_true = ProtobufCANInterface::unpackValue(
        kart_common_ValueType_BOOLEAN, 
        packed_bool_true
    );
    EXPECT_EQ(bool_value_true, unpacked_bool_true);
    
    // Boolean value - false
    int32_t bool_value_false = 0;
    uint32_t packed_bool_false = ProtobufCANInterface::packValue(
        kart_common_ValueType_BOOLEAN, 
        bool_value_false
    );
    int32_t unpacked_bool_false = ProtobufCANInterface::unpackValue(
        kart_common_ValueType_BOOLEAN, 
        packed_bool_false
    );
    EXPECT_EQ(bool_value_false, unpacked_bool_false);

    // Test both packing and unpacking
    for (int i = -1000; i < 1000; i++) {
        uint32_t packed = ProtobufCANInterface::packValue(
            kart_common_ValueType_INT24,
            i
        );
        int32_t unpacked = ProtobufCANInterface::unpackValue(
            kart_common_ValueType_INT24,
            packed
        );
        EXPECT_EQ(i, unpacked);
    }
}

TEST(ProtobufCANInterfaceTest, InterfaceCreation) {
    // Test interface creation with node ID
    ProtobufCANInterface interface(0x123);
    
    // Since we can't test actual CAN communication in unit tests,
    // we'll just verify the interface handles invalid inputs gracefully
    EXPECT_FALSE(interface.begin(500000, "invalid_can_device"));
}

// Mock handler for testing registerHandler
static void testHandler(kart_common_MessageType msg_type, 
                       kart_common_ComponentType comp_type,
                       uint8_t component_id, uint8_t command_id,
                       kart_common_ValueType value_type, int32_t value) {
    // This would normally handle the message, but in the test we just need it to exist
}

TEST(ProtobufCANInterfaceTest, HandlerRegistration) {
    ProtobufCANInterface interface(0x123);
    
    // Register a handler
    interface.registerHandler(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        0x01,  // component_id
        0x02,  // command_id
        testHandler
    );
    
    // We can't test if the handler gets called directly in a unit test
    // since we can't send/receive real CAN messages, but we can verify
    // the interface doesn't crash when registering handlers
} 