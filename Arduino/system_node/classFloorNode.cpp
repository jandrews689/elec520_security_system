#include <classFloorNode.h>

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

//NETWORKING////////////////////////////////////////////////////////////////////////////////////////

//MQTT Get client data. 
PubSubClient& getClient() { return client; }


//WIFI Set the Wifi Credentials
void setWifiNetworkCredentials(const char* ssid, const char* password) {
    _ssid = ssid;
    _password = password;
}


//MQTT callback function
void MqttCallBack(char* topicC, byte* payload, unsigned int length) {
    static char buf[256];
    unsigned int n = (length < sizeof(buf)-1) ? length : sizeof(buf)-1;
    memcpy(buf, payload, n);
    buf[n] = '\0';

    //Parse the Mqtt data into MODEL. 
    parseSystemMqttString(String(buf));

    // Serial.printf("MQTT Callback [%s]: %s\n", topicC, buf);
}


//MQTT Set client data.
void setMQTTClientData(const char* mqtt_server, int mqtt_port, const char* mqtt_client_id) {
    _mqtt_server = mqtt_server;
    _mqtt_port = mqtt_port;
    _mqtt_client_id = mqtt_client_id;
}


//MQTT Reconnect with the broker
void brokerReconnect() {
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


//ESP NOW//////////////////////////////////////////////////////////////////////////////////////////
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


//ESP send callback. 
void onESPSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
    Serial.printf("TX: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", strucTXMessage.src_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.printf(" | Value: %s\n", strucTXMessage.payload);
}


//Esp now receive call back function. 
void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    // Create a null-terminated buffer
    static char buf[251];
    int n = (len < 250) ? len : 250;
    memcpy(buf, data, n);
    buf[n] = '\0';  // make it a valid string

    Serial.print("RX: ");
    for (int i = 0; i < 6; i++) {
        Serial.printf("%02X", info->src_addr[i]);
        if (i < 5) Serial.print(":");
    }
    Serial.printf(" | Value: %s\n", buf);
    String msg(buf);
    parseRoomEspString(msg);
}


//Send esp now message
void sendEspNowMsg(struct_message &message) {
    // Ensure null-termination
    message.payload[sizeof(message.payload) - 1] = '\0';
    size_t len = strnlen(message.payload, sizeof(message.payload));
    
    if (len == 0 || len > 250) {
        Serial.printf("Invalid ESP-NOW payload length: %u\n", (unsigned)len);
        return;
    }

    esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t*)message.payload, len);
    if (result != ESP_OK)
        Serial.printf("esp_now_send failed with error: %d\n", result);
}


//*********************************************************************************************** */
//PUBLIC////////////////////////////////////////////////////////////////////////////////////////////

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
    _lastFloorSentMsg = 0;
    _uiNumRoom = 0;
}

//Set/Get NodeID
// void setNodeID(int iNodeID) { _iNodeID = iNodeID; }
// int getNodeID(){return _iNodeID; }

void setFloorID(byte id){_bFloorID = id;}
byte getFloorID(){return _bFloorID;}

void setNumOfFloors(int value){
    iNumOfFloors = value;
}

//NETWORKING/////////////////////////////////////////////////////////////////////////////
//Network setup. 
void setupNetwork() {
    setup_wifi();
    setup_espnow();
}

//Sets the number of rooms in the system. Used for sending the correct amount of esp now messages per floor. 
void setNumberOfRooms(int number){
    _uiNumRoom = number;
}


//Send floor data over esp now
void sendFloorData(){
    for (uint8_t i=1; i<_uiNumRoom+1; i++){
        // Serial.printf("Room ID is %d\n", i);
        String data = buildRoomEspString(getFloorID(), i);
        //convert string to char[250];
        data.toCharArray(strucTXMessage.payload, sizeof(strucTXMessage.payload));
        strucTXMessage.payload[sizeof(strucTXMessage.payload) - 1] = '\0';
        //Send the data over esp now. 
        sendEspNowMsg(strucTXMessage);
        delay(20);
    }

}


//Transmit window 
void transmitWindow(){
    //Trasnmit time interval. 
    if(millis() - startTime > 1000){
        //Increment the transmit window position. 
        bTransmitPosition = (bTransmitPosition<<1);
        //Roll over. 
        if (bTransmitPosition == 0b0000'0000) bTransmitPosition = 0b0000'0001;
        startTime = millis();
        //If ID match, then send data.
        if (getFloorID() == bTransmitPosition) {
            sendFloorData();
        } 
    }
}


//MQTT loop
void mqttOperate(){
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
        for (int i=1; i<iNumOfFloors+1; i++){
            topic = cloudTopicFloor(i);
            // topic = "ELEC520/security";
            payload = buildFloorMqttString(i);
            client.publish(topic.c_str(), payload.c_str());
            Serial.printf("MQTT Publish [%s]: %s\n", topic.c_str(), payload.c_str());
            delay(20);
        }
    }

}
