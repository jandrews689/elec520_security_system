#include <elec520_nano.h>
#include <elec520_protocol.h>
#include "classFloorNode.h"
#include <Arduino.h>
#include <Wire.h>
#include <WString.h>

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

const char* ssid = "Joe's S23 Ultra"; 
const char* password = "joea12345"; 
const char* mqtt_server = "broker.hivemq.com"; 
int mqtt_port = 1883; 
const char* mqtt_client_id = "BaseStation";

classFloorNode objFloor(ssid, password, mqtt_server, mqtt_port, mqtt_client_id);

bool xCloudConnectionNode = false;
// bool xI2CComplete = false;

// String buildStringFromChars(char incomingChar, bool &messageComplete) {
//   static String buffer = "";
  
//   // Reset completion flag
//   messageComplete = false;

//   if (incomingChar == '\n') {
//     // End of message — mark complete
//     messageComplete = true;
//     String completeMessage = buffer;
//     buffer = ""; // reset for next message
//     return completeMessage;
//   } else if (incomingChar != '\r') {
//     // Append normal characters (ignore carriage returns)
//     buffer += incomingChar;
//   }

//   // Not complete yet, return empty
//   return "";
// }


//SETUP////////////////////////////////////////////////////////////////////////////////
void setup() {
  //nano setup
  Serial.begin(115200);
  delay(100);

  Wire.begin(SDA_PIN, SCL_PIN); // 100 kHz
  Wire.setTimeOut(50);

  Serial.println("ESP32 I2C master started. Polling 0x12..0x20");


  //ESP setup
  objFloor.setupNetwork();

  //Setup the floor
  objFloor.setFloorID(1);
  objFloor.setNumberOfRooms(2);
  objFloor.setNumOfFloors(2);

  //floor 1
  addFloor  (objFloor.getFloorID());

    addRoom   (objFloor.getFloorID(), 1);

      addUltra  (objFloor.getFloorID(), 1, 1);

      addHall   (objFloor.getFloorID(), 1, 1);
      addHall   (objFloor.getFloorID(), 1, 2);

    addRoom   (objFloor.getFloorID(), 2);

      addUltra  (objFloor.getFloorID(), 2, 1);

      addHall   (objFloor.getFloorID(), 2, 1);
      addHall   (objFloor.getFloorID(), 2, 2);

}

// 0b0000_0000


//LOOP/////////////////////////////////////////////////////////////////////////////////////////
void loop() {
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
            // parseRoomEspString(line); //protocol process
            parseTokenLine(line);
          }
        }
      }
    }
  }
  // Move to next address
  addr++;
  if (addr > ADDR_MAX) addr = ADDR_MIN;

  // // Small delay so we’re not hammering the bus
  delay(10);
  
  // Test without I2C/////////////////////////////////////////
    // String nanoHallTest = nanoTokenHall(1, 1, 1, 1);
    // Serial.print(nanoHallTest);
    // Serial.println();
    // parseTokenLine(nanoHallTest);
  /////////////////////////////////////////////////////////////

  //gen esp string and sending over esp
  objFloor.transmitWindow();

  //Mqtt pub and sub
  if (objFloor.getFloorID() == 0b0000'0001) objFloor.mqttOperate();
  
  int count++;
  if (count > 500) {
    count = 0;
    //Spew everything onto the serial. 
    debugPrintModel(Serial);
  }


}
