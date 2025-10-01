#include <WiFi.h>
#include <PubSubClient.h>

// WiFi credentials
const char* ssid = "Joe's S23 Ultra";
const char* password = "joea12345";

// MQTT broker details
const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 1883; 
const char* mqtt_client_id = "ESP32_BaseStation"; 

uint64_t macAddress;

int message_ID = 0;

WiFiClient espClient;
PubSubClient client(espClient);

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

  // gives the base MAC as uint64_t
  macAddress = ESP.getEfuseMac(); 
  Serial.print("ESP32 MAC Address");
  Serial.println(macAddress);
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

// PubSubClient MQTT callback
void MqttCallBack(char* topicC, byte* payload, unsigned int length) {
  // Make a safe null-terminated copy of the payload
  static char buf[256];
  unsigned int n = (length < sizeof(buf)-1) ? length : sizeof(buf)-1;
  memcpy(buf, payload, n);
  buf[n] = '\0';  // null terminator

  // Now safe to call parseMqtt
  parseMqtt(topicC, buf);
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


