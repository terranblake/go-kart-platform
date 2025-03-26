#include <unity.h>
#include <ArduinoFake.h>
#include <FastLED.h>

using namespace fakeit;

// Include header files from the lighting component
#include "main.cpp"

void setUp() {
    ArduinoFake().reset();
    
    // Reset animation state
    clearLights(leds, NUM_LEDS);
}

void tearDown() {
}

// Test animation data reception
void test_animation_data_reception() {
    // Arrange
    const uint8_t testData[] = {
        255, 0, 0,    // Red for LED 0
        0, 255, 0,    // Green for LED 1
        0, 0, 255     // Blue for LED 2
    };
    const size_t dataLength = sizeof(testData);
    
    // Mock CAN interface
    When(Method(ArduinoFake(), millis)).Return(1000);
    
    // Act - Simulate reception of animation start message with first chunk
    uint32_t chunk1 = (testData[0] << 16) | (testData[1] << 8) | testData[2];
    
    // Simulate animation start packet
    kart_common_MessageType msgType = kart_common_MessageType_COMMAND;
    kart_common_ComponentType compType = kart_common_ComponentType_LIGHTS;
    kart_common_AnimationFlag animFlag = kart_common_AnimationFlag_ANIMATION_START;
    uint8_t componentId = kart_lights_LightComponentId_FRONT;
    uint8_t commandId = kart_lights_LightCommandId_ANIMATION_DATA;
    
    // Create header byte with animation flag
    uint8_t header = (msgType << 6) | ((compType & 0x07) << 3) | (animFlag & 0x07);
    
    // Simulate processAnimationMessage being called after header is unpacked
    processAnimationMessage(msgType, compType, animFlag, componentId, commandId, chunk1);
    
    // Send second chunk (simulate end of animation)
    uint32_t chunk2 = (testData[3] << 16) | (testData[4] << 8) | testData[5];
    animFlag = kart_common_AnimationFlag_ANIMATION_FRAME;
    processAnimationMessage(msgType, compType, animFlag, componentId, commandId, chunk2);
    
    // Send third chunk (end of animation)
    uint32_t chunk3 = (testData[6] << 16) | (testData[7] << 8) | testData[8];
    animFlag = kart_common_AnimationFlag_ANIMATION_END;
    processAnimationMessage(msgType, compType, animFlag, componentId, commandId, chunk3);
    
    // Assert - verify the LED colors have been set correctly
    TEST_ASSERT_EQUAL_UINT8(255, leds[0].r);
    TEST_ASSERT_EQUAL_UINT8(0, leds[0].g);
    TEST_ASSERT_EQUAL_UINT8(0, leds[0].b);
    
    TEST_ASSERT_EQUAL_UINT8(0, leds[1].r);
    TEST_ASSERT_EQUAL_UINT8(255, leds[1].g);
    TEST_ASSERT_EQUAL_UINT8(0, leds[1].b);
    
    TEST_ASSERT_EQUAL_UINT8(0, leds[2].r);
    TEST_ASSERT_EQUAL_UINT8(0, leds[2].g);
    TEST_ASSERT_EQUAL_UINT8(255, leds[2].b);
}

// Test animation mode setting
void test_animation_mode_setting() {
    // Arrange
    When(Method(ArduinoFake(), millis)).Return(1000);
    
    // Act - Change mode to animation
    kart_common_MessageType msgType = kart_common_MessageType_COMMAND;
    kart_common_ComponentType compType = kart_common_ComponentType_LIGHTS;
    uint8_t componentId = kart_lights_LightComponentId_FRONT;
    uint8_t commandId = kart_lights_LightCommandId_MODE;
    kart_common_ValueType valueType = kart_common_ValueType_UINT8;
    int32_t value = kart_lights_LightModeValue_ANIMATION;
    
    // Call the handler
    handleLightMode(msgType, compType, componentId, commandId, valueType, value);
    
    // Assert - Verify mode was set to animation
    TEST_ASSERT_EQUAL_INT(kart_lights_LightModeValue_ANIMATION, lightState.mode);
}

// Test animation configuration
void test_animation_config() {
    // Arrange
    When(Method(ArduinoFake(), millis)).Return(1000);
    
    // Act - Set animation FPS
    kart_common_MessageType msgType = kart_common_MessageType_COMMAND;
    kart_common_ComponentType compType = kart_common_ComponentType_LIGHTS;
    uint8_t componentId = kart_lights_LightComponentId_FRONT;
    uint8_t commandId = kart_lights_LightCommandId_ANIMATION_CONFIG;
    kart_common_ValueType valueType = kart_common_ValueType_UINT8;
    int32_t value = 30; // 30 FPS
    
    // Call the handler (this will be implemented in the next step)
    handleAnimationConfig(msgType, compType, componentId, commandId, valueType, value);
    
    // Assert - Verify animation FPS was set
    TEST_ASSERT_EQUAL_INT(30, animationConfig.fps);
}

// Run all tests
int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_animation_data_reception);
    RUN_TEST(test_animation_mode_setting);
    RUN_TEST(test_animation_config);
    
    return UNITY_END();
} 