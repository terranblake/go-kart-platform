// I2C Scanner Sketch for ESP32
#include <Arduino.h>
#include <Wire.h>

// Define I2C pins if not defined elsewhere (e.g., Config.h)
// These should match the actual wiring used for the test.
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 21
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 22
#endif

void setup() {
  // Initialize Serial Monitor
  Serial.begin(115200);
  while (!Serial) { delay(10); } // Wait for Serial connection
  Serial.println("\n--- I2C Scanner Starting ---");

  // Initialize I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.printf("Initialized I2C on SDA=%d, SCL=%d\n", I2C_SDA_PIN, I2C_SCL_PIN);
}

void loop() {
  byte error, address;
  int nDevices;

  Serial.println("Scanning I2C bus...");

  nDevices = 0;
  for (address = 1; address < 127; address++ ) {
    // The i2c_scanner uses the return value of
    // Wire.endTransmission to see if a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) { // Success, device found
      Serial.print("  [+] I2C device found at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.print(address, HEX);
      Serial.println(" !");
      nDevices++;
    } else if (error == 4) { // Other error
      Serial.print("  [!] Unknown error at address 0x");
      if (address < 16) {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }
    // error == 2: received NACK on transmit of address (no device)
    // error == 3: received NACK on transmit of data
  }

  if (nDevices == 0) {
    Serial.println("--> No I2C devices found this scan.\n");
  } else {
    Serial.printf("--> Scan complete. Found %d device(s).\n\n", nDevices);
  }

  delay(5000); // Wait 5 seconds for next scan
} 