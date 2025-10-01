#include <esp_now.h>
#include <WiFi.h>
#include <esp_wifi.h>

// Message structure
typedef struct struct_message {
  int id = 1;
  float value;
} struct_message;

struct_message myData;
struct_message incomingData;

// Send callback
void onSent(const wifi_tx_info_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// Receive callback
void onReceive(const esp_now_recv_info_t *info, const uint8_t *incomingDataBytes, int len) {
  // Copy the incoming bytes into your struct
  memcpy(&incomingData, incomingDataBytes, sizeof(incomingData));

  // Print sender MAC
  Serial.print("Received from: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }

  // Print struct data
  Serial.print(" | ID: ");
  Serial.print(incomingData.id);
  Serial.print(" | Value: ");
  Serial.println(incomingData.value);
}


// Broadcast peer (so no need to hard-code MACs)
esp_now_peer_info_t peerInfo = {};


void setup() {
  Serial.begin(115200);

  // Set device in STA mode
  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    Serial.println("Initialization ESP-NOW Successful");
  }

  // Register callbacks
  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onReceive);

  memset(peerInfo.peer_addr, 0xFF, 6); // broadcast MAC
  peerInfo.channel = 1;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  } else {
    Serial.println("Peer added successfully");
  }
}

void loop() {
  // Prepare message
  myData.id = myData.id+1;
  myData.value = random(0, 100) / 1.0;

  // Broadcast to all peers
  esp_err_t result = esp_now_send(peerInfo.peer_addr, (uint8_t *) &myData, sizeof(myData));
  if (result == ESP_OK) {
    Serial.println("Queued send OK");
  } else {
    Serial.printf("esp_now_send failed with error: %d\n", result);
  }

  delay(2000);
}
