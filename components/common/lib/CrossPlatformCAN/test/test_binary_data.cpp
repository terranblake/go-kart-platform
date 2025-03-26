/**
 * test_binary_data.cpp - Test for binary data transmission functionality
 */

#include <stdio.h>
#include "../ProtobufCANInterface.h"
#include "../CANInterface.h"
#include <cassert>
#include <cstring>
#include <vector>

// Mock CANInterface for testing
class MockCANInterface : public CANInterface {
public:
    bool begin(long baudRate, const char* device) override {
        printf("MockCANInterface::begin(%ld, %s)\n", baudRate, device ? device : "NULL");
        return true;
    }
    
    bool sendMessage(const CANMessage& msg) override {
        printf("MockCANInterface::sendMessage: ID=0x%X, length=%d, data=", msg.id, msg.length);
        for (int i = 0; i < msg.length; i++) {
            printf("%02X ", msg.data[i]);
        }
        printf("\n");
        
        // Store message for later inspection
        sent_messages.push_back(msg);
        return true;
    }
    
    bool receiveMessage(CANMessage& msg) override {
        if (test_messages.empty()) {
            return false;
        }
        
        msg = test_messages.front();
        test_messages.erase(test_messages.begin());
        return true;
    }
    
    // Queue a message to be "received"
    void queueTestMessage(const CANMessage& msg) {
        test_messages.push_back(msg);
    }
    
    // Get sent messages for verification
    const std::vector<CANMessage>& getSentMessages() const {
        return sent_messages;
    }
    
    void clearSentMessages() {
        sent_messages.clear();
    }
    
private:
    std::vector<CANMessage> test_messages;
    std::vector<CANMessage> sent_messages;
};

// Global callback counters
int binary_callback_count = 0;
size_t last_binary_size = 0;
uint8_t binary_buffer[1024];

// Binary handler callback
void testBinaryHandler(
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id,
    kart_common_ValueType value_type,
    const void* data,
    size_t data_size
) {
    printf("Binary handler called: msg_type=%d, comp_type=%d, component_id=%u, command_id=%u, value_type=%d, data_size=%zu\n",
           msg_type, comp_type, component_id, command_id, value_type, data_size);
    
    binary_callback_count++;
    last_binary_size = data_size;
    
    // Copy data to buffer for verification
    if (data_size <= sizeof(binary_buffer)) {
        memcpy(binary_buffer, data, data_size);
    }
}

void test_binary_data_send() {
    printf("\n--- Testing binary data send ---\n");
    
    // Create the mock CAN interface
    MockCANInterface mock_can;
    
    // Create the ProtobufCANInterface with mock CAN
    ProtobufCANInterface interface(0x123);
    
    // Replace the internal CAN interface with our mock
    // This is a bit hacky but works for testing
    interface.m_canInterface = mock_can;
    
    // Initialize
    assert(interface.begin(500000, "can0"));
    
    // Create test binary data
    const uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A};
    size_t data_size = sizeof(test_data);
    
    // Send the binary data
    bool result = interface.sendBinaryData(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        0xFF,  // ALL lights
        0x0A,  // Animation command
        kart_common_ValueType_UINT8,
        test_data,
        data_size
    );
    
    // Verify result
    assert(result);
    
    // Get sent messages and verify
    const std::vector<CANMessage>& messages = mock_can.getSentMessages();
    
    // Should have sent at least 2 messages (start and end frames)
    printf("Sent %zu messages\n", messages.size());
    assert(messages.size() >= 2);
    
    // Verify first message is a start frame
    const CANMessage& first_msg = messages[0];
    assert(first_msg.data[1] & 0x80);  // Bit 7 should be set for start frame
    
    // Verify last message is an end frame
    const CANMessage& last_msg = messages[messages.size() - 1];
    assert(last_msg.data[1] & 0x40);  // Bit 6 should be set for end frame
    
    printf("Binary data send test passed!\n");
}

void test_binary_data_receive() {
    printf("\n--- Testing binary data receive ---\n");
    
    // Create the mock CAN interface
    MockCANInterface mock_can;
    
    // Create the ProtobufCANInterface with mock CAN
    ProtobufCANInterface interface(0x123);
    
    // Replace the internal CAN interface with our mock
    interface.m_canInterface = mock_can;
    
    // Initialize
    assert(interface.begin(500000, "can0"));
    
    // Reset test counters
    binary_callback_count = 0;
    last_binary_size = 0;
    memset(binary_buffer, 0, sizeof(binary_buffer));
    
    // Register a binary handler
    interface.registerBinaryHandler(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        0xFF,  // ALL lights
        0x0A,  // Animation command
        testBinaryHandler
    );
    
    // Create test binary data
    const uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    size_t data_size = sizeof(test_data);
    
    // Create CAN messages for binary data transmission
    CANMessage start_msg;
    start_msg.id = 0x123;
    start_msg.length = 8;
    
    // Header: COMMAND + LIGHTS
    start_msg.data[0] = (kart_common_MessageType_COMMAND << 6) | (kart_common_ComponentType_LIGHTS << 3);
    start_msg.data[1] = 0x80;  // Start flag, seq 0
    start_msg.data[2] = 0xFF;  // ALL lights
    start_msg.data[3] = 0x0A;  // Animation command
    start_msg.data[4] = (kart_common_ValueType_UINT8 << 4) | 2;  // UINT8 type, 2 frames
    
    // Copy first 3 bytes of data
    start_msg.data[5] = test_data[0];
    start_msg.data[6] = test_data[1];
    start_msg.data[7] = test_data[2];
    
    // Create end message
    CANMessage end_msg;
    end_msg.id = 0x123;
    end_msg.length = 8;
    
    // Header: COMMAND + LIGHTS
    end_msg.data[0] = (kart_common_MessageType_COMMAND << 6) | (kart_common_ComponentType_LIGHTS << 3);
    end_msg.data[1] = 0x41;  // End flag, seq 1
    end_msg.data[2] = 0xFF;  // ALL lights
    end_msg.data[3] = 0x0A;  // Animation command
    end_msg.data[4] = 5;     // 5 bytes remaining
    
    // Copy remaining bytes of data - place in the proper locations (bytes 5-7)
    end_msg.data[5] = test_data[3];
    end_msg.data[6] = test_data[4];
    end_msg.data[7] = test_data[5];
    
    // Queue the messages
    mock_can.queueTestMessage(start_msg);
    mock_can.queueTestMessage(end_msg);
    
    // Process the messages
    interface.process();
    interface.process();
    
    // Verify handler was called
    printf("Binary callback count: %d\n", binary_callback_count);
    assert(binary_callback_count == 1);
    
    // Verify received data size
    printf("Last binary size: %zu\n", last_binary_size);
    assert(last_binary_size == 6);  // 3 bytes from first frame + 3 bytes from second frame
    
    // Verify data contents
    for (size_t i = 0; i < 6; i++) {
        printf("binary_buffer[%zu] = 0x%02X, test_data[%zu] = 0x%02X\n", 
               i, binary_buffer[i], i, test_data[i]);
        assert(binary_buffer[i] == test_data[i]);
    }
    
    printf("Binary data receive test passed!\n");
}

int main() {
    printf("Running binary data tests\n");
    
    test_binary_data_send();
    test_binary_data_receive();
    
    printf("\nAll binary data tests passed!\n");
    return 0;
} 