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
    byte _bFloorID = 0b0000'0001;
    int _iNumOfFloors;

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


    //Set the Floor ID, helper function to make system buildering easier to read. 
    void setFloorID(byte id);


    //Get the Floor ID
    byte getFloorID();


    //Set the number of floors in the system, used by MMQT 
    void setNumOfFloors(int value);

    
    //Set up the wifi with the cloud. 
    void setup_wifi();


    //MQTT loop
    void mqttOperate();


    //Security system alarm state machine.
    void alarmSystemStateMachine();

    void triggerAlarm();

};

// Define static instance pointer
inline securitySystemNetworkMQTT* securitySystemNetworkMQTT::instance = nullptr;

#endif



#define ULTRA_THRESHOLDS 100

/*
        done 1) Check sensor data against limits and system state. If ARMED and exceed limit then trigger event.
        done 2) Check for trigger events
            done 2) Location ID should be published to cloud during trigger event.
            3) Trigger event causes alarm buzzer and LED to be set off. Maybe configure certain pins on esp32 for buzzer and led.
        4) configure certain pins for keypad connection as wel.
        5) MMQT messaging with the cloud (not part of this class)
        6) System setup and configuration. Place into configuration file which all nodes read from to build system.
        7) keypad data entry checking of user passwords and access to system state.
        8) Store password and user into local database.
    */

    // Functions




    // Check trigger loop, cycles through the model data and compares with pre-configured values.
        //set the trigger threshold as hard coded for now, but later add this to the config file.

    // Functions for setting and getting the system state in prep for the keypad.

    // Keypad input will bypass the coms protocol, Josh to implement a function which will take the keypad data and then
        //call the password compare function.