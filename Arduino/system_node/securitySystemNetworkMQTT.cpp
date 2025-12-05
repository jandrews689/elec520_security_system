/*******************************************************************************************
 * Project:      ELEC520 - Distributed and Interactive Systems Coursework - Security System
 * File:         securitySystemNetworkMQTT
 * Description:
 *
 * Authors:      Joseph Andrews
 * Created:      November 2025
 *
 * Notes:
 *  - This file is part of the ELEC520 coursework project.
 *******************************************************************************************/


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

    if (bool messageRX = parseCloud(topicC, buf)) Serial.printf("MQTT Callback [%s]: %s\n", topicC, buf);

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
        for (int i=1; i<MODEL.numOfFloors+1; i++){

            topic = cloudTopicFloor(i);
            // topic = "ELEC520/security";
            payload = buildFloorMqttString(i);
            client.publish(topic.c_str(), payload.c_str());
            Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());
            delay(20);
        }
    }

}


//Security system alarm state machine.
void securitySystemNetworkMQTT::alarmSystemStateMachine() {
    String topic;
    String payload;

    //If elected leader then control of systemstate machine and triggering of alarms.
    if (WiFi.macAddress() == MODEL.mac ) {
        if (MODEL.systemState == SystemState::ARMED) {
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
        }
    }
}


//Trigger LED and Buzzer if system state == ALARM
void securitySystemNetworkMQTT::triggerAlarm() {
    if (MODEL.systemState == SystemState::ALARM) {
        digitalWrite(23, HIGH);
        static unsigned long publishMsg = 0;
        if (millis() - publishMsg > 1000) {
            publishMsg = millis();
            String alarmLocation = MODEL.triggerLoc;
            Serial.printf("ALARM TRIGGERED in zone %s \n", alarmLocation.c_str());
        }
    } else {
        digitalWrite(23, LOW);
    }

}
