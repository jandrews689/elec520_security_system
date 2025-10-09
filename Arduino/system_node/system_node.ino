#include "classFloorNode.h"

const char* ssid = "Joe's S23 Ultra"; 
const char* password = "joea12345"; 
const char* mqtt_server = "broker.hivemq.com"; 
int mqtt_port = 1883; 
const char* mqtt_client_id = "ESP32_BaseStation";

classFloorNode objFloor(ssid, password, mqtt_server, mqtt_port, mqtt_client_id);

bool xCloudConnectionNode = false;

//SETUP////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);

  objFloor.setupNetwork();

  //Setup the floor
  objFloor.setNumberOfRooms(2);

  addFloor  (0b0000'0001);

  addRoom   (0b0000'0001, 0b0000'0001);

  addUltra  (0b0000'0001, 0b0000'0001, 0b0000'0001);

  addHall   (0b0000'0001, 0b0000'0001, 0b0000'0001);
  addHall   (0b0000'0001, 0b0000'0001, 0b0000'0010);

  addRoom   (0b0000'0001, 0b0000'0010);

  addUltra  (0b0000'0001, 0b0000'0010, 0b0000'0001);

  addHall   (0b0000'0001, 0b0000'0010, 0b0000'0001);
  addHall   (0b0000'0001, 0b0000'0010, 0b0000'0010);



  



}

// 0b0000_0000


//LOOP/////////////////////////////////////////////////////////////////////////////////////////
void loop() {

  objFloor.sendFloorData();
  

}
