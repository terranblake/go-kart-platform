#include <gtest/gtest.h>
#include "../ProtobufCANInterface.h"
#include <vector>
#include <iostream>

// Test fixture for animation protocol tests
class AnimationProtocolTest : public ::testing::Test {
protected:
    ProtobufCANInterface* sender;
    ProtobufCANInterface* receiver;
    
    // Test data - RGB values for 10 LEDs (30 bytes)
    std::vector<uint8_t> testData;
    
    // Flag to track if animation data was received
    bool dataReceived;
    
    // Storage for received data
    std::vector<uint8_t> receivedData;
    
    void SetUp() override {
        // Create sender and receiver interfaces with small buffer for testing
        sender = new ProtobufCANInterface(0x01, 100);
        receiver = new ProtobufCANInterface(0x02, 100);
        
        // Initialize the CAN interfaces with virtual CAN device for testing
        sender->begin(500000, "vcan0");
        receiver->begin(500000, "vcan0");
        
        // Prepare test data - RGB values for 10 LEDs (30 bytes)
        testData.clear();
        for (int i = 0; i < 10; i++) {
            // Red, green, blue values (increasing pattern)
            testData.push_back(i * 25);       // R
            testData.push_back(255 - i * 25); // G
            testData.push_back(128);          // B
        }
        
        // Reset received flag and data
        dataReceived = false;
        receivedData.clear();
        
        // Register animation handler on receiver
        receiver->registerAnimationHandler(
            kart_lights_LightComponentId_FRONT,
            kart_lights_LightCommandId_ANIMATION_DATA,
            [this](uint8_t comp_id, uint8_t cmd_id, 
                   const uint8_t* data, size_t length, bool is_last_chunk) {
                
                // Store received data
                for (size_t i = 0; i < length; i++) {
                    receivedData.push_back(data[i]);
                }
                
                // Set flag if this is the last chunk
                if (is_last_chunk) {
                    dataReceived = true;
                }
            }
        );
    }
    
    void TearDown() override {
        delete sender;
        delete receiver;
    }
};

// Test packing and unpacking header with animation flags
TEST_F(AnimationProtocolTest, HeaderWithAnimationFlag) {
    // Pack header with different animation flags
    uint8_t header1 = ProtobufCANInterface::packHeader(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_common_AnimationFlag_ANIMATION_START
    );
    
    uint8_t header2 = ProtobufCANInterface::packHeader(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_common_AnimationFlag_ANIMATION_FRAME
    );
    
    uint8_t header3 = ProtobufCANInterface::packHeader(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_common_AnimationFlag_ANIMATION_END
    );
    
    // Unpack and verify headers
    kart_common_MessageType msg_type;
    kart_common_ComponentType comp_type;
    kart_common_AnimationFlag anim_flag;
    
    ProtobufCANInterface::unpackHeader(header1, msg_type, comp_type, anim_flag);
    EXPECT_EQ(msg_type, kart_common_MessageType_COMMAND);
    EXPECT_EQ(comp_type, kart_common_ComponentType_LIGHTS);
    EXPECT_EQ(anim_flag, kart_common_AnimationFlag_ANIMATION_START);
    
    ProtobufCANInterface::unpackHeader(header2, msg_type, comp_type, anim_flag);
    EXPECT_EQ(msg_type, kart_common_MessageType_COMMAND);
    EXPECT_EQ(comp_type, kart_common_ComponentType_LIGHTS);
    EXPECT_EQ(anim_flag, kart_common_AnimationFlag_ANIMATION_FRAME);
    
    ProtobufCANInterface::unpackHeader(header3, msg_type, comp_type, anim_flag);
    EXPECT_EQ(msg_type, kart_common_MessageType_COMMAND);
    EXPECT_EQ(comp_type, kart_common_ComponentType_LIGHTS);
    EXPECT_EQ(anim_flag, kart_common_AnimationFlag_ANIMATION_END);
}

// Test sending animation data in chunks
TEST_F(AnimationProtocolTest, SendAnimationData) {
    // Send animation data with default chunk size (6 bytes = 2 LEDs)
    bool result = sender->sendAnimationData(
        kart_common_ComponentType_LIGHTS,
        kart_lights_LightComponentId_FRONT,
        kart_lights_LightCommandId_ANIMATION_DATA,
        testData.data(),
        testData.size()
    );
    
    EXPECT_TRUE(result);
    
    // Process messages on receiver side (this would happen in a real application)
    for (int i = 0; i < 20; i++) { // Process enough times to receive all chunks
        receiver->process();
    }
    
    // Verify data was received correctly
    EXPECT_TRUE(dataReceived);
    EXPECT_EQ(receivedData.size(), testData.size());
    
    // Verify received data matches sent data
    for (size_t i = 0; i < testData.size() && i < receivedData.size(); i++) {
        EXPECT_EQ(receivedData[i], testData[i]) << "Data mismatch at index " << i;
    }
}

// Test sending animation data with custom chunk size
TEST_F(AnimationProtocolTest, SendAnimationDataWithCustomChunkSize) {
    // Reset received data
    dataReceived = false;
    receivedData.clear();
    
    // Send animation data with custom chunk size (3 bytes = 1 LED)
    bool result = sender->sendAnimationData(
        kart_common_ComponentType_LIGHTS,
        kart_lights_LightComponentId_FRONT,
        kart_lights_LightCommandId_ANIMATION_DATA,
        testData.data(),
        testData.size(),
        3 // 3 bytes per message (1 LED)
    );
    
    EXPECT_TRUE(result);
    
    // Process messages on receiver side
    for (int i = 0; i < 30; i++) { // Process enough times to receive all chunks
        receiver->process();
    }
    
    // Verify data was received correctly
    EXPECT_TRUE(dataReceived);
    EXPECT_EQ(receivedData.size(), testData.size());
    
    // Verify received data matches sent data
    for (size_t i = 0; i < testData.size() && i < receivedData.size(); i++) {
        EXPECT_EQ(receivedData[i], testData[i]) << "Data mismatch at index " << i;
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 