#include "../ProtobufCANInterface.h"
#include <iostream>
#include <vector>
#include <iomanip>

#define TEST_PRINT(msg) std::cout << msg << std::endl

// Custom output helper to show message details
void printMessage(kart_common_MessageType msg_type, 
                 kart_common_ComponentType comp_type, 
                 uint8_t comp_id, uint8_t cmd_id, 
                 kart_common_ValueType val_type, int32_t val) {
    std::cout << "Received Message:" << std::endl
              << "  Message Type: " << (int)msg_type << std::endl
              << "  Component Type: " << (int)comp_type << std::endl
              << "  Component ID: " << (int)comp_id << std::endl
              << "  Command ID: " << (int)cmd_id << std::endl
              << "  Value Type: " << (int)val_type << std::endl
              << "  Value: " << val << std::endl;
}

// Callback handler with variables to track handler calls
struct HandlerTracker {
    int called_count = 0;
    kart_common_MessageType last_msg_type;
    kart_common_ComponentType last_comp_type;
    uint8_t last_comp_id;
    uint8_t last_cmd_id;
    kart_common_ValueType last_val_type;
    int32_t last_val;
    
    void reset() {
        called_count = 0;
    }
    
    void callback(kart_common_MessageType msg_type, 
                 kart_common_ComponentType comp_type,
                 uint8_t comp_id, uint8_t cmd_id,
                 kart_common_ValueType val_type, int32_t val) {
        called_count++;
        last_msg_type = msg_type;
        last_comp_type = comp_type;
        last_comp_id = comp_id;
        last_cmd_id = cmd_id;
        last_val_type = val_type;
        last_val = val;
        
        printMessage(msg_type, comp_type, comp_id, cmd_id, val_type, val);
    }
};

// Custom CAN implementation for testing
class TestCAN : public CANInterface {
public:
    std::vector<CANMessage> sent_messages;
    std::vector<CANMessage> received_messages;
    
    TestCAN() {}
    
    bool begin(long baudrate, const char* device) override {
        return true;
    }
    
    bool sendMessage(const CANMessage& msg) override {
        sent_messages.push_back(msg);
        return true;
    }
    
    bool receiveMessage(CANMessage& msg) override {
        if (received_messages.empty()) {
            return false;
        }
        
        msg = received_messages.back();
        received_messages.pop_back();
        return true;
    }
    
    // Helper to inject a message for processing
    void injectMessage(const CANMessage& msg) {
        received_messages.insert(received_messages.begin(), msg);
    }
    
    // Helper to create test packets
    static CANMessage createTestMessage(
        uint32_t id,
        kart_common_MessageType msg_type,
        kart_common_ComponentType comp_type,
        uint8_t comp_id,
        uint8_t cmd_id,
        kart_common_ValueType val_type,
        int32_t val
    ) {
        CANMessage msg;
        
        // Create CAN ID using the protocol format
        msg.id = id;
        
        // Create message data bytes
        uint8_t header = ((uint8_t)msg_type << 4) | ((uint8_t)comp_type & 0x0F);
        msg.data[0] = header;
        msg.data[1] = comp_id;
        msg.data[2] = cmd_id;
        msg.data[3] = (uint8_t)val_type;
        
        // Pack the value
        switch (val_type) {
            case kart_common_ValueType_BOOL:
                msg.data[4] = (val != 0) ? 1 : 0;
                msg.len = 5;
                break;
            case kart_common_ValueType_UINT8:
                msg.data[4] = (uint8_t)val;
                msg.len = 5;
                break;
            case kart_common_ValueType_INT8:
                msg.data[4] = (int8_t)val;
                msg.len = 5;
                break;
            case kart_common_ValueType_UINT16:
                msg.data[4] = (uint8_t)(val & 0xFF);
                msg.data[5] = (uint8_t)((val >> 8) & 0xFF);
                msg.len = 6;
                break;
            case kart_common_ValueType_INT16:
                msg.data[4] = (uint8_t)(val & 0xFF);
                msg.data[5] = (uint8_t)((val >> 8) & 0xFF);
                msg.len = 6;
                break;
            case kart_common_ValueType_UINT32:
                msg.data[4] = (uint8_t)(val & 0xFF);
                msg.data[5] = (uint8_t)((val >> 8) & 0xFF);
                msg.data[6] = (uint8_t)((val >> 16) & 0xFF);
                msg.data[7] = (uint8_t)((val >> 24) & 0xFF);
                msg.len = 8;
                break;
            case kart_common_ValueType_INT32:
                msg.data[4] = (uint8_t)(val & 0xFF);
                msg.data[5] = (uint8_t)((val >> 8) & 0xFF);
                msg.data[6] = (uint8_t)((val >> 16) & 0xFF);
                msg.data[7] = (uint8_t)((val >> 24) & 0xFF);
                msg.len = 8;
                break;
            default:
                msg.len = 4; // No value
        }
        
        return msg;
    }
};

bool test_message_type_filtering() {
    TEST_PRINT("Testing message type filtering...");
    
    TestCAN can;
    ProtobufCANInterface interface(0x123);
    interface.setCANInterface(&can);
    
    // Create handler trackers for different message types
    HandlerTracker command_handler;
    HandlerTracker status_handler;
    HandlerTracker any_handler;
    
    // Register handlers for specific message types
    interface.registerHandler(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_MOTOR,
        0x01,
        0x02,
        std::bind(&HandlerTracker::callback, &command_handler, 
                  std::placeholders::_1, std::placeholders::_2, 
                  std::placeholders::_3, std::placeholders::_4, 
                  std::placeholders::_5, std::placeholders::_6)
    );
    
    interface.registerHandler(
        kart_common_MessageType_STATUS,
        kart_common_ComponentType_MOTOR,
        0x01,
        0x02,
        std::bind(&HandlerTracker::callback, &status_handler, 
                  std::placeholders::_1, std::placeholders::_2, 
                  std::placeholders::_3, std::placeholders::_4, 
                  std::placeholders::_5, std::placeholders::_6)
    );
    
    // Register handler for any message type (-1)
    interface.registerHandler(
        (kart_common_MessageType)(-1),
        kart_common_ComponentType_MOTOR,
        0x01,
        0x03, // Different command ID
        std::bind(&HandlerTracker::callback, &any_handler, 
                  std::placeholders::_1, std::placeholders::_2, 
                  std::placeholders::_3, std::placeholders::_4, 
                  std::placeholders::_5, std::placeholders::_6)
    );
    
    // Create test messages
    CANMessage command_msg = TestCAN::createTestMessage(
        0x100,
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_MOTOR,
        0x01,
        0x02,
        kart_common_ValueType_INT32,
        42
    );
    
    CANMessage status_msg = TestCAN::createTestMessage(
        0x100,
        kart_common_MessageType_STATUS,
        kart_common_ComponentType_MOTOR,
        0x01,
        0x02,
        kart_common_ValueType_INT32,
        43
    );
    
    CANMessage command_diff_cmd_msg = TestCAN::createTestMessage(
        0x100,
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_MOTOR,
        0x01,
        0x03, // Different command ID
        kart_common_ValueType_INT32,
        44
    );
    
    // Test 1: Command message should trigger command_handler only
    TEST_PRINT("\nTest 1: COMMAND message with matching parameters");
    command_handler.reset();
    status_handler.reset();
    any_handler.reset();
    
    can.injectMessage(command_msg);
    interface.process();
    
    if (command_handler.called_count != 1) {
        TEST_PRINT("FAILED: Command handler should be called exactly once");
        return false;
    }
    
    if (status_handler.called_count != 0) {
        TEST_PRINT("FAILED: Status handler should not be called");
        return false;
    }
    
    if (any_handler.called_count != 0) {
        TEST_PRINT("FAILED: Any handler should not be called (command ID mismatch)");
        return false;
    }
    
    // Test 2: Status message should trigger status_handler only
    TEST_PRINT("\nTest 2: STATUS message with matching parameters");
    command_handler.reset();
    status_handler.reset();
    any_handler.reset();
    
    can.injectMessage(status_msg);
    interface.process();
    
    if (command_handler.called_count != 0) {
        TEST_PRINT("FAILED: Command handler should not be called");
        return false;
    }
    
    if (status_handler.called_count != 1) {
        TEST_PRINT("FAILED: Status handler should be called exactly once");
        return false;
    }
    
    if (any_handler.called_count != 0) {
        TEST_PRINT("FAILED: Any handler should not be called (command ID mismatch)");
        return false;
    }
    
    // Test 3: Different command ID should trigger any_handler only
    TEST_PRINT("\nTest 3: COMMAND message with different command ID");
    command_handler.reset();
    status_handler.reset();
    any_handler.reset();
    
    can.injectMessage(command_diff_cmd_msg);
    interface.process();
    
    if (command_handler.called_count != 0) {
        TEST_PRINT("FAILED: Command handler should not be called (command ID mismatch)");
        return false;
    }
    
    if (status_handler.called_count != 0) {
        TEST_PRINT("FAILED: Status handler should not be called (command ID mismatch)");
        return false;
    }
    
    if (any_handler.called_count != 1) {
        TEST_PRINT("FAILED: Any handler should be called exactly once");
        return false;
    }
    
    TEST_PRINT("\nMessage type filtering tests PASSED!");
    return true;
}

int main() {
    bool all_tests_passed = true;
    
    // Run tests
    all_tests_passed &= test_message_type_filtering();
    
    // Print final results
    std::cout << "\n==================================" << std::endl;
    if (all_tests_passed) {
        std::cout << "All tests PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "Some tests FAILED!" << std::endl;
        return 1;
    }
} 