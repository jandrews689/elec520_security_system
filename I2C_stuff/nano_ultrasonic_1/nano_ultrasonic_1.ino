#include <Wire.h>
#define I2C_ADDR  0x13 //change to slave adress you are uploading too (room 1 0x12 room 2 0x13)
#define trigpin   6
#define echopin   7

volatile bool have_msg = false;
char msg[16];
uint8_t msg_len = 0;
float duration, distance;

void onRequestHandler() {
  uint8_t len = have_msg ? msg_len : 0;
  Wire.write(len);
  if (len > 0) {
    Wire.write((const uint8_t*)msg, len);
    have_msg = false;
    msg_len = 0;
  }
}

void setup() {
 //setup for I2C
  pinMode(A4, INPUT); digitalWrite(A4, LOW);
  pinMode(A5, INPUT); digitalWrite(A5, LOW);
  Wire.begin(I2C_ADDR);          // Nano as SLAVE
  Wire.onRequest(onRequestHandler);
  pinMode(A4, INPUT); digitalWrite(A4, LOW);
  pinMode(A5, INPUT); digitalWrite(A5, LOW);
  pinMode(trigpin, OUTPUT);
  pinMode(echopin, INPUT);
  Serial.begin(9600);
  
}

void loop() {
  // Measure distance
  digitalWrite(trigpin, LOW);  delayMicroseconds(2);
  digitalWrite(trigpin, HIGH); delayMicroseconds(10);
  digitalWrite(trigpin, LOW);

  duration = pulseIn(echopin, HIGH, 30000UL); // 30 ms timeouy improves reliability (can push it later if we want )
  if (duration == 0) {
    // no echo treat so just set to far 
    distance = 9999.0;
  } else {
    distance = (duration * 0.0343f) / 2.0f; // cm (roughly need to actualy confirm that)
  }

  Serial.print("Distance: "); Serial.println(distance);
  // Send once when we cross below 5 cm after >6 cm to re arm
  static bool armed = true;
  if (armed && distance > 0 && distance < 5.0) {
    const char* j = "f/1/r/2/u/1/t";
    msg_len = 13;
    memcpy(msg, j, msg_len);
    have_msg = true;
    armed = false; // avoid spamming
  } else if (!armed && distance > 6.0) {
    armed = true;  // re-arm when target moves away
  }

  delay(100); // polls arround 10 Hz
}
