#include <ArduinoJson.h>
#include <ArduinoJson.hpp>
#include <json_formatting.h>
#include <networkSetup.h>
#include <classFloorNode.h>

classFloorNode objFloorNode;


void setup(){
  Serial.begin(115200);

  // Assign Mac address
  objFloorNode.setMacAddress(macAddress);
  // Establish cloud connection
  setup_wifi();
                                    //ESP Now callback function here


  client.setServer(mqtt_server, mqtt_port);
  client.subscribe(topicEverything().c_str()); //Subscribe to full system from broker. 
  client.setCallback(MqttCallBack); //MQTT callback for subscribed topics. 
}

void loop(){
  //Once the ESP Now connection has been established, then define device ID.

}

//OLD SYSTEM//////////////////////////////
// void setup() {
//   Serial.begin(115200);

//   //1) Setup and connecto WIFI to MQTT broker. 
//   setup_wifi();
//   client.setServer(mqtt_server, mqtt_port);

//   client.subscribe(topicEverything().c_str()); //Subscribe to full system from broker. 
//   client.setCallback(MqttCallBack); //MQTT callback for subscribed topics. 
//                                     //ESP Now callback function here
// }

// void loop() {
//   //Re-establish the connection to the broker. 
//   if (!client.connected()) {
//     brokerReconnect();
//   }
//   client.loop();

//   //Publish system status every 1 seconds
//   static unsigned long lastSentMsg = 0;
//   if (millis() - lastSentMsg > 1000) {
//     lastSentMsg = millis();

//     //build snapshot of the MQTT data held in MODEL
//     String BaseStationPublishData = buildSnapshot();
//     String topicEverythingString = topicEverything();
//     //publish the data to the broker. 
//     client.publish(topicEverythingString.c_str(), BaseStationPublishData.c_str());
//     Serial.println(BaseStationPublishData + String(message_ID));
    
//     // client.publish("home/security/status", "System Armed");
//     // Serial.println("Status published: System Armed. Message Count:" + String(message_ID));
//     message_ID ++;
//   }


// }




// //ESP Now callback function - Not sure how this works yet so this is just example.
// void espNowCallback(char* topicC, char* payloadC){

//   //Parse the data into the MODEL global variable. 
//   parseMqtt(topicC, payloadC);
// }


// void securitySystemSetup(){
//   //System floor, room, sensor setup
//   addFloor(100);

//     addRoom(100, 101);
//       addUltrasonic(100, 101, 101);
//       addHall(100, 101, 102);

//     addRoom(100, 102);
//       addUltrasonic(100, 102, 103);
//       addHall(100, 102, 104);

//   addFloor(200);
//     addRoom(200, 201);
//       addUltrasonic(200, 201, 201);
//       addHall(200, 201, 202);

//     addRoom(200, 202);
//       addUltrasonic(200, 202, 203);
//       addHall(200, 202, 204);
// }