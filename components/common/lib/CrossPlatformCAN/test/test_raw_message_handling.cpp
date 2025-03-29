#include <gtest/gtest.h>
#include "ProtobufCANInterface.h"

// Mock raw handler for testing registerRawHandler
static void testRawHandler(uint32_t can_id, const uint8_t* data, uint8_t length) {
    // This would normally handle the raw message, but in the test we just need it to exist
}

TEST(ProtobufCANRawHandlingTest, RawHandlerRegistration) {
    ProtobufCANInterface interface(0x123);
    
    // Register a raw handler for animation data (CAN ID 0x700)
    interface.registerRawHandler(0x700, testRawHandler);
    
    // We can't test if the handler gets called directly in a unit test
    // since we can't send/receive real CAN messages, but we can verify
    // the interface doesn't crash when registering raw handlers
}

TEST(ProtobufCANRawHandlingTest, SendRawMessage) {
    ProtobufCANInterface interface(0x123);
    
    // Create test data (8 bytes max for CAN)
    uint8_t testData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    // Test sending raw message
    // Note: We can't verify actual transmission in a unit test, just that the method handles the call
    EXPECT_FALSE(interface.sendRawMessage(0x700, testData, 8));
    
    // Without a real CAN interface, this should return false,
    // as the underlying CANInterface hasn't been properly initialized
}

// Test to verify that raw message handling follows the design constraints
TEST(ProtobufCANRawHandlingTest, RawMessageConstraints) {
    ProtobufCANInterface interface(0x123);
    
    // Test data at maximum length
    uint8_t maxData[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    
    // Send with maximum length
    EXPECT_FALSE(interface.sendRawMessage(0x700, maxData, 8));
    
    // Testing with different CAN IDs in the reserved range
    EXPECT_FALSE(interface.sendRawMessage(0x700, maxData, 4));  // Animation data
    EXPECT_FALSE(interface.sendRawMessage(0x701, maxData, 4));  // Reserved for future expansion
    EXPECT_FALSE(interface.sendRawMessage(0x70F, maxData, 4));  // Upper bound of reserved range
}
