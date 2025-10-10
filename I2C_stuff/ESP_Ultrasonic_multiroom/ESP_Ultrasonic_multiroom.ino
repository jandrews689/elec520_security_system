#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22

//two slaves 0x12 nano 0x13 every
const uint8_t SLAVES[] = { 0x12, 0x13 };

void setup() {
  Serial.begin(115200);
  while(!Serial) {;}
  Wire.begin(SDA_PIN, SCL_PIN);    //I2C master
  Wire.setTimeOut(50);             // 50ms timeout to set pins or else it has a little moan
  Serial.println("ESP32 I2C master started.");
}

bool read_message(uint8_t addr, char* out, uint8_t& out_len) {
  out[0] = 0; out_len = 0;

  const size_t ASK = 16;
  size_t got = Wire.requestFrom((int)addr, (int)ASK, (int)true); 
  if (got == 0) return false;

  if (!Wire.available()) return false;

  uint8_t len = Wire.read();       // first byte is length of message
  if (len == 0 || len > 15) {
    // drain
    while (Wire.available()) (void)Wire.read();
    return false;
  }

  for (uint8_t i = 0; i < len && Wire.available(); i++) {
    out[i] = (char)Wire.read();
  }
  out[len] = '\0';
  out_len   = len;
  // drain
  while (Wire.available()) (void)Wire.read();
  return true;
}

void loop() {
  for (uint8_t i = 0; i < sizeof(SLAVES); ++i) {
    uint8_t addr = SLAVES[i];
    char buf[16];
    uint8_t len = 0;

    if (!read_message(addr, buf, len)) {
      // Serial.printf("No data from 0x%02X\n", addr); (ignore this its just for debuggin)
      continue;
    }

    // Print raw I2C input
    Serial.printf("0x%02X -> \"%s\"\n", addr, buf);

    // Branch on address to see what room we are getting from (all the serial can go we can do it purly off I2C adresses and there messages the serial is just nice to show)
    if (addr == 0x12 && len == 13 && strncmp(buf, "f/1/r/1/u/1/t", 13) == 0) {
      Serial.println("Ultrasonic TRUE from Nano (room 1).");
    } else if (addr == 0x13 && len == 13 && strncmp(buf, "f/1/r/2/u/1/t", 13) == 0) {
      Serial.println("Ultrasonic TRUE from Nano Every (room 2).");
    } else {
      // Fallback payload varification (UNTESTED)
      if (len == 13 && strncmp(buf, "f/1/r/1/u/1/t", 13) == 0) {
        Serial.println("Ultrasonic TRUE from room 1 (by payload).");
      } else if (len == 13 && strncmp(buf, "f/1/r/2/u/1/t", 13) == 0) {
        Serial.println("Ultrasonic TRUE from room 2 (by payload).");
      } else {
        Serial.println("Unrecognized message.");
      }
    }
  }

  delay(20); //polling without spamming the bus (havent tested faster yet)
}
