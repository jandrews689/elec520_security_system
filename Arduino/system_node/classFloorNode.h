#ifndef CLASS_FLOOR_NODE
#define CLASS_FLOOR_NODE

#include <cstdint>
#include <PubSubClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>
#include "json_formatting.h"

#define NUM_SAMPLES 10

class classFloorNode {
private:
    // Singleton-style instance pointer for static callbacks
    static classFloorNode* instance;

    int _iNodeID;
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

    // Message structure
    typedef struct struct_message {
        int id = 1;
        char value[250];
        uint8_t src_addr[6];
    } struct_message;

    struct_message strucTXMessage;
    struct_message strucRXMessage;

    unsigned long startTime;

    // --- Static callbacks that forward into the instance ---
    static void mqttCallbackStatic(char* topic, byte* payload, unsigned int length) {
        if (instance) instance->MqttCallBack(topic, payload, length);
    }

    static void onSentStatic(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
        if (instance) instance->onSent(mac_addr, status);
    }

    static void onReceiveStatic(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
        if (instance) instance->onReceive(info, data, len);
    }

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
        _iNodeID = -1;
        for (int i = 0; i < 6; i++) _auiMacAddress[i] = 0;
    }

    // --- Your original functions, preserved ---

    void setupNetwork() {
        setup_wifi();
        setup_espnow();
        esp_startup_ping();
    }

    void update() { _MODEL = StoreMODEL(); }

    void storeMacAddress(uint8_t* MacAddress) {
        for (int i = 0; i < 6; i++) _auiMacAddress[i] = MacAddress[i];
    }

    void setNodeID(int iNodeID) { _iNodeID = iNodeID; }

    void setWifiNetworkCredentials(const char* ssid, const char* password) {
        _ssid = ssid;
        _password = password;
    }

    void setMQTTClientData(const char* mqtt_server, int mqtt_port, const char* mqtt_client_id) {
        _mqtt_server = mqtt_server;
        _mqtt_port = mqtt_port;
        _mqtt_client_id = mqtt_client_id;
    }

    // --- MQTT ---

    void MqttCallBack(char* topicC, byte* payload, unsigned int length) {
        static char buf[256];
        unsigned int n = (length < sizeof(buf)-1) ? length : sizeof(buf)-1;
        memcpy(buf, payload, n);
        buf[n] = '\0';
        Serial.printf("MQTT message on [%s]: %s\n", topicC, buf);
    }



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

    PubSubClient& getClient() { return client; }

    // --- RSSI ---

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
        Serial.printf("Average RSSI: %.2f\n", avgRSSI);
    }

    float getRSSI() { return avgRSSI; }

    // --- ESP-NOW ---

    void onSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
        Serial.print("Send Status: ");
        Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
    }

    void onReceive(const esp_now_recv_info_t* info, const uint8_t* incomingDataBytes, int len) {
        memcpy(&strucRXMessage, incomingDataBytes, sizeof(strucRXMessage));
        memcpy(strucRXMessage.src_addr, info->src_addr, 6);

        Serial.print("Received from: ");
        for (int i = 0; i < 6; i++) {
            Serial.printf("%02X", strucRXMessage.src_addr[i]);
            if (i < 5) Serial.print(":");
        }
        Serial.printf(" | ID: %d | Value: %s\n", strucRXMessage.id, strucRXMessage.value);
    }

    esp_now_peer_info_t peerInfo = {};



    void esp_startup_ping() {
        if (millis() - startTime < 5000) {
            strucTXMessage.id++;
            snprintf(strucTXMessage.value, sizeof(strucTXMessage.value), "Node RSSI: %.2f", getRSSI());
            esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t*)&strucTXMessage, sizeof(strucTXMessage));
            if (result == ESP_OK) Serial.println("Queued send OK");
            else Serial.printf("esp_now_send failed with error: %d\n", result);
            delay(1000);
        }
    }
};

// Define static instance pointer
inline classFloorNode* classFloorNode::instance = nullptr;

#endif
