#include <unity.h>
#include <ArduinoFake.h>

using namespace fakeit;

// Include header files from the lighting component
#include "main.cpp"

// Mock FastLED
Mock<CRGB> ledMock;

void setUp() {
    ArduinoFake().reset();
}

void tearDown() {
}

// Test initialization function
void test_initialization() {
    // Arrange
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    When(Method(ArduinoFake(), Serial)).AlwaysReturn();
    
    // Act - Call the setup function
    setup();
    
    // Assert - Verify initialization calls
    Verify(Method(ArduinoFake(), pinMode).Using(LED_BUILTIN, OUTPUT)).Once();
    Verify(Method(ArduinoFake(), Serial.begin).Using(115200)).Once();
}

// Test led color setting
void test_led_color_setting() {
    // Arrange
    uint8_t r = 100, g = 150, b = 200;
    
    // Act
    setLedColor(0, r, g, b);
    
    // Assert
    // Since we can't directly verify FastLED calls, we can check the global state
    CRGB expectedColor = CRGB(r, g, b);
    // This simplistic test assumes ledColors is accessible and properly updated
    // Adjust according to your actual implementation
    TEST_ASSERT_EQUAL_UINT8(r, leds[0].r);
    TEST_ASSERT_EQUAL_UINT8(g, leds[0].g);
    TEST_ASSERT_EQUAL_UINT8(b, leds[0].b);
}

// Test message handling
void test_message_handling() {
    // Arrange
    When(Method(ArduinoFake(), millis)).Return(1000);
    
    // Set up a mock CAN message
    kart_common_MessageType msgType = kart_common_MessageType_COMMAND;
    kart_common_ComponentType compType = kart_common_ComponentType_LIGHTS;
    uint8_t componentId = 0;
    uint8_t commandId = COMMAND_SET_MODE;
    kart_common_ValueType valueType = kart_common_ValueType_INTEGER;
    int32_t value = LED_MODE_STATIC;
    
    // Act
    // Call the handler function with these parameters
    handleLightsCommand(msgType, compType, componentId, commandId, valueType, value);
    
    // Assert
    // Check that the LED mode was updated correctly
    TEST_ASSERT_EQUAL_INT(LED_MODE_STATIC, currentMode);
}

// Run all tests
int main() {
    UNITY_BEGIN();
    
    RUN_TEST(test_initialization);
    RUN_TEST(test_led_color_setting);
    RUN_TEST(test_message_handling);
    
    return UNITY_END();
} 