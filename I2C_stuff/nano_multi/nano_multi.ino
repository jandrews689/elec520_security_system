// -------- NANO ULTRASONIC + HALL (I²C SLAVE) --------
#include <Arduino.h>
#include <Wire.h>
#include "elec520_nano.h"   // Joes header

// ====== USER PARAMETERS ======
#define I2C_ADDR   0x12       // Nano adresses from 0x12 to 0x20
#define FLOOR_ID   0x01       // set per room (hardcoded for now)
#define ULTRA_ID   0x01       // base ultrasonic ID for this room (sensor 0 will be ULTRA_ID)
// CHANGED: HALL_ID becomes base ID so we can add (index) on top
#define HALL_ID    0x01       // base hall ID for this room (sensor 0 will be HALL_ID)

// --- Sensor counts (can be 1–8) ---
#define NUM_ULTRAS 1          // NEW: set 1–8 ultrasonics per room
#define NUM_HALLS  1          // NEW: set 1–8 halls per room

// Ultrasonic pins HC-SR04 (first sensor kept for backwards compatibility)
#define TRIG_PIN   6
#define ECHO_PIN   7
#define TRIG_PIN_2 8
#define ECHO_PIN_2 9
// Hall pin (active low when magnet present i cant remeber if this is the right way ask charlie?)
#define HALL_PIN   4
#define HALL_PIN_2 3
#define HALL_PIN_3 10
#define HALL_PIN_4 11
#define LED_PIN    8 

// NEW: arrays for up to 8 ultrasonics and 8 halls
// For now we only use 1 of each (the pins you already had).
// To add more, extend these arrays and bump NUM_ULTRAS / NUM_HALLS.
static const uint8_t ULTRA_TRIG_PINS[NUM_ULTRAS] = {
  TRIG_PIN//,TRIG_PIN_2      
};

static const uint8_t ULTRA_ECHO_PINS[NUM_ULTRAS] = {
  ECHO_PIN//,ECHO_PIN_2      
};

static const uint8_t HALL_PINS[NUM_HALLS] = {
  HALL_PIN//,HALL_PIN_2      // add more hall pins here
};

// ====== INTERNAL ======
static uint8_t ROOM_ID;                // derived from I2C address
static volatile bool haveLine = false; // when line is ready
static String outLine;                 // the current line
static bool sentConnectOnce = false;   // only send connection token once
static bool sendUltraNext = true;      // alternate ultra/hall
static uint32_t lastSenseMs = 0;

// CHANGED: arrays to hold last readings for multiple sensors
static uint8_t lastUltraCm[NUM_ULTRAS] = {0};  // NEW: one value per ultrasonic
static uint8_t hallOpen01[NUM_HALLS]  = {0};   // NEW: 1=detected, 0=no magnet

// NEW: indices so we can cycle through multiple sensors
static uint8_t ultraIndex = 0;
static uint8_t hallIndex  = 0;

// ===== Ultrasonic helper =====
// CHANGED: now takes explicit pins so it can be reused for multiple sensors
static uint8_t readUltrasonicCm(uint8_t trigPin, uint8_t echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(3);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  // pulseIn timeout ~ 30ms -> ~5m range in theory capped at 255cm anyway for the min
  unsigned long echo = pulseIn(echoPin, HIGH, 30000UL);
  if (echo == 0) return 255; // no echo
  // distance cm = echo / 58 (approx)
  unsigned long cm = echo / 58UL;
  if (cm > 255UL) cm = 255UL;
  return (uint8_t)cm;
}

// Refresh ALL sensors (up to 8 ultra + 8 hall)
static void refreshSensors() {
  // 20 Hz max update rate
  if (millis() - lastSenseMs < 50) return;
  lastSenseMs = millis();

  // === Ultrasonic array update ===
  bool anyClose = false; // NEW: to drive LED if any sensor < 20cm

  for (uint8_t i = 0; i < NUM_ULTRAS; i++) {  // NEW: loop over all ultrasonics
    lastUltraCm[i] = readUltrasonicCm(ULTRA_TRIG_PINS[i], ULTRA_ECHO_PINS[i]);

    if (lastUltraCm[i] < 20) {
      anyClose = true;
    }
  }

  // LED behaviour kept similar but now based on "any" sensor being close
  if (anyClose) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }

  // === Hall array update ===
  for (uint8_t i = 0; i < NUM_HALLS; i++) {   // NEW: loop over all halls
    int raw = digitalRead(HALL_PINS[i]);
    hallOpen01[i] = (raw == HIGH) ? 0 : 1;
  }
}

// Build line to send over I2C
static void prepareNextLine() {
  if (!sentConnectOnce) {
    outLine = nanoTokenRoomConnection(FLOOR_ID, ROOM_ID, /*connected*/true);
    sentConnectOnce = true;
  } else {
    // ultra / hall
    if (sendUltraNext) {
      // NEW: send one ultrasonic reading per call, cycling through all sensors
      uint8_t thisUltraId = ULTRA_ID + ultraIndex; // e.g. 0x01, 0x02, ... for each sensor
      outLine = nanoTokenUltra(FLOOR_ID, ROOM_ID, thisUltraId, lastUltraCm[ultraIndex]);

      ultraIndex++;
      if (ultraIndex >= NUM_ULTRAS) ultraIndex = 0; // wrap around

    } else {
      // NEW: send one hall reading per call, cycling through all sensors
      uint8_t thisHallId = HALL_ID + hallIndex;    // e.g. 0x01, 0x02, ... for each hall
      outLine = nanoTokenHall(FLOOR_ID, ROOM_ID, thisHallId, hallOpen01[hallIndex]);

      hallIndex++;
      if (hallIndex >= NUM_HALLS) hallIndex = 0;   // wrap around
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
  //counter
  int i = payload.length();
  const size_t N = min((size_t)payload.length(), (size_t)i); // leave margin
  Wire.write((const uint8_t*)payload.c_str(), N);
  haveLine = false; 
}

void setup() {
  // CHANGED: set pinModes using the new arrays (so extra sensors are covered)
  for (uint8_t i = 0; i < NUM_ULTRAS; i++) {       // NEW
    pinMode(ULTRA_TRIG_PINS[i], OUTPUT);
    pinMode(ULTRA_ECHO_PINS[i], INPUT);
  }

  for (uint8_t i = 0; i < NUM_HALLS; i++) {        // NEW
    pinMode(HALL_PINS[i], INPUT_PULLUP);
  }

  pinMode(LED_PIN, OUTPUT);

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
  Serial.print(" UltraBaseID="); Serial.print(ULTRA_ID);   // CHANGED: clarified as base
  Serial.print(" HallBaseID="); Serial.println(HALL_ID);   // CHANGED: clarified as base

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
