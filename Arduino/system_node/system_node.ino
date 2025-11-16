// #include <ArduinoJson.h>
// #include <ArduinoJson.hpp>

#include <elec520_nano.h>
#include <elec520_protocol.h>
#include "securitySystemNetworkMQTT.h"
#include "baseStation.h"
#include "i2c.h"
#include <Arduino.h>
#include <WString.h>



const char* ssid = "Joe's S23 Ultra"; 
const char* password = "joea12345"; 
const char* mqtt_server = "broker.hivemq.com"; 
int mqtt_port = 1883; 
const char* mqtt_client_id = "BaseStation";

securitySystemNetworkMQTT objFloor(ssid, password, mqtt_server, mqtt_port, mqtt_client_id);

int count = 0;
bool mqttSystemDebug = true;


//SETUP////////////////////////////////////////////////////////////////////////////////
void setup() {
  pinMode(23, OUTPUT);

  //nano setup
  Serial.begin(115200);
  delay(100);

  //I2C SETUP//////////////////////////////////////////////////////////
  i2cSetup();

  // //ESP setup///////////////////////////////////////////////////////////
  objFloor.setup_wifi();

  //Security Architecture Setup//////////////////////////////////////////
  //Setup the floor
  objFloor.setFloorID(1);
  objFloor.setNumOfFloors(1);

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
  //////////////////////////////////////////////////////////////////////

}



//LOOP/////////////////////////////////////////////////////////////////////////////////////////
void loop() {

    //i2C////////////////////////////////////////////////////////////////////////////////
    i2cOperation();
    //ESP32/////////////////////////////////////////////////////////////////////////////


    //MQTT//////////////////////////////////////////////////////////////////////////////
    if (objFloor.getFloorID() == 0b0000'0001) objFloor.mqttOperate();


    //MQTT System Serial Print Debugging
    if (mqttSystemDebug) {
     count++;
     if (count > 500) {
       count = 0;
       //Spew everything onto the serial.
       debugPrintModel(Serial);
     }
    }


    //SYSTEM STATE////////////////////////////////////////////////////////////////////////
    objFloor.alarmSystemStateMachine();

    //Trigger alarm
    objFloor.triggerAlarm();


}
