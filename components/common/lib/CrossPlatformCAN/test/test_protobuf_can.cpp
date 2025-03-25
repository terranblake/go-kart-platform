#include <iostream>
#include <cassert>
#include <cstring>

#include "../ProtobufCANInterface.h"

// Test utilities
#define TEST_PASS(msg) std::cout << "[PASS] " << msg << std::endl
#define TEST_FAIL(msg) std::cout << "[FAIL] " << msg << std::endl; exit(1)
#define ASSERT_TRUE(cond, msg) if (!(cond)) { TEST_FAIL(msg); }
#define ASSERT_FALSE(cond, msg) if (cond) { TEST_FAIL(msg); }
#define ASSERT_EQUAL(expected, actual, msg) if ((expected) != (actual)) { \
    std::cout << "Expected: " << (expected) << ", Actual: " << (actual) << std::endl; \
    TEST_FAIL(msg); \
}

// Platform-independent tests
void test_pack_header() {
    // Test case 1: COMMAND + LIGHTS
    uint8_t header1 = ProtobufCANInterface::packHeader(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS
    );
    // Expected: [00 (COMMAND)][000 (LIGHTS)][000 (reserved)] = 0b00000000 = 0x00
    ASSERT_EQUAL(0x00, header1, "COMMAND + LIGHTS header should be 0x00");
    
    // Test case 2: STATUS + CONTROLS
    uint8_t header2 = ProtobufCANInterface::packHeader(
        kart_common_MessageType_STATUS,
        kart_common_ComponentType_CONTROLS
    );
    // Expected: [01 (STATUS)][100 (CONTROLS)][000 (reserved)] = 0b01100000 = 0x60
    ASSERT_EQUAL(0x60, header2, "STATUS + CONTROLS header should be 0x60");
    
    // Test case 3: ACK + MOTORS
    uint8_t header3 = ProtobufCANInterface::packHeader(
        kart_common_MessageType_ACK,
        kart_common_ComponentType_MOTORS
    );
    // Expected: [10 (ACK)][001 (MOTORS)][000 (reserved)] = 0b10001000 = 0x88
    ASSERT_EQUAL(0x88, header3, "ACK + MOTORS header should be 0x88");
    
    // Test case 4: ERROR + BATTERY
    uint8_t header4 = ProtobufCANInterface::packHeader(
        kart_common_MessageType_ERROR,
        kart_common_ComponentType_BATTERY
    );
    // Expected: [11 (ERROR)][011 (BATTERY)][000 (reserved)] = 0b11011000 = 0xD8
    ASSERT_EQUAL(0xD8, header4, "ERROR + BATTERY header should be 0xD8");
    
    TEST_PASS("test_pack_header");
}

void test_unpack_header() {
    // Test case 1: 0x00 = COMMAND + LIGHTS
    kart_common_MessageType msg_type1;
    kart_common_ComponentType comp_type1;
    ProtobufCANInterface::unpackHeader(0x00, msg_type1, comp_type1);
    ASSERT_EQUAL(kart_common_MessageType_COMMAND, msg_type1, "Message type should be COMMAND");
    ASSERT_EQUAL(kart_common_ComponentType_LIGHTS, comp_type1, "Component type should be LIGHTS");
    
    // Test case 2: 0x60 = STATUS + CONTROLS
    kart_common_MessageType msg_type2;
    kart_common_ComponentType comp_type2;
    ProtobufCANInterface::unpackHeader(0x60, msg_type2, comp_type2);
    ASSERT_EQUAL(kart_common_MessageType_STATUS, msg_type2, "Message type should be STATUS");
    ASSERT_EQUAL(kart_common_ComponentType_CONTROLS, comp_type2, "Component type should be CONTROLS");
    
    // Test case 3: 0x88 = ACK + MOTORS
    kart_common_MessageType msg_type3;
    kart_common_ComponentType comp_type3;
    ProtobufCANInterface::unpackHeader(0x88, msg_type3, comp_type3);
    ASSERT_EQUAL(kart_common_MessageType_ACK, msg_type3, "Message type should be ACK");
    ASSERT_EQUAL(kart_common_ComponentType_MOTORS, comp_type3, "Component type should be MOTORS");
    
    // Test case 4: 0xD8 = ERROR + BATTERY
    kart_common_MessageType msg_type4;
    kart_common_ComponentType comp_type4;
    ProtobufCANInterface::unpackHeader(0xD8, msg_type4, comp_type4);
    ASSERT_EQUAL(kart_common_MessageType_ERROR, msg_type4, "Message type should be ERROR");
    ASSERT_EQUAL(kart_common_ComponentType_BATTERY, comp_type4, "Component type should be BATTERY");
    
    TEST_PASS("test_unpack_header");
}

void test_pack_value() {
    // Test BOOLEAN values
    ASSERT_EQUAL(0, ProtobufCANInterface::packValue(kart_common_ValueType_BOOLEAN, false), "Boolean false should pack to 0");
    ASSERT_EQUAL(1, ProtobufCANInterface::packValue(kart_common_ValueType_BOOLEAN, true), "Boolean true should pack to 1");
    
    // Test INT8 values
    ASSERT_EQUAL(0x7F, ProtobufCANInterface::packValue(kart_common_ValueType_INT8, 127), "INT8 max should pack to 0x7F");
    ASSERT_EQUAL(0x80, ProtobufCANInterface::packValue(kart_common_ValueType_INT8, -128), "INT8 min should pack to 0x80");
    ASSERT_EQUAL(0xFF, ProtobufCANInterface::packValue(kart_common_ValueType_INT8, -1), "INT8 -1 should pack to 0xFF");
    
    // Test UINT8 values
    ASSERT_EQUAL(0xFF, ProtobufCANInterface::packValue(kart_common_ValueType_UINT8, 255), "UINT8 max should pack to 0xFF");
    ASSERT_EQUAL(0x00, ProtobufCANInterface::packValue(kart_common_ValueType_UINT8, 0), "UINT8 min should pack to 0x00");
    
    // Test INT16 values
    ASSERT_EQUAL(0x7FFF, ProtobufCANInterface::packValue(kart_common_ValueType_INT16, 32767), "INT16 max should pack to 0x7FFF");
    ASSERT_EQUAL(0x8000, ProtobufCANInterface::packValue(kart_common_ValueType_INT16, -32768), "INT16 min should pack to 0x8000");
    ASSERT_EQUAL(0xFE0C, ProtobufCANInterface::packValue(kart_common_ValueType_INT16, -500), "INT16 -500 should pack to 0xFE0C");
    
    // Test UINT16 values
    ASSERT_EQUAL(0xFFFF, ProtobufCANInterface::packValue(kart_common_ValueType_UINT16, 65535), "UINT16 max should pack to 0xFFFF");
    ASSERT_EQUAL(0x0000, ProtobufCANInterface::packValue(kart_common_ValueType_UINT16, 0), "UINT16 min should pack to 0x0000");
    
    // Test INT24 values
    ASSERT_EQUAL(0x7FFFFF, ProtobufCANInterface::packValue(kart_common_ValueType_INT24, 8388607), "INT24 max should pack to 0x7FFFFF");
    ASSERT_EQUAL(0x800000, ProtobufCANInterface::packValue(kart_common_ValueType_INT24, -8388608), "INT24 min should pack to 0x800000");
    
    // Test UINT24 values
    ASSERT_EQUAL(0xFFFFFF, ProtobufCANInterface::packValue(kart_common_ValueType_UINT24, 16777215), "UINT24 max should pack to 0xFFFFFF");
    ASSERT_EQUAL(0x000000, ProtobufCANInterface::packValue(kart_common_ValueType_UINT24, 0), "UINT24 min should pack to 0x000000");
    
    TEST_PASS("test_pack_value");
}

void test_unpack_value() {
    // Test BOOLEAN values
    ASSERT_EQUAL(false, ProtobufCANInterface::unpackValue(kart_common_ValueType_BOOLEAN, 0), "Unpacked Boolean 0 should be false");
    ASSERT_EQUAL(true, ProtobufCANInterface::unpackValue(kart_common_ValueType_BOOLEAN, 1), "Unpacked Boolean 1 should be true");
    
    // Test INT8 values
    ASSERT_EQUAL(127, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT8, 0x7F), "Unpacked INT8 0x7F should be 127");
    ASSERT_EQUAL(-128, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT8, 0x80), "Unpacked INT8 0x80 should be -128");
    ASSERT_EQUAL(-1, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT8, 0xFF), "Unpacked INT8 0xFF should be -1");
    
    // Test UINT8 values
    ASSERT_EQUAL(255, ProtobufCANInterface::unpackValue(kart_common_ValueType_UINT8, 0xFF), "Unpacked UINT8 0xFF should be 255");
    ASSERT_EQUAL(0, ProtobufCANInterface::unpackValue(kart_common_ValueType_UINT8, 0x00), "Unpacked UINT8 0x00 should be 0");
    
    // Test INT16 values
    ASSERT_EQUAL(32767, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT16, 0x7FFF), "Unpacked INT16 0x7FFF should be 32767");
    ASSERT_EQUAL(-32768, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT16, 0x8000), "Unpacked INT16 0x8000 should be -32768");
    ASSERT_EQUAL(-500, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT16, 0xFE0C), "Unpacked INT16 0xFE0C should be -500");
    
    // Test UINT16 values
    ASSERT_EQUAL(65535, ProtobufCANInterface::unpackValue(kart_common_ValueType_UINT16, 0xFFFF), "Unpacked UINT16 0xFFFF should be 65535");
    ASSERT_EQUAL(0, ProtobufCANInterface::unpackValue(kart_common_ValueType_UINT16, 0x0000), "Unpacked UINT16 0x0000 should be 0");
    
    // Test INT24 values
    ASSERT_EQUAL(8388607, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT24, 0x7FFFFF), "Unpacked INT24 0x7FFFFF should be 8388607");
    ASSERT_EQUAL(-8388608, ProtobufCANInterface::unpackValue(kart_common_ValueType_INT24, 0x800000), "Unpacked INT24 0x800000 should be -8388608");
    
    // Test UINT24 values
    ASSERT_EQUAL(16777215, ProtobufCANInterface::unpackValue(kart_common_ValueType_UINT24, 0xFFFFFF), "Unpacked UINT24 0xFFFFFF should be 16777215");
    ASSERT_EQUAL(0, ProtobufCANInterface::unpackValue(kart_common_ValueType_UINT24, 0x000000), "Unpacked UINT24 0x000000 should be 0");
    
    TEST_PASS("test_unpack_value");
}

// Test the symmetry of packing and unpacking
void test_pack_unpack_symmetry() {
    // Test values that should round-trip correctly
    int32_t test_values[] = {
        // Some common values
        0, 1, -1, 42, -42,
        
        // Edge cases for different types
        127, -128,                // INT8 limits
        255, 0,                   // UINT8 limits
        32767, -32768,            // INT16 limits
        65535, 0,                 // UINT16 limits
        8388607, -8388608,        // INT24 limits
        16777215, 0,              // UINT24 limits
        
        // Some interesting values
        -500, 1000, -1000, 10000, -10000
    };
    
    kart_common_ValueType value_types[] = {
        kart_common_ValueType_INT8,
        kart_common_ValueType_UINT8,
        kart_common_ValueType_INT16,
        kart_common_ValueType_UINT16,
        kart_common_ValueType_INT24,
        kart_common_ValueType_UINT24
    };
    
    // For each type and value, check round-trip consistency
    for (kart_common_ValueType type : value_types) {
        for (int32_t value : test_values) {
            // Skip values that are out of range for the type
            if ((type == kart_common_ValueType_UINT8 && value < 0) ||
                (type == kart_common_ValueType_UINT8 && value > 255) ||
                (type == kart_common_ValueType_INT8 && (value < -128 || value > 127)) ||
                (type == kart_common_ValueType_UINT16 && value < 0) ||
                (type == kart_common_ValueType_UINT16 && value > 65535) ||
                (type == kart_common_ValueType_INT16 && (value < -32768 || value > 32767)) ||
                (type == kart_common_ValueType_UINT24 && value < 0) ||
                (type == kart_common_ValueType_UINT24 && value > 16777215) ||
                (type == kart_common_ValueType_INT24 && (value < -8388608 || value > 8388607))) {
                continue;
            }
            
            // Pack the value
            uint32_t packed = ProtobufCANInterface::packValue(type, value);
            
            // Unpack the value
            int32_t unpacked = ProtobufCANInterface::unpackValue(type, packed);
            
            // Check round-trip consistency
            if (value != unpacked) {
                std::cout << "Type: " << (int)type << ", Original: " << value << ", Packed: 0x" 
                    << std::hex << packed << ", Unpacked: " << std::dec << unpacked << std::endl;
                ASSERT_EQUAL(value, unpacked, "Pack/unpack round-trip should preserve value");
            }
        }
    }
    
    TEST_PASS("test_pack_unpack_symmetry");
}

// Test message encoding and decoding for a complete CAN message
void test_message_encoding() {
    // Test case: INT16 value -500 in a STATUS message for CONTROLS component
    kart_common_MessageType msg_type = kart_common_MessageType_STATUS;
    kart_common_ComponentType comp_type = kart_common_ComponentType_CONTROLS;
    uint8_t component_id = 2;
    uint8_t command_id = 3;
    kart_common_ValueType value_type = kart_common_ValueType_INT16;
    int32_t value = -500;
    
    // Pack header
    uint8_t header = ProtobufCANInterface::packHeader(msg_type, comp_type);
    ASSERT_EQUAL(0x60, header, "Header should be 0x60");
    
    // Pack value
    uint32_t packed_value = ProtobufCANInterface::packValue(value_type, value);
    ASSERT_EQUAL(0xFE0C, packed_value, "Packed value should be 0xFE0C");
    
    // Create a CAN message manually
    uint8_t can_data[8];
    can_data[0] = header;
    can_data[1] = 0;  // Reserved
    can_data[2] = component_id;
    can_data[3] = command_id;
    can_data[4] = static_cast<uint8_t>(value_type) << 4;  // Value type in high nibble
    can_data[5] = (packed_value >> 16) & 0xFF;  // Value byte 2 (MSB)
    can_data[6] = (packed_value >> 8) & 0xFF;   // Value byte 1
    can_data[7] = packed_value & 0xFF;          // Value byte 0 (LSB)
    
    // Verify the message bytes
    ASSERT_EQUAL(0x60, can_data[0], "Byte 0 should be 0x60");
    ASSERT_EQUAL(0x00, can_data[1], "Byte 1 should be 0x00");
    ASSERT_EQUAL(0x02, can_data[2], "Byte 2 should be 0x02");
    ASSERT_EQUAL(0x03, can_data[3], "Byte 3 should be 0x03");
    ASSERT_EQUAL(0x30, can_data[4], "Byte 4 should be 0x30");
    ASSERT_EQUAL(0x00, can_data[5], "Byte 5 should be 0x00");
    ASSERT_EQUAL(0xFE, can_data[6], "Byte 6 should be 0xFE");
    ASSERT_EQUAL(0x0C, can_data[7], "Byte 7 should be 0x0C");
    
    // Now decode the message
    kart_common_MessageType decoded_msg_type;
    kart_common_ComponentType decoded_comp_type;
    ProtobufCANInterface::unpackHeader(can_data[0], decoded_msg_type, decoded_comp_type);
    
    uint8_t decoded_component_id = can_data[2];
    uint8_t decoded_command_id = can_data[3];
    kart_common_ValueType decoded_value_type = static_cast<kart_common_ValueType>(can_data[4] >> 4);
    
    uint32_t decoded_packed_value = 
        (static_cast<uint32_t>(can_data[5]) << 16) |
        (static_cast<uint32_t>(can_data[6]) << 8) |
        can_data[7];
    
    int32_t decoded_value = ProtobufCANInterface::unpackValue(decoded_value_type, decoded_packed_value);
    
    // Verify decoded message matches original
    ASSERT_EQUAL(msg_type, decoded_msg_type, "Decoded message type should match original");
    ASSERT_EQUAL(comp_type, decoded_comp_type, "Decoded component type should match original");
    ASSERT_EQUAL(component_id, decoded_component_id, "Decoded component ID should match original");
    ASSERT_EQUAL(command_id, decoded_command_id, "Decoded command ID should match original");
    ASSERT_EQUAL(value_type, decoded_value_type, "Decoded value type should match original");
    ASSERT_EQUAL(value, decoded_value, "Decoded value should match original");
    
    TEST_PASS("test_message_encoding");
}

// Test edge cases for value packing
void test_pack_value_edge_cases() {
    // Test overflow behavior
    // When values exceed their type's range, they should be truncated
    
    // INT8 overflow (127+1 should wrap to -128 in two's complement)
    int32_t overflow_int8 = 128;
    ASSERT_EQUAL(0x80, ProtobufCANInterface::packValue(kart_common_ValueType_INT8, overflow_int8), 
                "INT8 overflow should truncate to 0x80");
    
    // UINT8 overflow (255+1 should wrap to 0)
    int32_t overflow_uint8 = 256;
    ASSERT_EQUAL(0x00, ProtobufCANInterface::packValue(kart_common_ValueType_UINT8, overflow_uint8), 
                "UINT8 overflow should truncate to 0x00");
    
    // INT16 overflow
    int32_t overflow_int16 = 32768;
    ASSERT_EQUAL(0x8000, ProtobufCANInterface::packValue(kart_common_ValueType_INT16, overflow_int16), 
                "INT16 overflow should truncate to 0x8000");
    
    // Negative values in unsigned types should be masked appropriately
    ASSERT_EQUAL(0xFF, ProtobufCANInterface::packValue(kart_common_ValueType_UINT8, -1), 
                "Negative value in UINT8 should be masked to 0xFF");
    ASSERT_EQUAL(0xFFFF, ProtobufCANInterface::packValue(kart_common_ValueType_UINT16, -1), 
                "Negative value in UINT16 should be masked to 0xFFFF");
    
    // Test large values that should be truncated
    int32_t large_value = 0x12345678;
    ASSERT_EQUAL(0x78, ProtobufCANInterface::packValue(kart_common_ValueType_UINT8, large_value), 
                "Large value in UINT8 should keep only lowest 8 bits");
    ASSERT_EQUAL(0x5678, ProtobufCANInterface::packValue(kart_common_ValueType_UINT16, large_value), 
                "Large value in UINT16 should keep only lowest 16 bits");
    ASSERT_EQUAL(0x345678, ProtobufCANInterface::packValue(kart_common_ValueType_UINT24, large_value), 
                "Large value in UINT24 should keep only lowest 24 bits");
    
    TEST_PASS("test_pack_value_edge_cases");
}

// Test specific bit manipulation behaviors
void test_bit_manipulation() {
    // Test header bit layout for all combinations
    for (int msg_type = 0; msg_type <= 3; msg_type++) {
        for (int comp_type = 0; comp_type <= 7; comp_type++) {
            uint8_t header = ProtobufCANInterface::packHeader(
                static_cast<kart_common_MessageType>(msg_type),
                static_cast<kart_common_ComponentType>(comp_type)
            );
            
            // Verify bit layout: [2 bits MessageType][3 bits ComponentType][3 bits reserved]
            uint8_t expected = (msg_type << 6) | (comp_type << 3);
            ASSERT_EQUAL(expected, header, "Header bit packing should match expected pattern");
            
            // Verify unpack works correctly
            kart_common_MessageType decoded_msg_type;
            kart_common_ComponentType decoded_comp_type;
            ProtobufCANInterface::unpackHeader(header, decoded_msg_type, decoded_comp_type);
            
            ASSERT_EQUAL(msg_type, static_cast<int>(decoded_msg_type), 
                        "Unpacked message type should match original");
            ASSERT_EQUAL(comp_type, static_cast<int>(decoded_comp_type), 
                        "Unpacked component type should match original");
        }
    }
    
    // Test value type bit layout in message byte 4
    for (int val_type = 0; val_type <= 7; val_type++) {
        // ValueType is stored in high nibble of byte 4
        uint8_t type_byte = static_cast<uint8_t>(val_type) << 4;
        uint8_t decoded_type = type_byte >> 4;
        
        ASSERT_EQUAL(val_type, decoded_type, 
                    "Value type should be correctly encoded in high nibble");
    }
    
    TEST_PASS("test_bit_manipulation");
}

// Test different message types and component combinations
void test_message_type_variations() {
    // Test case structures
    struct MessageTestCase {
        kart_common_MessageType msg_type;
        kart_common_ComponentType comp_type;
        uint8_t component_id;
        uint8_t command_id;
        kart_common_ValueType value_type;
        int32_t value;
        const char* description;
    };
    
    MessageTestCase test_cases[] = {
        // COMMAND message for LIGHTS component
        {
            kart_common_MessageType_COMMAND,
            kart_common_ComponentType_LIGHTS,
            1,  // FRONT
            0,  // MODE
            kart_common_ValueType_UINT8,
            1,  // ON
            "COMMAND to turn on front lights"
        },
        
        // STATUS message for MOTORS component
        {
            kart_common_MessageType_STATUS,
            kart_common_ComponentType_MOTORS,
            0,  // Motor ID
            2,  // SPEED command
            kart_common_ValueType_INT16,
            1000,  // Speed value
            "STATUS reporting motor speed"
        },
        
        // ACK message
        {
            kart_common_MessageType_ACK,
            kart_common_ComponentType_CONTROLS,
            0,  // THROTTLE
            4,  // PARAMETER
            kart_common_ValueType_BOOLEAN,
            1,  // ACK value
            "ACK for control parameter command"
        },
        
        // ERROR message
        {
            kart_common_MessageType_ERROR,
            kart_common_ComponentType_BATTERY,
            0,  // Battery ID
            0,  // Error code
            kart_common_ValueType_UINT8,
            42,  // Error value
            "ERROR message for battery"
        }
    };
    
    // Test each case
    for (const auto& test_case : test_cases) {
        // Pack header
        uint8_t header = ProtobufCANInterface::packHeader(
            test_case.msg_type,
            test_case.comp_type
        );
        
        // Pack value
        uint32_t packed_value = ProtobufCANInterface::packValue(
            test_case.value_type,
            test_case.value
        );
        
        // Create a CAN message
        uint8_t can_data[8];
        can_data[0] = header;
        can_data[1] = 0;  // Reserved
        can_data[2] = test_case.component_id;
        can_data[3] = test_case.command_id;
        can_data[4] = static_cast<uint8_t>(test_case.value_type) << 4;
        can_data[5] = (packed_value >> 16) & 0xFF;
        can_data[6] = (packed_value >> 8) & 0xFF;
        can_data[7] = packed_value & 0xFF;
        
        // Decode the message
        kart_common_MessageType decoded_msg_type;
        kart_common_ComponentType decoded_comp_type;
        ProtobufCANInterface::unpackHeader(can_data[0], decoded_msg_type, decoded_comp_type);
        
        uint8_t decoded_component_id = can_data[2];
        uint8_t decoded_command_id = can_data[3];
        kart_common_ValueType decoded_value_type = static_cast<kart_common_ValueType>(can_data[4] >> 4);
        
        uint32_t decoded_packed_value = 
            (static_cast<uint32_t>(can_data[5]) << 16) |
            (static_cast<uint32_t>(can_data[6]) << 8) |
            can_data[7];
        
        int32_t decoded_value = ProtobufCANInterface::unpackValue(
            decoded_value_type,
            decoded_packed_value
        );
        
        // Verify everything matches
        ASSERT_EQUAL(test_case.msg_type, decoded_msg_type, 
                    std::string("Decoded message type should match original for ") + test_case.description);
        ASSERT_EQUAL(test_case.comp_type, decoded_comp_type, 
                    std::string("Decoded component type should match original for ") + test_case.description);
        ASSERT_EQUAL(test_case.component_id, decoded_component_id, 
                    std::string("Decoded component ID should match original for ") + test_case.description);
        ASSERT_EQUAL(test_case.command_id, decoded_command_id, 
                    std::string("Decoded command ID should match original for ") + test_case.description);
        ASSERT_EQUAL(test_case.value_type, decoded_value_type, 
                    std::string("Decoded value type should match original for ") + test_case.description);
        ASSERT_EQUAL(test_case.value, decoded_value, 
                    std::string("Decoded value should match original for ") + test_case.description);
    }
    
    TEST_PASS("test_message_type_variations");
}

// Run all tests
int main() {
    std::cout << "=== Running Protocol Packing/Unpacking Tests ===" << std::endl;
    
    test_pack_header();
    test_unpack_header();
    test_pack_value();
    test_unpack_value();
    test_pack_unpack_symmetry();
    test_message_encoding();
    
    // Additional tests
    test_pack_value_edge_cases();
    test_bit_manipulation();
    test_message_type_variations();
    
    std::cout << "All tests passed!" << std::endl;
    return 0;
} 