#include <Arduino.h>
#include <unity.h>
#include <FastLED.h>

#include "../include/AnimationProtocol.h"

// Define mock CAN message buffers for testing
uint8_t mockStreamStartMsg[8] = {CMD_STREAM_START, 10, 30, 60, 0, 0, 0, 0}; // Start with 10 frames, 30 FPS, 60 LEDs
uint8_t mockStreamEndMsg[8] = {CMD_STREAM_END, 0, 0, 0, 0, 0, 0, 0};         // End stream
uint8_t mockFrameStartMsg[8] = {CMD_FRAME_START, 0, 9, 0, 0, 0, 0, 0};       // Start frame 0, 9 bytes (3 RGB LEDs)
uint8_t mockFrameEndMsg[8] = {CMD_FRAME_END, 0, 0, 0, 0, 0, 0, 0};           // End frame 0
uint8_t mockFrameData[8] = {255, 0, 0, 0, 255, 0, 0, 0};                   // Red, Green LED data (partial)
uint8_t mockConfigMsg[8] = {CMD_CONFIG, CONFIG_FPS, 30, 0, 0, 0, 0, 0};      // Set FPS to 30

// Mock LED array for testing
#define TEST_NUM_LEDS 10
CRGB mockLeds[TEST_NUM_LEDS];

// Global instance of the animation protocol for testing
AnimationProtocol animProtocol;

void setUp(void) {
  // Initialize before each test
  animProtocol = AnimationProtocol();
  animProtocol.begin(mockLeds, TEST_NUM_LEDS);
  
  // Reset LEDs to black
  for (int i = 0; i < TEST_NUM_LEDS; i++) {
    mockLeds[i] = CRGB::Black;
  }
}

void test_protocol_initialization(void) {
  // Test initial state after initialization
  TEST_ASSERT_FALSE(animProtocol.isAnimationActive());
  TEST_ASSERT_EQUAL_UINT16(0, animProtocol.getReceivedFrames());
  TEST_ASSERT_EQUAL_UINT16(0, animProtocol.getDroppedFrames());
}

void test_stream_start_message(void) {
  // Test handling of stream start message
  bool result = animProtocol.processMessage(ANIM_CTRL_ID, mockStreamStartMsg, 8);
  
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_TRUE(animProtocol.isAnimationActive());
  TEST_ASSERT_EQUAL_UINT8(30, animProtocol.getFPS());
}

void test_stream_end_message(void) {
  // First start a stream
  animProtocol.processMessage(ANIM_CTRL_ID, mockStreamStartMsg, 8);
  
  // Then end it
  bool result = animProtocol.processMessage(ANIM_CTRL_ID, mockStreamEndMsg, 8);
  
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_FALSE(animProtocol.isAnimationActive());
}

void test_frame_processing(void) {
  // Start a stream
  animProtocol.processMessage(ANIM_CTRL_ID, mockStreamStartMsg, 8);
  
  // Start a frame
  animProtocol.processMessage(ANIM_CTRL_ID, mockFrameStartMsg, 8);
  
  // Send frame data
  animProtocol.processMessage(ANIM_DATA_ID, mockFrameData, 8);
  
  // Finish frame
  animProtocol.processMessage(ANIM_CTRL_ID, mockFrameEndMsg, 8);
  
  // Check that we received the frame
  TEST_ASSERT_EQUAL_UINT16(1, animProtocol.getReceivedFrames());
  
  // Check that the LEDs were updated
  TEST_ASSERT_EQUAL_UINT8(255, mockLeds[0].r);
  TEST_ASSERT_EQUAL_UINT8(0, mockLeds[0].g);
  TEST_ASSERT_EQUAL_UINT8(0, mockLeds[0].b);
  
  TEST_ASSERT_EQUAL_UINT8(0, mockLeds[1].r);
  TEST_ASSERT_EQUAL_UINT8(255, mockLeds[1].g);
  TEST_ASSERT_EQUAL_UINT8(0, mockLeds[1].b);
}

void test_config_message(void) {
  // Process a config message
  bool result = animProtocol.processMessage(ANIM_CTRL_ID, mockConfigMsg, 8);
  
  TEST_ASSERT_TRUE(result);
  TEST_ASSERT_EQUAL_UINT8(30, animProtocol.getFPS());
}

void test_memory_usage(void) {
  // Check initial memory usage
  uint16_t initialUsage = animProtocol.getMemoryUsage();
  TEST_ASSERT_GREATER_THAN(0, initialUsage);
  
  // Start a stream and check that memory usage increases
  animProtocol.processMessage(ANIM_CTRL_ID, mockStreamStartMsg, 8);
  animProtocol.processMessage(ANIM_CTRL_ID, mockFrameStartMsg, 8);
  animProtocol.processMessage(ANIM_DATA_ID, mockFrameData, 8);
  
  uint16_t activeUsage = animProtocol.getMemoryUsage();
  TEST_ASSERT_GREATER_OR_EQUAL(initialUsage, activeUsage);
  
  // End the stream and check that memory is freed
  animProtocol.processMessage(ANIM_CTRL_ID, mockStreamEndMsg, 8);
  uint16_t finalUsage = animProtocol.getMemoryUsage();
  TEST_ASSERT_EQUAL_UINT16(initialUsage, finalUsage);
}

void test_dropped_frames(void) {
  // Start a stream
  animProtocol.processMessage(ANIM_CTRL_ID, mockStreamStartMsg, 8);
  
  // Start a frame
  animProtocol.processMessage(ANIM_CTRL_ID, mockFrameStartMsg, 8);
  
  // End the frame without sending data - should be dropped
  animProtocol.processMessage(ANIM_CTRL_ID, mockFrameEndMsg, 8);
  
  // Check that we dropped the frame
  TEST_ASSERT_EQUAL_UINT16(1, animProtocol.getDroppedFrames());
  TEST_ASSERT_EQUAL_UINT16(0, animProtocol.getReceivedFrames());
}

void test_update_function(void) {
  // Start a stream
  animProtocol.processMessage(ANIM_CTRL_ID, mockStreamStartMsg, 8);
  
  // Verify that update returns true when animation is active
  TEST_ASSERT_TRUE(animProtocol.update());
  
  // End the stream
  animProtocol.processMessage(ANIM_CTRL_ID, mockStreamEndMsg, 8);
  
  // Verify that update returns false when idle
  TEST_ASSERT_FALSE(animProtocol.update());
}

int main() {
  UNITY_BEGIN();
  
  RUN_TEST(test_protocol_initialization);
  RUN_TEST(test_stream_start_message);
  RUN_TEST(test_stream_end_message);
  RUN_TEST(test_frame_processing);
  RUN_TEST(test_config_message);
  RUN_TEST(test_memory_usage);
  RUN_TEST(test_dropped_frames);
  RUN_TEST(test_update_function);
  
  return UNITY_END();
}
