#include <WiFi.h>
#include <PubSubClient.h>

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
}

// Callback when receiving subscribed messages
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

// Try reconnecting if disconnected
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id)) {
      Serial.println("connected");
      client.subscribe("ELEC520/security/#"); // subscribe to commands
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Example publish system status every 5 seconds
  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();
    
    client.publish("ELEC520/security", "System Armed");
    Serial.println("Status published: System Armed. Message Count:" + String(message_ID));
    message_ID ++;
  }
}