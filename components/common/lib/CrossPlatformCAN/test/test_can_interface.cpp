#include <gtest/gtest.h>
#include "CANInterface.h"

// Mock test for CANInterface - primarily testing the data structures
// since we can't actually test CAN communication in unit tests
TEST(CANInterfaceTest, MessageStructure) {
    CANMessage msg;
    
    // Test message structure
    msg.id = 0x123;
    msg.length = 8;
    msg.data[0] = 0x01;
    msg.data[1] = 0x02;
    msg.data[2] = 0x03;
    msg.data[3] = 0x04;
    msg.data[4] = 0x05;
    msg.data[5] = 0x06;
    msg.data[6] = 0x07;
    msg.data[7] = 0x08;
    
    EXPECT_EQ(msg.id, 0x123);
    EXPECT_EQ(msg.length, 8);
    EXPECT_EQ(msg.data[0], 0x01);
    EXPECT_EQ(msg.data[1], 0x02);
    EXPECT_EQ(msg.data[2], 0x03);
    EXPECT_EQ(msg.data[3], 0x04);
    EXPECT_EQ(msg.data[4], 0x05);
    EXPECT_EQ(msg.data[5], 0x06);
    EXPECT_EQ(msg.data[6], 0x07);
    EXPECT_EQ(msg.data[7], 0x08);
}

#ifdef PLATFORM_LINUX
// Test Linux-specific functionality
TEST(CANInterfaceTest, LinuxInterfaceCreation) {
    CANInterface interface;
    
    // This test will not actually connect to CAN hardware, 
    // but we can verify the behavior of the interface
    
    // Test initialization with invalid device name (should fail gracefully)
    EXPECT_FALSE(interface.begin(500000, "invalid_can_device"));
    
    // Note: We can't easily test begin() with a valid device in a unit test
    // as it would require actual CAN hardware. Integration tests would be needed.
}
#endif

// Main test runner
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 