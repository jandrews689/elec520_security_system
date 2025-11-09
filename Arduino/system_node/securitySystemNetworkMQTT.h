#ifndef CLASS_SECSYS_NETOWRK_MQTT
#define CLASS_SECSYS_NETOWRK_MQTT

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

};

// Define static instance pointer
inline securitySystemNetworkMQTT* securitySystemNetworkMQTT::instance = nullptr;

#endif