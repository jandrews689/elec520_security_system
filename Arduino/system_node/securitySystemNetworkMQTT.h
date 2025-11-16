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
 *  - All code is original work unless stated otherwise.
 *******************************************************************************************/


#ifndef CLASS_SECSYS_NETWORK_MQTT
#define CLASS_SECSYS_NETWORK_MQTT

#include <cstdint>
#include <PubSubClient.h>
#include <WiFi.h>
#include "elec520_protocol.h"

#define NUM_SAMPLES 50

class securitySystemNetworkMQTT {
//*********************************************************************************************** */
//PRIVATE///////////////////////////////////////////////////////////////////////////////////////////
private:
    // Singleton-style instance pointer for static callbacks
    static securitySystemNetworkMQTT* instance;

    //Wifi setup variables
    const char* _ssid;
    const char* _password;
    const char* _mqtt_server; 
    int _mqtt_port; 
    const char* _mqtt_client_id; 
    WiFiClient espClient;

    //MQTT setup variables
    PubSubClient client;

    //System setup
    // byte _bFloorID = 0b0000'0001;

    // Message structure
    typedef struct struct_message {
        char topic[64];
        char payload[250];        //value contained in message
        uint8_t src_addr[6];      //source address (used for RX messages not TX messages)
    } struct_message;


    // --- Static callbacks that forward into the instance ---
    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length) {
        if (instance) instance->MqttCallBack(topic, payload, length);
    }


    //MQTT Get client data. 
    PubSubClient& getClient();


    //WIFI Set the Wifi Credentials
    void setWifiNetworkCredentials(const char* ssid, const char* password);


    //MQTT callback function
    void MqttCallBack(char* topicC, byte* payload, unsigned int length);


    //MQTT Set client data.
    void setMQTTClientData(const char* mqtt_server, int mqtt_port, const char* mqtt_client_id);


    //MQTT Reconnect with the broker
    void brokerReconnect();


public:

    // Constructor
    securitySystemNetworkMQTT(const char* ssid,
                   const char* password,
                   const char* mqtt_server,
                   int mqtt_port,
                   const char* mqtt_client_id);


    // //Set the Floor ID, helper function to make system buildering easier to read.
    // void setFloorID(byte id);
    //
    //
    // //Get the Floor ID
    // byte getFloorID();




    
    //Set up the wifi with the cloud. 
    void setup_wifi();


    //MQTT loop
    void mqttOperate();


    //Security system alarm state machine.
    void alarmSystemStateMachine();


    //Trigger LED and Buzzer if system state == ALARM
    void triggerAlarm();



};

// Define static instance pointer
inline securitySystemNetworkMQTT* securitySystemNetworkMQTT::instance = nullptr;

#endif



#define ULTRA_THRESHOLDS 100

/*
        6) System setup and configuration. Place into configuration file which all nodes read from to build system.
        JOSH 7) keypad data entry checking of user passwords and access to system state.
        8) Store password and user into local database.
    */

