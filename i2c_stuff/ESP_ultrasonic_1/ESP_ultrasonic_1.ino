#include <Wire.h>

#define SDA_PIN     21
#define SCL_PIN     22
#define SLAVE_ADDR  0x12 //adress for nano 1

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);     // 100 kHz
  Wire.setTimeOut(50);              // 50 ms i2c timeout to avvoid spammy mesages
}

void loop() {
  // requests up to 16 bytes
  size_t asked = 16;
  size_t got = Wire.requestFrom((int)SLAVE_ADDR, (int)asked, (int)true); // true = send STOP
  if (got == 0) {
    // nano didn't respond or bus stuck (for later use in debugging)
    Serial.println("I2C Issue Attachment??");
    delay(50);
    return;
  }
  if (Wire.available()) {
    uint8_t len = Wire.read();
    if (len > 0 && len <= 15) {
      char buffer[16] = {0};
      for (uint8_t i = 0; i < len && Wire.available(); i++) {
        buffer[i] = (char)Wire.read();
      }
      Serial.print("Got: "); Serial.println(buffer);
      if (len == 5 && strncmp(buffer, "ALERT", 5) == 0) {
        Serial.println("ALERT received over I2C"); // for later use we can spesify the nano it came from
      }
    }
    // Drain 
    while (Wire.available()) (void)Wire.read();
  }
  delay(10);  // slight delay sonot to cause any spam errors for the I2C adress
}
