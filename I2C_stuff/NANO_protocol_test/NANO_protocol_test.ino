// -------- NANO ULTRASONIC + HALL (I²C SLAVE) --------
#include <Arduino.h>
#include <Wire.h>
#include "elec520_protocol_nano.h"   // Joes header

// ====== USER PARAMETERS ======
#define I2C_ADDR   0x12       // Nano adresses from 0x12 to 0x20
#define FLOOR_ID    0x01       // set per room (hardcoded for now)
#define ULTRA_ID   0x01       // one ultrasonic per node (for now)
#define HALL_ID    0x01       // one hall per node (for now)

// Ultrasonic pins HC-SR04
#define TRIG_PIN   6
#define ECHO_PIN   7
// Hall pin (active low when magnet present i cant remeber if this is the right way ask charlie?)
#define HALL_PIN   4

// ====== INTERNAL ======
static uint8_t ROOM_ID;               // derived from I2C address
static volatile bool haveLine = false; // when line is ready
static String outLine;                 // the current line
static bool sentConnectOnce = false;   // only send connection token once
static bool sendUltraNext = true;      // alternate ultra/hall
static uint32_t lastSenseMs = 0;
static uint8_t lastUltraCm = 0;
static uint8_t hallOpen01 = 1;         // 0=detected, 1=no magnet (right way?)

//  ultrasonics: simple read (cap at 255 cm need to add some averaging to minimise the jitter)
static uint8_t readUltrasonicCm() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(3);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  // pulseIn timeout ~ 30ms -> ~5m range in theory capped at 255cm anyway for the min
  unsigned long echo = pulseIn(ECHO_PIN, HIGH, 30000UL);
  if (echo == 0) return 255; // no echo
  // distance cm = echo / 58 (approx)
  unsigned long cm = echo / 58UL;
  if (cm > 255UL) cm = 255UL;
  return (uint8_t)cm;
}

static void refreshSensors() {
  // 20 Hz max update rate
  if (millis() - lastSenseMs < 50) return;
  lastSenseMs = millis();

  // Ultrasonic
  lastUltraCm = readUltrasonicCm();

  // Hall
  int raw = digitalRead(HALL_PIN);
  hallOpen01 = (raw == LOW) ? 0 : 1;
}

// Build line to send over I2C
static void prepareNextLine() {
  if (!sentConnectOnce) {
    outLine = nanoTokenRoomConnection(FLOOR_ID, ROOM_ID, /*connected*/true);
    sentConnectOnce = true;
  } else {
    // ultra / hall
    if (sendUltraNext) {
      outLine = nanoTokenUltra(FLOOR_ID, ROOM_ID, ULTRA_ID, lastUltraCm);
    } else {
      outLine = nanoTokenHall(FLOOR_ID, ROOM_ID, HALL_ID, hallOpen01);
    }
    sendUltraNext = !sendUltraNext;
  }
  haveLine = true;
}

// I2C request handler master to ask for a line
static void onRequestHandler() {
  if (!haveLine) {
    // If nothing prepared build one
    prepareNextLine();
  }
  // Ensure don't overflow the Wire TX buffer (32 bytes)
  String payload = outLine + "\n";
  const size_t N = min((size_t)payload.length(), (size_t)30); // leave margin
  Wire.write((const uint8_t*)payload.c_str(), N);
  haveLine = false; 
}

void setup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(HALL_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(50);

  // Derive room from I2C address: 0x12=1, 0x13=2, ...
  ROOM_ID = (uint8_t)(I2C_ADDR - 0x11);

  // Start I2C slave
  Wire.begin((int)I2C_ADDR);
  Wire.onRequest(onRequestHandler);

  Serial.print("Nano up. I2C=0x"); Serial.print(I2C_ADDR, HEX);
  Serial.print(" Floor="); Serial.print(FLOOR_ID);
  Serial.print(" Room="); Serial.print(ROOM_ID);
  Serial.print(" UltraID="); Serial.print(ULTRA_ID);
  Serial.print(" HallID="); Serial.println(HALL_ID);

  // Prepare initial connection token (on first master request)
  prepareNextLine();
}

void loop() {
  // keep sensors fresh (contant polling)
  refreshSensors();

  // if no requests poll keep updating anyway so we allways have up to date values
  static uint32_t lastPrep = 0;
  if (millis() - lastPrep > 150) { // 6–7 Hz per stream ish
    prepareNextLine();
    lastPrep = millis();
  }
}
