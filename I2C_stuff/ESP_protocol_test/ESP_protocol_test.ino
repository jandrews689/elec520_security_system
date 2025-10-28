#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN   21
#define SCL_PIN   22
#define ADDR_MIN  0x12
#define ADDR_MAX  0x20
#define NUM_ADDR  (ADDR_MAX - ADDR_MIN + 1)
#define REQ_BYTES 30
#define POLL_DELAY_MS 10

static String rxBuf[NUM_ADDR];
static String lastMsg[NUM_ADDR];

static inline int idxFromAddr(uint8_t a){ return (a<ADDR_MIN||a>ADDR_MAX)?-1:(a-ADDR_MIN); }
static inline bool isPrintableAscii(char c){ return c == '\n' || c == '\r' || (c >= 32 && c <= 126); }

void setup(){
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeOut(50);
  Serial.println("ESP32 I2C master: polling and buffering clean lines.");
}

void loop(){
  static uint8_t addr = ADDR_MIN;

  // quick probe
  Wire.beginTransmission(addr);
  uint8_t txStatus = Wire.endTransmission(true);
  if (txStatus == 0){
    // device ACKed; try to read
    size_t got = Wire.requestFrom((int)addr, (int)REQ_BYTES, (int)true);
    if (got > 0){
      int idx = idxFromAddr(addr);
      if (idx >= 0){
        // read exactly 'got' bytes, filter non-printables
        for (size_t i = 0; i < got; ++i){
          char c = (char)Wire.read();
          if (!isPrintableAscii(c)) continue;     // drop 0x00/0xFF/etc.
          rxBuf[idx] += c;
        }
        // extract full lines ending with '\n'
        for (;;){
          int nl = rxBuf[idx].indexOf('\n');
          if (nl < 0) break;
          String line = rxBuf[idx].substring(0, nl);
          rxBuf[idx].remove(0, nl + 1);

          // sanity check: only accept our protocol lines
          if (line.startsWith("f/")){
            lastMsg[idx] = line;
            Serial.println(line);
          }
        }
      }
    }
  }

  // example: show cached message for 0x12 once per second
  static uint32_t tEcho = 0;
  if (millis() - tEcho > 1000){
    int i = idxFromAddr(0x12);
    if (i >= 0 && lastMsg[i].length()){
      Serial.print("[cached 0x12] ");
      Serial.println(lastMsg[i]);
    }
    tEcho = millis();
  }

  // next address
  addr++;
  if (addr > ADDR_MAX) addr = ADDR_MIN;
  delay(POLL_DELAY_MS);
}
