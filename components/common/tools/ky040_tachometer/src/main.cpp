#include <Arduino.h>

// --- Configuration ---
// KY-040 Encoder Module Connections (Arduino Uno/Nano)
const int ENCODER_PIN_A = 2; // CLK pin to D2 (Interrupt 0)
const int ENCODER_PIN_B = 3; // DT pin to D3 (Interrupt 1)

// Base PPR (Pulses Per Revolution) for the KY-040 encoder's mechanical cycle
const int ENCODER_BASE_PPR = 20;
// Effective counts per revolution using 4x quadrature decoding (A & B, CHANGE edge)
const int EFFECTIVE_COUNTS_PER_REV = ENCODER_BASE_PPR * 4; // 20 * 4 = 80

// --- Global Variables ---
// Use volatile for variables modified within an ISR
volatile unsigned long pulseCount = 0;

// Variables for tracking time and counts between calculations
unsigned long lastCalcTime = 0;
unsigned long lastPulseCount = 0;
const unsigned long CALC_INTERVAL_MS = 500; // Calculate RPM every 500ms

// --- Interrupt Service Routine (ISR) ---
// This function is called whenever Pin A or Pin B changes state.
// Keep it as fast as possible - just increment the counter.
void countPulse() {
  pulseCount++;
}

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial port connection (optional, good for Uno/Nano)

  // Configure encoder pins as inputs.
  // The KY-040 module typically includes pull-up resistors, so INPUT_PULLUP is not needed.
  pinMode(ENCODER_PIN_A, INPUT);
  pinMode(ENCODER_PIN_B, INPUT);

  // Attach interrupts to both encoder pins
  // Trigger on any CHANGE (rising or falling edge) for maximum resolution (4x decoding)
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_A), countPulse, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENCODER_PIN_B), countPulse, CHANGE);

  Serial.println("KY-040 Encoder Tachometer Initialized");
  Serial.print("Effective Counts/Rev: ");
  Serial.println(EFFECTIVE_COUNTS_PER_REV);

  lastCalcTime = millis(); // Initialize timing
}

void loop() {
  unsigned long currentTime = millis();

  // Check if the calculation interval has passed
  if (currentTime - lastCalcTime >= CALC_INTERVAL_MS) {
    unsigned long currentPulseCount;

    // Safely read the volatile pulse count by temporarily disabling interrupts
    noInterrupts();
    currentPulseCount = pulseCount;
    interrupts();

    // Calculate time elapsed and pulses counted during this interval
    unsigned long timeElapsed = currentTime - lastCalcTime;
    // Handle potential overflow of pulseCount if necessary (though less likely for short intervals)
    unsigned long pulsesInInterval = currentPulseCount - lastPulseCount;

    // Calculate pulses per second
    // Avoid division by zero if timeElapsed is somehow 0
    float pulsesPerSecond = (timeElapsed > 0) ? ((float)pulsesInInterval / (timeElapsed / 1000.0f)) : 0.0f;

    // Calculate RPM
    // RPM = (Pulses per second * 60) / Effective Counts per Revolution
    float rpm = (pulsesPerSecond * 60.0f) / (float)EFFECTIVE_COUNTS_PER_REV;

    // Print the results
    Serial.print("Pulses/sec: ");
    Serial.print(pulsesPerSecond, 2); // 2 decimal places
    Serial.print(" | Pulses: ");
    Serial.print(pulsesInInterval);
    Serial.print(" | Time(ms): ");
    Serial.print(timeElapsed);
    Serial.print(" | RPM: ");
    Serial.println(rpm, 1); // 1 decimal place

    // Update variables for the next calculation interval
    lastCalcTime = currentTime;
    lastPulseCount = currentPulseCount;
  }

  // Small delay to keep the loop from running too fast (optional)
  delay(1);
} 