// #include <ArduinoJson.h>
// #include <ArduinoJson.hpp>
// #include <json_formatting.h>
// #include <WiFi.h>
// #include <esp_now.h>
// #include <esp_wifi.h>
#include <networkSetup.h>




//SETUP////////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);

  // //Setups ESP NOW
  // while (!setup_espnow()) {
  //   delay(500);
  //   Serial.print("Waiting for setup of ESP device:");
  //   Serial.println(WiFi.macAddress());
  // }

  
  setup_wifi();
  setup_espnow();

  //Sends a ESP NOW message
  esp_startup_ping();
}


//LOOP/////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  // //ESP NOW /////////////////////////////////////////////////////////////////////////////////
  //   // Prepare message
  // myData.id = myData.id+1;
  // myData.value = random(0, 100) / 1.0;

  // // Broadcast to all peers
  // esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *) &myData, sizeof(myData));
  // if (result == ESP_OK) {
  //   Serial.println("Queued Message Sent OK");
  // } else {
  //   Serial.printf("Error: esp_now_send failed with error: %d\n", result);
  // }

  // delay(2000);

  // //CLOUD MQTT/////////////////////////////////////////////////////////////////////////////////////
  // //Re-establish the connection to the broker. 
  // if (!client.connected()) {
  //   brokerReconnect();
  // }
  // client.loop();

  // //Publish system status every 1 seconds
  // static unsigned long lastSentMsg = 0;
  // if (millis() - lastSentMsg > 1000) {
  //   lastSentMsg = millis();

  //   // //build snapshot of the MQTT data held in MODEL
  //   // String BaseStationPublishData = buildSnapshot();
  //   // String topicEverythingString = topicEverything();
  //   // //publish the data to the broker. 
  //   // client.publish(topicEverythingString.c_str(), BaseStationPublishData.c_str());
  //   // Serial.println(BaseStationPublishData + String(message_ID));
    
  //   client.publish("home/security/COM4", "Connection Successful");
  //   Serial.println("Status published, Connection Successful:" + String(message_ID));
  //   message_ID ++;
  // }
}
