#include <Wire.h>

#define SDA_PIN   21
#define SCL_PIN   22
#define SLAVE_ADDR 0x12

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN, 400000); // 400 kHz I2C
}

void loop() {
  // Poll the slave: ask for up to 16 bytes (can change that atm this is fine)
  Wire.requestFrom(SLAVE_ADDR, 16);
  if (Wire.available()) {
    uint8_t len = Wire.read();
    if (len > 0 && len <= 15) {
      char buf[16] = {0};
      for (uint8_t i = 0; i < len && Wire.available(); i++) {
        buf[i] = (char)Wire.read();
      }
      Serial.print("Got: ");
      Serial.println(buf);
      // Act on ALERT
      if (len == 5 && strncmp(buf, "ALERT", 5) == 0) {
        Serial.println("Button press received over I2C.");
      }
      // Drain any leftover bytes (if master asked for 16 but slave sent fewer)
      while (Wire.available()) (void)Wire.read();
    } else {
      // no message (len==0), or invalid length so drain
      while (Wire.available()) (void)Wire.read();
    }
  }

  delay(20); // poll every 20 ms
}