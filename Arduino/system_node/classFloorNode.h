#ifndef CLASS_FLOOR_NODE
#define CLASS_FLOOR_NODE

#include <cstdint>
#include <PubSubClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "json_formatting.h"

#define NUM_SAMPLES 50

class classFloorNode {
private:
    // Singleton-style instance pointer for static callbacks
    static classFloorNode* instance;

    // int _iNodeID;
    uint8_t _auiMacAddress[6];
    ProtocolModel _MODEL;

    const char* _ssid;
    const char* _password;
    const char* _mqtt_server; 
    int _mqtt_port; 
    const char* _mqtt_client_id; 
    int message_ID = 0;

    WiFiClient espClient;
    PubSubClient client;

    int rssiSamples[NUM_SAMPLES];
    int sampleIndex = 0;
    bool bufferFilled = false;
    float avgRSSI = 0.0;

    int _iFloorID = 0; //need to update this value frrom the peermacaddressstorage. 


    // Message structure
    typedef struct struct_message {
        int id = 1;
        char value[250];
        uint8_t src_addr[6];
    } struct_message;

    struct_message strucTXMessage;
    struct_message strucRXMessage;

    esp_now_peer_info_t peerInfo = {};          //ESP Peer information.

    // Peer Node structure
    typedef struct peer_node_data {
        int id = -1;
        uint32_t uiMacAddress;
    } peer_node_data;

    peer_node_data uiPeerMacAddressStorage[8] = {};   //Peer address storage.
    int iPA = 0;                                //Peer address storage element.

    unsigned long startTime;
    uint8_t uiTransmitPosition = 0;




    // --- Static callbacks that forward into the instance ---
    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length) {
        if (instance) instance->MqttCallBack(topic, payload, length);
    }

    static void onSentStatic(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
        if (instance) instance->onESPSent(mac_addr, status);
    }

    static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
        if (instance) instance->onReceive(info, data, len);
    }

    //Set up the wifi with the cloud. 
    void setup_wifi() {
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

    //Set up the esp now for peer to peer communication.
    bool setup_espnow() {
        startTime = millis();
        WiFi.mode(WIFI_STA);
        if (esp_now_init() != ESP_OK) {
            Serial.println("Error initializing ESP-NOW");
            return false;
        }
        Serial.println("Initialization ESP-NOW Successful");

        esp_now_register_send_cb(onSentStatic);
        esp_now_register_recv_cb(onReceiveStatic);

        memset(peerInfo.peer_addr, 0xFF, 6);
        peerInfo.channel = WiFi.channel();
        peerInfo.encrypt = false;
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
            Serial.println("Failed to add peer");
            return false;
        }
        Serial.println("Peer 0xFF successfully Added. Transmitting to all Nodes!");
        return true;
    }

public:

    // Constructor
    classFloorNode(const char* ssid = "Joe's S23 Ultra",
                   const char* password = "joea12345",
                   const char* mqtt_server = "broker.hivemq.com",
                   int mqtt_port = 1883,
                   const char* mqtt_client_id = "ESP32_BaseStation")
        : _ssid(ssid), _password(password),
          _mqtt_server(mqtt_server), _mqtt_port(mqtt_port),
          _mqtt_client_id(mqtt_client_id), client(espClient) {
        instance = this;  // set singleton pointer
        for (int i = 0; i < 6; i++) _auiMacAddress[i] = 0;
    }

    
     //Network setup. 
    void setupNetwork() {
        setup_wifi();
        setup_espnow();
        setThisNodeMac();
        esp_startup_ping();
    }


    //Update the MODEL data
    void updateMODEL() { _MODEL = StoreMODEL(); }

    void storeMacAddress(uint8_t* MacAddress) {
        for (int i = 0; i < 6; i++) _auiMacAddress[i] = MacAddress[i];
    }


    // //Set/Get NodeID
    // void setNodeID(int iNodeID) { _iNodeID = iNodeID; }
    // int getNodeID(){return _iNodeID; }


    //Set the Wifi Credentials
    void setWifiNetworkCredentials(const char* ssid, const char* password) {
        _ssid = ssid;
        _password = password;
    }


    //Set the MQTT client data.
    void setMQTTClientData(const char* mqtt_server, int mqtt_port, const char* mqtt_client_id) {
        _mqtt_server = mqtt_server;
        _mqtt_port = mqtt_port;
        _mqtt_client_id = mqtt_client_id;
    }

    //MQTT callback function
    void MqttCallBack(char* topicC, byte* payload, unsigned int length) {
        static char buf[256];
        unsigned int n = (length < sizeof(buf)-1) ? length : sizeof(buf)-1;
        memcpy(buf, payload, n);
        buf[n] = '\0';
        Serial.printf("MQTT message on [%s]: %s\n", topicC, buf);
    }


    //Reconnect with the broker
    void brokerReconnect() {
        while (!client.connected()) {
            Serial.print("Attempting MQTT connection...");
            if (client.connect(_mqtt_client_id)) {
                Serial.println("connected");
                client.subscribe("home/security/commands");
            } else {
                Serial.printf("failed, rc=%d. retry in 5s\n", client.state());
                delay(5000);
            }
        }
    }


    //Get the MQTT client data. 
    PubSubClient& getClient() { return client; }


    //Get the RSSI (Received Signal Strength Indicator)
    float getRSSI() { return avgRSSI; }
    

    //Update the RSSI 
    void updateRSSI() {
        int rssi = WiFi.RSSI();
        rssiSamples[sampleIndex++] = rssi;
        if (sampleIndex >= NUM_SAMPLES) {
            sampleIndex = 0;
            bufferFilled = true;
        }
        int count = bufferFilled ? NUM_SAMPLES : sampleIndex;
        long sum = 0;
        for (int i = 0; i < count; i++) sum += rssiSamples[i];
        avgRSSI = (float)sum / count;
        //Serial.printf("Average RSSI: %.2f\n", avgRSSI);
    }


    //Initial RSSI value 
    void initialRSSI(){
        if (sampleIndex < NUM_SAMPLES){
            updateRSSI();
        }
    }


    // Get MAC address used by ESP-NOW (STA interface)
    void setThisNodeMac() {
        esp_err_t result = esp_wifi_get_mac(WIFI_IF_STA, strucTXMessage.src_addr);
        if (result == ESP_OK) {
            Serial.print("Node Mac Address Stored: ");
            for (int i = 0; i < 6; i++) {
                Serial.printf("%02X", strucTXMessage.src_addr[i]);
                if (i < 5) Serial.print(":");
            }
        }
    }

    
    //ESP send callback. 
    void onESPSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
        Serial.printf("TX: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", strucTXMessage.src_addr[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.printf(" | ID: %d | Value: %s\n", strucTXMessage.id, strucTXMessage.value);
    }


    //ESP recieve callback.
    void onReceive(const esp_now_recv_info_t* info, const uint8_t* incomingDataBytes, int len) {
        //Stores the data into RXMessage
        memcpy(&strucRXMessage, incomingDataBytes, sizeof(strucRXMessage));
        memcpy(strucRXMessage.src_addr, info->src_addr, 6);
        
        Serial.print("RX: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", strucRXMessage.src_addr[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.printf(" | ID: %d | Value: %s\n", strucRXMessage.id, strucRXMessage.value);

        //Converts the mac address and stores.
        storePeerAddress(macToShortInt(info->src_addr));

        //Store the received data into model. 
        // updateModelFromSnapshot(incomingDataBytes);
    }


    //Stores the Peers mac address
    void storePeerAddress(uint32_t macID){
        //check if address already exists. 
        // Serial.printf("Checking Peer address......");
        for (int i = 0; i<8; i++){
            if(uiPeerMacAddressStorage[i].uiMacAddress == macID) {
                //Serial.printf("Peer address already stored\n");
                return;
            } else { //store the address into the storage
                uiPeerMacAddressStorage[iPA].uiMacAddress = macID;
                uiPeerMacAddressStorage[iPA].id = iPA;
                iPA++;
                Serial.printf("Peer address: %d SAVED!\n", macID);
                return;
            }
        }
        
    }


    //Convert the mac address into a int value
    uint32_t macToShortInt(const uint8_t *mac) {
        uint32_t value = 0;
        for (int i = 0; i < 6; i++) {
            value = (value * 31) + mac[i];  // simple hash function
        }
        return value;
    }


    //ESP Startup ping - Used for estabishing contact with other nodes.
    void esp_startup_ping() {
        //Try establish connection for 5 seconds. 
        while (millis() - startTime < 5000) {
            //Calculate the average RSSI value.
            initialRSSI();
            //Get the current RSSI for the node. Save the data in the value entry of TX packet. 
            snprintf(strucTXMessage.value, sizeof(strucTXMessage.value), "Node RSSI: %.2f", getRSSI());
            sendSingleEspNowMsg(strucTXMessage.value);
            delay(1000);
        }
    }
    

    //Compares the RSSI of all the node and selects the lowest rssi for connection with the cloud. 
    void setCloudConnectionNode(){

    }


    //Update floor sensor data. 
    String updateSENSORS(){
        //get data from the sensors

        // return data from the sensors. 
        Serial.printf("Updating Sensor stored data\n");
        return ("TESTING");
    }


    //Send ESP NOW packet. 
    void compileFloorData(){
        updateSENSORS();
        String strFloor = buildFloorSnapshot(getNodeID());
        Serial.printf("Building floor snapshot\n");
    }


    //send ESP now message
    void sendSingleEspNowMsg(char* message){
        Serial.printf("Sending Single ESP now Message\n");
        memcpy(&strucTXMessage, message, sizeof(strucTXMessage));
        //Send the TX packet over ESP now.
        esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&strucTXMessage, sizeof(strucTXMessage));
        if (result != ESP_OK) Serial.printf("esp_now_send failed with error: %d\n", result);
        //Update TX Message ID
        strucTXMessage.id++;
    }


    //Break down the packet into ESP NOW size messages. 
    void sendFloorDataOverEspNow(){
        compileFloorData();

        //code to breakdown the string size into <250 chars
        //while message > 250 chars, 
        //store first 249 chars onto string,
        //add null terminator.
        //send over esp now.
        char *message = "HELLO";
        sendSingleEspNowMsg(message);
    }


    int getNodeID(){
        return _iFloorID;
    };



    //Tranmit window 
    void transmitWindow(int iNodeID){
        if(millis() - startTime > 125){
            uiTransmitPosition = (uiTransmitPosition<<1);
            if (uiTransmitPosition > 0b10000000) uiTransmitPosition = 0b00000001;
        }

        //RX ESP data

        switch (uiTransmitPosition){
            case 0b00000001: //cloud data subscribe

            case 0b00000010: //node_01
                if (iNodeID == uiTransmitPosition) sendFloorDataOverEspNow();

            case 0b00000100: //node_02
                if (iNodeID == uiTransmitPosition) sendFloorDataOverEspNow();

            case 0b00001000: //node_03
                if (iNodeID == uiTransmitPosition) sendFloorDataOverEspNow();

            case 0b00010000: //node_04
                if (iNodeID == uiTransmitPosition) sendFloorDataOverEspNow();

            case 0b00100000: //node_05
                if (iNodeID == uiTransmitPosition) sendFloorDataOverEspNow();

            case 0b01000000: //node_06
                if (iNodeID == uiTransmitPosition) sendFloorDataOverEspNow();

            case 0b10000000: //node_07
                if (iNodeID == uiTransmitPosition) sendFloorDataOverEspNow();

            // default: //do something

        }
    }
};

// Define static instance pointer
inline classFloorNode* classFloorNode::instance = nullptr;

#endif


