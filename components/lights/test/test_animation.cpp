#include <Arduino.h>
#include <unity.h>
#include <FastLED.h>
#include "../include/Config.h"

// Mock objects
CRGB leds[NUM_LEDS];
LightState lightState;
AnimationState animationState;
AnimationConfig animationConfig;

// Function declarations for functions we'll test
void resetAnimationState();
void displayAnimationFrame(CRGB* ledArray, uint16_t numLeds, AnimationState& anim);
void updateAnimation(CRGB* ledArray, uint16_t numLeds, AnimationState& anim, const AnimationConfig& config);

// Setup function runs before each test
void setUp(void) {
  // Initialize our test state
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  
  // Clear light state
  lightState.mode = kart_lights_LightModeValue_OFF;
  lightState.brightness = DEFAULT_BRIGHTNESS;
  lightState.turnLeft = 0;
  lightState.turnRight = 0;
  lightState.hazard = 0;
  lightState.braking = 0;
  lightState.sweepPosition = 0;
  lightState.animation = 0;
  
  // Set default animation config
  animationConfig.fps = DEFAULT_ANIMATION_FPS;
  animationConfig.frameDuration = 1000 / DEFAULT_ANIMATION_FPS;
  animationConfig.loopAnimation = true;
  animationConfig.brightness = DEFAULT_BRIGHTNESS;
  
  // Reset animation state
  resetAnimationState();
}

// Test resetAnimationState function
void test_reset_animation_state(void) {
  // First set some values in animation state
  animationState.currentFrame = 10;
  animationState.receivingFrame = true;
  animationState.frameComplete = true;
  animationState.totalFrames = 5;
  animationState.receivedFrames = 3;
  animationState.lastFrameTime = 1000;
  animationState.isPlaying = true;
  animationState.singlePixelIndex = 15;
  
  // Now reset and verify all values are reset
  resetAnimationState();
  
  TEST_ASSERT_EQUAL_INT(0, animationState.currentFrame);
  TEST_ASSERT_EQUAL_INT(false, animationState.receivingFrame);
  TEST_ASSERT_EQUAL_INT(false, animationState.frameComplete);
  TEST_ASSERT_EQUAL_INT(0, animationState.totalFrames);
  TEST_ASSERT_EQUAL_INT(0, animationState.receivedFrames);
  TEST_ASSERT_EQUAL_INT(0, animationState.lastFrameTime);
  TEST_ASSERT_EQUAL_INT(false, animationState.isPlaying);
  TEST_ASSERT_EQUAL_INT(0, animationState.singlePixelIndex);
}

// Test displayAnimationFrame function
void test_display_animation_frame(void) {
  // Setup test data
  animationState.totalFrames = 2;
  animationState.currentFrame = 0;
  
  // Fill first frame with red
  for (int i = 0; i < NUM_LEDS; i++) {
    uint16_t bufferIndex = i % MAX_ANIMATION_BUFFER_SIZE;
    animationState.frameBuffer[bufferIndex] = CRGB(255, 0, 0); // Red
  }
  
  // Fill second frame with blue
  for (int i = 0; i < NUM_LEDS; i++) {
    uint16_t bufferIndex = (NUM_LEDS + i) % MAX_ANIMATION_BUFFER_SIZE;
    animationState.frameBuffer[bufferIndex] = CRGB(0, 0, 255); // Blue
  }
  
  // Clear current LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  
  // Display first frame
  displayAnimationFrame(leds, NUM_LEDS, animationState);
  
  // Check that all LEDs are red
  for (int i = 0; i < NUM_LEDS; i++) {
    TEST_ASSERT_EQUAL_INT(255, leds[i].r);
    TEST_ASSERT_EQUAL_INT(0, leds[i].g);
    TEST_ASSERT_EQUAL_INT(0, leds[i].b);
  }
  
  // Switch to second frame
  animationState.currentFrame = 1;
  
  // Display second frame
  displayAnimationFrame(leds, NUM_LEDS, animationState);
  
  // Check that all LEDs are blue
  for (int i = 0; i < NUM_LEDS; i++) {
    TEST_ASSERT_EQUAL_INT(0, leds[i].r);
    TEST_ASSERT_EQUAL_INT(0, leds[i].g);
    TEST_ASSERT_EQUAL_INT(255, leds[i].b);
  }
}

// Test updateAnimation function
void test_update_animation(void) {
  // Setup animation state
  animationState.totalFrames = 3;
  animationState.currentFrame = 0;
  animationState.isPlaying = true;
  animationState.lastFrameTime = millis() - 1000; // Ensure we're past frame duration
  
  // Setup animation config
  animationConfig.fps = 10;
  animationConfig.frameDuration = 100; // 100ms per frame
  animationConfig.loopAnimation = true;
  
  // Fill frames with different colors
  for (int frame = 0; frame < 3; frame++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      uint16_t bufferIndex = (frame * NUM_LEDS + i) % MAX_ANIMATION_BUFFER_SIZE;
      if (frame == 0) {
        animationState.frameBuffer[bufferIndex] = CRGB(255, 0, 0); // Red
      } else if (frame == 1) {
        animationState.frameBuffer[bufferIndex] = CRGB(0, 255, 0); // Green
      } else {
        animationState.frameBuffer[bufferIndex] = CRGB(0, 0, 255); // Blue
      }
    }
  }
  
  // Update animation - should move to frame 1
  updateAnimation(leds, NUM_LEDS, animationState, animationConfig);
  
  // Verify we moved to frame 1
  TEST_ASSERT_EQUAL_INT(1, animationState.currentFrame);
  
  // Verify LEDs show green (frame 1)
  TEST_ASSERT_EQUAL_INT(0, leds[0].r);
  TEST_ASSERT_EQUAL_INT(255, leds[0].g);
  TEST_ASSERT_EQUAL_INT(0, leds[0].b);
  
  // Test animation looping
  animationState.currentFrame = 2;
  animationState.lastFrameTime = millis() - 200; // Ensure we're past frame duration
  
  // Update animation - should loop back to frame 0
  updateAnimation(leds, NUM_LEDS, animationState, animationConfig);
  
  // Verify we looped back to frame 0
  TEST_ASSERT_EQUAL_INT(0, animationState.currentFrame);
  
  // Verify LEDs show red (frame 0)
  TEST_ASSERT_EQUAL_INT(255, leds[0].r);
  TEST_ASSERT_EQUAL_INT(0, leds[0].g);
  TEST_ASSERT_EQUAL_INT(0, leds[0].b);
  
  // Test animation stopping at end
  animationConfig.loopAnimation = false;
  animationState.currentFrame = 2;
  animationState.lastFrameTime = millis() - 200;
  
  // Update animation - should stop at end
  updateAnimation(leds, NUM_LEDS, animationState, animationConfig);
  
  // Verify animation stopped
  TEST_ASSERT_EQUAL_INT(false, animationState.isPlaying);
}

void setup() {
  delay(2000); // Give board time to settle
  
  UNITY_BEGIN();
  
  RUN_TEST(test_reset_animation_state);
  RUN_TEST(test_display_animation_frame);
  RUN_TEST(test_update_animation);
  
  UNITY_END();
}

void loop() {
  // Nothing to do
} 