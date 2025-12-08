#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <elec520_protocol.h>
#include "config.h"
#include "i2c_roombus.h"

class FloorController {
public:
  void begin();
  void loop();

private:
  // ---- IDs / election ----
  uint8_t  mac_[6]{};
  uint32_t selfRank_=0, lowestRank_=0;
  uint8_t  lowestMac_[6]{};
  uint32_t lastPeerSeenMs_=0;
  bool     isLeader_=true;
  bool     leaderPending_=false, pendingLeaderState_=false;
  uint32_t leaderPendingSinceMs_=0;

  // ---- ESP-NOW ----
  struct GossipMsg {
    uint8_t  proto;     // 1
    char     site[8];   // zero-terminated
    uint8_t  floor_id;
    uint32_t rank;
    uint8_t  mac[6];
    uint8_t  is_leader;
    uint32_t hb;
  } __attribute__((packed));
  volatile bool espnowInit_=false;
  uint32_t hb_=0;
  static FloorController* self_;
  static void onRx_(const uint8_t* mac, const uint8_t* data, int len);

  // ---- Wi-Fi/MQTT ----
  WiFiClient tcp_;
  WiFiClientSecure tls_;
  PubSubClient mqtt_{USE_TLS ? (Client&)tls_ : (Client&)tcp_};
  uint32_t mqttBackoffMs_=500, lastMqttTryMs_=0, mqttStabilizeMs_=0;

  // ---- Cadence ----
  uint32_t lastGossipMs_=0, lastI2CPollMs_=0, lastRoomTxMs_=0, lastSummaryMs_=0;
  // last time we published alarm trigger MQTT messages (rate limiting)
  uint32_t lastAlarmPublishMs_ = 0;
  // last time alarm state changed to ALARM (for auto-clear)
  uint32_t lastAlarmStateChangeMs_ = 0;
  // previous alarm state to detect transitions
  uint8_t prevAlarmState_ = SystemState::DISARMED;
  // track the last MQTT/cloud-sourced system state (ARMED or DISARMED)
  // when ESP clears an ALARM, it reverts to this state
  uint8_t lastMqttSystemState_ = SystemState::DISARMED;

  // ---- I2C rooms ----
  RoomBusI2C rb_;
  String lastLocal_[MAX_ROOMS];
  uint8_t rr_=0;

  // ---- dedupe ----
  struct Recent { uint32_t h, ts; } recent_[RECENT_CACHE_SIZE]{};
  uint8_t rHead_=0;
  static uint32_t fnv1a(const uint8_t* d, int n);
  bool seen(uint32_t h, uint32_t now);
  void remember(uint32_t h, uint32_t now);
  // ---- gossip/send metrics ----
  uint32_t sendAttemptCount_ = 0;
  uint32_t sendSuccessCount_ = 0;
  uint32_t sendFailCount_ = 0;
  uint8_t  sendConsecFails_ = 0;
  uint32_t lastSendFailMs_ = 0;
  uint32_t lastEspNowRecoveryMs_ = 0;
  uint32_t lastPeerEnsureMs_ = 0;

  // limited peer table to keep track of the last seen gossip peers for
  // reliable deterministic leader election. We'll prune peers older than
  // LEADER_PEER_TIMEOUT_MS when computing the election.
  struct Peer { uint8_t mac[6]; uint32_t rank; uint32_t lastSeenMs; uint8_t floor_id; bool valid; };
  Peer peers_[MAX_FLOORS]{};
  void updatePeer(const uint8_t* mac, uint32_t rank, uint8_t floor, bool is_leader);

  // ---- helpers ----
  void initEspNow();
  void ensureBroadcastPeer();
  void sendGossip();
  void evalLeader();
  void dumpEspNowStatus();
  void tickLeaderNet();
  void tickPublish();
  void tickRoomI2C();
  void sendRoomLine(const String& line, bool force=false);
  void sendSystemLine(const String& line, bool force=false);
  static void onRxMQTT_(char* topicC, byte* payload, unsigned int length);
  // Alarm / system helpers
  void alarmSystemStateMachine();
  void triggerAlarm();
  void clearAlarm();
};
