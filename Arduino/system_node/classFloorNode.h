#ifndef CLASS_FLOOR_NODE
#define CLASS_FLOOR_NODE

#include <cstdint>
#include <PubSubClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
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
    int message_ID = 0;
    WiFiClient espClient;

    //MQTT setup variables
    PubSubClient client;

    byte _bFloorID = 0b0000'0001;
    int iNumOfFloors = 2; //Need to figure out a way to define this.

    // Message structure
    typedef struct struct_message {
        char topic[64];
        char payload[250];        //value contained in message
        uint8_t src_addr[6];      //source address (used for RX messages not TX messages)
    } struct_message;

    struct_message strucTXMessage;
    struct_message strucRXMessage;
    esp_now_peer_info_t peerInfo = {}; //ESP Peer information.

    // Peer Node structure
    typedef struct peer_node_data {
        uint32_t uiMacAddress;
    } peer_node_data;

    unsigned long startTime;
    byte bTransmitPosition = 0b0000'0001;
    uint8_t _uiNumRoom;


    // --- Static callbacks that forward into the instance ---
    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length);
    static void onSentStatic(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status);
    static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len);


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


    //ESP NOW//////////////////////////////////////////////////////////////////////////////////////////
    //Set up the esp now for peer to peer communication.
    bool setup_espnow();


    //ESP send callback. 
    void onESPSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status);


    //Esp now receive call back function. 
    void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len);


    //Send esp now message
    void sendEspNowMsg(struct_message &message);


public:

    // Constructor
    classFloorNode(const char* ssid = "Joe's S23 Ultra",
                   const char* password = "joea12345",
                   const char* mqtt_server = "broker.hivemq.com",
                   int mqtt_port = 1883,
                   const char* mqtt_client_id = "ESP32_BaseStation")
        : _ssid(ssid), _password(password),
          _mqtt_server(mqtt_server), _mqtt_port(mqtt_port),
          _mqtt_client_id(mqtt_client_id), client(espClient);


    //Set the Floor ID
    void setFloorID(byte id);


    //Get the Floor ID
    byte getFloorID();


    //Set the number of floors in the system
    void setNumOfFloors(int value);


    //Network setup
    void setupNetwork();


    //Sets the number of rooms in the system. Used for sending the correct amount of esp now messages per floor. 
    void setNumberOfRooms(int number);


    //Send floor data over esp now
    void sendFloorData();


    //Transmit window 
    void transmitWindow();


    //MQTT loop
    void mqttOperate();



};

// Define static instance pointer
inline classFloorNode* classFloorNode::instance = nullptr;

#endif