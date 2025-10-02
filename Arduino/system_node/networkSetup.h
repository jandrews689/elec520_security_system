#include <PubSubClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WiFi.h>

// WiFi credentials
const char* ssid = "Joe's S23 Ultra";
const char* password = "joea12345";

// MQTT broker details
const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 1883; 
const char* mqtt_client_id = "ESP32_BaseStation"; 

int message_ID = 0;

WiFiClient espClient;
PubSubClient client(espClient);


const int NUM_SAMPLES = 10;   // how many samples to average
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


// PubSubClient MQTT callback
void MqttCallBack(char* topicC, byte* payload, unsigned int length) {
  // Make a safe null-terminated copy of the payload
  static char buf[256];
  unsigned int n = (length < sizeof(buf)-1) ? length : sizeof(buf)-1;
  memcpy(buf, payload, n);
  buf[n] = '\0';  // null terminator

  // Now safe to call parseMqtt
  // parseMqtt(topicC, buf);
}

// Connect to WiFi
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, mqtt_port);

  // client.subscribe(topicEverything().c_str()); //Subscribe to full system from broker. 
  client.setCallback(MqttCallBack); //MQTT callback for subscribed topics. 
}

// Example of Callback when receiving subscribed messages
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(" => ");
  String msg;
  for (int i = 0; i < length; i++) {
    msg += (char)message[i];
  }
  Serial.println(msg);

  // Example: if "DISARM" received, act on it
  if (msg == "DISARM") {
    Serial.println("System Disarmed!");
    // Add buzzer/LED logic here
  }
}



// Try reconnecting to the broker if disconnected
void brokerReconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      client.subscribe("home/security/commands"); // subscribe to commands
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// //Returns the numerical value of the mac address. 
// uint64_t getNumericMAC() {
//   uint64_t mac = ESP.getEfuseMac(); // gives the base MAC as uint64_t
//   return mac;
// }


//Take average of the wifi signal strength. 
void updateRSSI() {
  int rssi = WiFi.RSSI();
  rssiSamples[sampleIndex] = rssi;
  sampleIndex++;

  if (sampleIndex >= NUM_SAMPLES) {
    sampleIndex = 0;
    bufferFilled = true;
  }

  // compute average if enough samples
  int count = bufferFilled ? NUM_SAMPLES : sampleIndex;
  long sum = 0;
  for (int i = 0; i < count; i++) {
    sum += rssiSamples[i];
  }
  avgRSSI = (float)sum / count;
  Serial.print("Average RSSI: ");
  Serial.println(avgRSSI);
}


float getRSSI(){
  return avgRSSI;
}


// Send callback
void onSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}


// Receive callback
void onReceive(const esp_now_recv_info_t *info, const uint8_t *incomingDataBytes, int len) {
  // Copy the incoming bytes into your struct
  memcpy(&strucRXMessage, incomingDataBytes, sizeof(strucRXMessage));

  // Save the sender MAC into your struct field
  memcpy(strucRXMessage.src_addr, info->src_addr, 6);

  // Print sender MAC
  Serial.print("Received from: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", strucRXMessage.src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  // Print struct data
  Serial.print(" | ID: ");
  Serial.print(strucRXMessage.id);
  Serial.print(" | Value: ");
  Serial.println(strucRXMessage.value);
}


// Broadcast peer (so no need to hard-code MACs)
esp_now_peer_info_t peerInfo = {};


bool setup_espnow() {
  startTime = millis(); //time started

  // Ensure STA mode (but don't reconnect Wi-Fi, already done in setup_wifi)
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }
  Serial.println("Initialization ESP-NOW Successful");

  // Register callbacks
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onReceive);

  // Broadcast peer — must use the same channel as current Wi-Fi
  memset(peerInfo.peer_addr, 0xFF, 6); 
  peerInfo.channel = WiFi.channel();   // ✅ match Wi-Fi channel, not hardcoded
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return false;
  }
  Serial.println("Peer 0xFF successfully Added. Transmitting to all Nodes!");
  return true;
}


//Sends a simple message over ESP Now 5 times to make sure a message has sent via esp now. 
//Pings the nodes RSSI wifi signal 5 times. 
void esp_startup_ping() {

  if (millis() - startTime < 5000){
    strucTXMessage.id = strucTXMessage.id+1;
    snprintf(strucTXMessage.value, sizeof(strucTXMessage.value), "Node RSSI: %.2f", getRSSI());
    // Broadcast to all peers
    esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *) &strucTXMessage, sizeof(strucTXMessage));
    if (result == ESP_OK) {
      Serial.println("Queued send OK");
    } else {
      Serial.printf("esp_now_send failed with error: %d\n", result);
    }

    delay(1000);
  }
}

