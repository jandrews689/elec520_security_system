#include <elec520_nano.h>
#include <elec520_protocol.h>
#include "classFloorNode.h"
#include <Arduino.h>
#include <Wire.h>
#include <WString.h>

#define SDA_PIN   21
#define SCL_PIN   22

// I2C address range to scan (room 1-8)
#define ADDR_MIN  0x12
#define ADDR_MAX  0x20

const char* ssid = "Joe's S23 Ultra"; 
const char* password = "joea12345"; 
const char* mqtt_server = "broker.hivemq.com"; 
int mqtt_port = 1883; 
const char* mqtt_client_id = "BaseStation";

classFloorNode objFloor(ssid, password, mqtt_server, mqtt_port, mqtt_client_id);

bool xCloudConnectionNode = false;
bool xI2CComplete = false;

String buildStringFromChars(char incomingChar, bool &messageComplete) {
  static String buffer = "";
  
  // Reset completion flag
  messageComplete = false;

  if (incomingChar == '\n') {
    // End of message — mark complete
    messageComplete = true;
    String completeMessage = buffer;
    buffer = ""; // reset for next message
    return completeMessage;
  } else if (incomingChar != '\r') {
    // Append normal characters (ignore carriage returns)
    buffer += incomingChar;
  }

  // Not complete yet, return empty
  return "";
}


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
        char nanoData = (char)Wire.read();
        String msg = buildStringFromChars(nanoData, xI2CComplete);
        if (xI2CComplete){
          Serial.print(msg);
          parseRoomEspString(msg);
        }
        
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

  //i2c
  // objFloor.updateFloorData();
  //setstring fx -> model


  //convert char to string;




  //gen esp string and sending over esp
  objFloor.transmitWindow();

  //Mqtt pub and sub
  if (objFloor.getFloorID() == 0b0000'0001) objFloor.mqttOperate();
  
  

}
