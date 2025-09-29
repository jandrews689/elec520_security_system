#include <Wire.h>

#define I2C_ADDR     0x12
#define BUTTON_PIN   2

volatile bool have_msg = false;
char msg[16];
uint8_t msg_len = 0;

// Simple debounce
bool lastBtn = HIGH;
unsigned long lastChange = 0;
const unsigned long debounceMs = 30;

void onRequestHandler() {
  // Master requested up to N bytes (master decides N)
  // We always send [LEN][DATA], where LEN is 0-15
  uint8_t len = have_msg ? msg_len : 0;
  Wire.write(len);
  if (len > 0) {
    Wire.write((const uint8_t*)msg, len);
    // one-shot
    have_msg = false;
    msg_len = 0;
  }
}

void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(A4, INPUT); digitalWrite(A4, LOW);
  pinMode(A5, INPUT); digitalWrite(A5, LOW);

  Wire.begin(I2C_ADDR);          // Nano as SLAVE
  Wire.onRequest(onRequestHandler);
  // repeat to ensure
  pinMode(A4, INPUT); digitalWrite(A4, LOW);
  pinMode(A5, INPUT); digitalWrite(A5, LOW);
}

void loop() {
  // Debounced detection
  bool reading = digitalRead(BUTTON_PIN);
  if (reading != lastBtn) {
    lastChange = millis();
    lastBtn = reading;
  }
  if ((millis() - lastChange) > debounceMs) {
    static bool sentThisPress = false;
    if (reading == LOW && !sentThisPress) {
      // Store message for master to pick up
      const char* s = "ALERT";
      msg_len = 5;
      memcpy(msg, s, msg_len);
      have_msg = true;
      sentThisPress = true;
    } else if (reading == HIGH && sentThisPress) {
      sentThisPress = false; //next press to re-arm
    }
  }
}
