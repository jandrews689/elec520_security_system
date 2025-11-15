#include <securitySystemNetworkMQTT.h>


//NETWORKING////////////////////////////////////////////////////////////////////////////////////////

//MQTT Get client data. 
PubSubClient& securitySystemNetworkMQTT::getClient() { return client; }


//WIFI Set the Wifi Credentials
void securitySystemNetworkMQTT::setWifiNetworkCredentials(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
}


//MQTT callback function
void securitySystemNetworkMQTT::MqttCallBack(char* topicC, byte* payload, unsigned int length) {
    static char buf[256];
    unsigned int n = (length < sizeof(buf)-1) ? length : sizeof(buf)-1;
    memcpy(buf, payload, n);
    buf[n] = '\0';

    parseCloud(topicC, buf);
        
    Serial.printf("MQTT Callback [%s]: %s\n", topicC, buf);

}


//MQTT Set client data.
void securitySystemNetworkMQTT::setMQTTClientData(const char* mqtt_server, int mqtt_port, const char* mqtt_client_id) {
    _mqtt_server = mqtt_server;
    _mqtt_port = mqtt_port;
    _mqtt_client_id = mqtt_client_id;
}


//MQTT Reconnect with the broker
void securitySystemNetworkMQTT::brokerReconnect() {
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
void securitySystemNetworkMQTT::setup_wifi() {
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
securitySystemNetworkMQTT::securitySystemNetworkMQTT(const char* ssid = "Joe's S23 Ultra",
                const char* password = "joea12345",
                const char* mqtt_server = "broker.hivemq.com",
                int mqtt_port = 1883,
                const char* mqtt_client_id = "ESP32_BaseStation")
    : _ssid(ssid), _password(password),
        _mqtt_server(mqtt_server), _mqtt_port(mqtt_port),
        _mqtt_client_id(mqtt_client_id), client(espClient) {

    instance = this;  // set singleton pointer
    _iNumOfFloors = 1;
    
}


//Set the Floor ID, helper function to make system buildering easier to read. 
void securitySystemNetworkMQTT::setFloorID(byte id){_bFloorID = id;}


//Get the Floor ID
byte securitySystemNetworkMQTT::getFloorID(){return _bFloorID;}


//Sets the number of rooms in the system. Used for sending the correct amount of esp now messages per floor. 
void securitySystemNetworkMQTT::setNumOfFloors(int value){
    _iNumOfFloors = value;
}

//NETWORKING//////////////////////////////////////////////////////////////////////////


//MQTT loop
void securitySystemNetworkMQTT::mqttOperate(){
    if (!client.connected()) {
        brokerReconnect();
    }
    client.loop();

    // Example publish system status every 5 seconds
    static unsigned long publishMsg = 0;
    if (millis() - publishMsg > 5000) {
        publishMsg = millis();
        String topic;
        String payload;

        //Publish system state
        topic = cloudTopicSystemState();
        payload = MODEL.systemState;
        client.publish(topic.c_str(), payload.c_str());
        Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());

        //Publish keypad status
        topic = cloudTopicKeypad();
        payload = MODEL.keypad;
        client.publish(topic.c_str(), payload.c_str());
        Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());

        //Publish network status
        topic = cloudTopicNetwork();
        payload = MODEL.network;
        client.publish(topic.c_str(), payload.c_str());
        Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());

        //Publish Base Station mac address
        topic = cloudTopicMac();
        payload = MODEL.mac;
        client.publish(topic.c_str(), payload.c_str());
        Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());

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


void securitySystemNetworkMQTT::alarmSystemStateMachine() {
    String topic;
    String payload;

    //If elected leader then control of systemstate machine and triggering of alarms.
    if (WiFi.macAddress() == MODEL.mac ) {
        switch (uint8_t alarm = MODEL.systemState) {
            case SystemState::DISARMED:
                break;
            case SystemState::ARMED:
                for (int i = 0; i < SMP_MAX_FLOORS-1; i++) {
                    if (MODEL.floors[i].used == true) {
                        for (int j = 0; j < SMP_MAX_ROOMS-1; j++) {
                            if (MODEL.floors[i].rooms[j].used == true) {
                                for (int k = 0; k < SMP_MAX_SENSORS-1; k++) {
                                    if (MODEL.floors[i].rooms[j].hall[k].open == true) {
                                        setTriggerLoc(i,j,0,k);
                                        //send mqtt message
                                        topic = cloudTopicTrigger();
                                        payload = MODEL.triggerLoc;
                                        client.publish(topic.c_str(), payload.c_str());
                                        Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());
                                    }
                                    if (MODEL.floors[i].rooms[j].ultra[k].value >= ULTRA_THRESHOLDS) {
                                        setTriggerLoc(i,j,k,0);
                                        //send mqtt message
                                        topic = cloudTopicTrigger();
                                        payload = MODEL.triggerLoc;
                                        client.publish(topic.c_str(), payload.c_str());
                                        Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            case SystemState::ALARM:
                //ESP32 buzzer and light illuminate
                break;
            default:
                alarm = SystemState::DISARMED;
                break;
        }
    }
}


void securitySystemNetworkMQTT::triggerAlarm() {
    if (MODEL.systemState == SystemState::ALARM) {
        //ESP32 LED FLASH
        digitalWrite(25, HIGH);
        //ESP32 BUZZER SOUND


        static unsigned long publishMsg = 0;
        if (millis() - publishMsg > 1000) {
            publishMsg = millis();
            Serial.printf("ALARM TRIGGERED");
        }
    }

}
