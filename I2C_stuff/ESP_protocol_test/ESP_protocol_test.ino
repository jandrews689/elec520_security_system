// -------- ESP32 I²C MASTER: POLL NANO SLAVES AND PRINT (TO BE CHANGED BUT JUST FOR TESTING) --------
#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN   21
#define SCL_PIN   22

// I2C address range to scan (room 1-8)
#define ADDR_MIN  0x12
#define ADDR_MAX  0x20

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin(SDA_PIN, SCL_PIN); // 100 kHz
  Wire.setTimeOut(50);

  Serial.println("ESP32 I2C master started. Polling 0x12..0x20");
}

void loop() {
  const size_t REQ = 30; // bytes to request matches Nano cap
  static uint8_t addr = ADDR_MIN;

  // Try to request one line from the current address
  Wire.beginTransmission(addr);
  // A zero length write = end and do requestFrom next.
  uint8_t txStatus = Wire.endTransmission(true); // send STOP

  // If we have a connection try and read from the requests
  if (txStatus == 0) {
    size_t got = Wire.requestFrom((int)addr, (int)REQ, (int)true);
    if (got > 0) {
      // Read bytes, print as a line
      while (Wire.available()) {
        char c = (char)Wire.read();
        Serial.print(c);
      }
      // Ensure line break even if Nano didn’t send one
      Serial.println();
    }
  }

  // Move to next address
  addr++;
  if (addr > ADDR_MAX) addr = ADDR_MIN;

  // Small delay so we’re not hammering the bus
  delay(10);
}
