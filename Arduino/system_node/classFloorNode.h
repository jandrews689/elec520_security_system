#ifndef CLASS_FLOOR_NODE
#define CLASS_FLOOR_NODE

#include <cstdint>
#include <PubSubClient.h>
#include <WiFi.h>
#include "elec520_protocol.h"

#define NUM_SAMPLES 50

class classFloorNode {
//*********************************************************************************************** */
//PRIVATE///////////////////////////////////////////////////////////////////////////////////////////
private:
    // Singleton-style instance pointer for static callbacks
    static classFloorNode* instance;

    //Wifi setup variables
    const char* _ssid;
    const char* _password;
    const char* _mqtt_server; 
    int _mqtt_port; 
    const char* _mqtt_client_id; 
    WiFiClient espClient;

    //MQTT setup variables
    PubSubClient client;

    byte _bFloorID = 0b0000'0001;
    int _iNumOfFloors; //Need to figure out a way to define this.

    // Message structure
    typedef struct struct_message {
        char topic[64];
        char payload[250];        //value contained in message
        uint8_t src_addr[6];      //source address (used for RX messages not TX messages)
    } struct_message;


    unsigned long startTime;
    byte bTransmitPosition = 0b0000'0001;
    uint8_t _uiNumRoom;


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


    //Set up the wifi with the cloud. 
    void setup_wifi();


public:

    // Constructor
    classFloorNode(const char* ssid,
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


    //Network setup
    void setupNetwork();


    //Sets the number of rooms in the system. Used for sending the correct amount of esp now messages per floor. 
    void setNumberOfRooms(int number);


    //MQTT loop
    void mqttOperate();



};

// Define static instance pointer
inline classFloorNode* classFloorNode::instance = nullptr;

#endif