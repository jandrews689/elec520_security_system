#include "classFloorNode.h"

const char* ssid = "Joe's S23 Ultra"; 
const char* password = "joea12345"; 
const char* mqtt_server = "broker.hivemq.com"; 
int mqtt_port = 1883; 
const char* mqtt_client_id = "BaseStation";

classFloorNode objFloor(ssid, password, mqtt_server, mqtt_port, mqtt_client_id);

bool xCloudConnectionNode = false;

//SETUP////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);

  objFloor.setupNetwork();

  //Setup the floor
  objFloor.setFloorID(0b0000'0001);
  objFloor.setNumberOfRooms(2);
  objFloor.setNumOfFloors(2);

  //floor 1
  addFloor  (objFloor.getFloorID());

    addRoom   (objFloor.getFloorID(), 0b0000'0001);

      addUltra  (objFloor.getFloorID(), 0b0000'0001, 0b0000'0001);

      addHall   (objFloor.getFloorID(), 0b0000'0001, 0b0000'0001);
      addHall   (objFloor.getFloorID(), 0b0000'0001, 0b0000'0010);

    addRoom   (objFloor.getFloorID(), 0b0000'0010);

      addUltra  (objFloor.getFloorID(), 0b0000'0010, 0b0000'0001);

      addHall   (objFloor.getFloorID(), 0b0000'0010, 0b0000'0001);
      addHall   (objFloor.getFloorID(), 0b0000'0010, 0b0000'0010);



  



}

// 0b0000_0000


//LOOP/////////////////////////////////////////////////////////////////////////////////////////
void loop() {

  //i2c
  // objFloor.updateFloorData();
  //setstring fx -> model


  //gen esp string and sending over esp
  objFloor.transmitWindow();

  //Mqtt pub and sub
  if (objFloor.getFloorID() == 0b0000'0001) objFloor.mqttOperate();
  
  

}
