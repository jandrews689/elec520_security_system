#include <classFloorNode.h>


//NETWORKING////////////////////////////////////////////////////////////////////////////////////////

//MQTT Get client data. 
PubSubClient& classFloorNode::getClient() { return client; }


//WIFI Set the Wifi Credentials
void classFloorNode::setWifiNetworkCredentials(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
}


//MQTT callback function
void classFloorNode::MqttCallBack(char* topicC, byte* payload, unsigned int length) {
    static char buf[256];
    unsigned int n = (length < sizeof(buf)-1) ? length : sizeof(buf)-1;
    memcpy(buf, payload, n);
    buf[n] = '\0';

    //Parse the Mqtt data into MODEL. 
    // parseSystemMqttString(String(buf));

    
    if (parseCloud(topicC, buf);){
        Serial.printf("MQTT Callback cloud [%s]: %s\n", topicC, buf);
    }

    Serial.printf("MQTT Callback ALL [%s]: %s\n", topicC, buf);
}


//MQTT Set client data.
void classFloorNode::setMQTTClientData(const char* mqtt_server, int mqtt_port, const char* mqtt_client_id) {
    _mqtt_server = mqtt_server;
    _mqtt_port = mqtt_port;
    _mqtt_client_id = mqtt_client_id;
}


//MQTT Reconnect with the broker
void classFloorNode::brokerReconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...\n");
        if (client.connect(_mqtt_client_id)) {
            Serial.println("connected");
            client.subscribe("ELEC520/security/#");
        } else {
            Serial.printf("failed, rc=%d. retry in 5s\n", client.state());
            delay(5000);
        }
    }
}


//Set up the wifi with the cloud. 
void classFloorNode::setup_wifi() {
    delay(10);
    Serial.printf("Connecting to %s\n", _ssid);
    WiFi.begin(_ssid, _password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.printf("\nWiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());

    client.setServer(_mqtt_server, _mqtt_port);
    client.setCallback(mqttCallbackStatic);
}


//*********************************************************************************************** */
//PUBLIC////////////////////////////////////////////////////////////////////////////////////////////

// Constructor
classFloorNode::classFloorNode(const char* ssid = "Joe's S23 Ultra",
                const char* password = "joea12345",
                const char* mqtt_server = "broker.hivemq.com",
                int mqtt_port = 1883,
                const char* mqtt_client_id = "ESP32_BaseStation")
    : _ssid(ssid), _password(password),
        _mqtt_server(mqtt_server), _mqtt_port(mqtt_port),
        _mqtt_client_id(mqtt_client_id), client(espClient) {

    instance = this;  // set singleton pointer
    _uiNumRoom = 0;
    _iNumOfFloors = 1;
    
}


//Set the Floor ID, helper function to make system buildering easier to read. 
void classFloorNode::setFloorID(byte id){_bFloorID = id;}

//Get the Floor ID
byte classFloorNode::getFloorID(){return _bFloorID;}


//Sets the number of rooms in the system. Used for sending the correct amount of esp now messages per floor. 
void classFloorNode::setNumOfFloors(int value){
    _iNumOfFloors = value;
}

//NETWORKING/////////////////////////////////////////////////////////////////////////////
//Network setup. 
void classFloorNode::setupNetwork() {
    setup_wifi();
}

//Sets the number of rooms in the system. Used for sending the correct amount of esp now messages per floor. 
void classFloorNode::setNumberOfRooms(int number){
    _uiNumRoom = number;
}


// //Transmit system data
// void classFloorNode::sendSystemData(){
    
// }


//MQTT loop
void classFloorNode::mqttOperate(){
    if (!client.connected()) {
        brokerReconnect();
    }
    client.loop();

    // Example publish system status every 5 seconds
    static unsigned long publishMsg = 0;
    if (millis() - publishMsg > 5000) {
        publishMsg = millis();
        
        //Publish to client. 
        String topic;
        String payload;

        //Publish system data


        //Publish floor data
        for (int i=1; i<_iNumOfFloors+1; i++){
            topic = cloudTopicFloor(i);
            // topic = "ELEC520/security";
            payload = buildFloorMqttString(i);
            client.publish(topic.c_str(), payload.c_str());
            Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());
            delay(20);
        }
    }

}
