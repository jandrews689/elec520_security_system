// -------- esp32_floor/include/classFloorController.h --------
#pragma once
#ifndef CLASSFLOORCONTROLLER_H
#define CLASSFLOORCONTROLLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <elec520_protocol.h> // protocol API (builders/parsers/model)
#include "config.h"
#include "i2c_roombus.h"

// FloorController = single ESP32 controlling one floor.
// Responsibilities:
// • Elect a leader via ESP‑NOW gossip (lowest MAC wins).
// • As leader: bring up Wi‑Fi + MQTT and publish summaries using protocol builders.
// • As follower: keep Wi‑Fi off, just gossip and parse room payloads.
// • Always: poll the I²C room bus and feed any compact room strings into the protocol.
class FloorController
{
public:
    void begin(); // init ESP‑NOW, election, MQTT client, I²C bus
    void loop();  // run non‑blocking ticks (call from Arduino loop)

private:
    // ===== Leader election state =====
    uint8_t mac_[6]{};            // device MAC (STA)
    uint32_t selfRank_ = 0;       // rank = last 3 MAC bytes as uint24
    uint32_t lowestRank_ = 0;     // lowest rank seen recently (windowed)
    uint8_t lowestMac_[6] = {};   // lowest MAC seen recently (lexicographic)
    uint32_t lastPeerSeenMs_ = 0; // last time any peer gossip was received
    bool isLeader_ = true;        // assume leader until proven otherwise
    // Leader switch hysteresis to avoid flip/flop on transient packet loss
    bool leaderPending_ = false;
    bool pendingLeaderState_ = false;
    uint32_t leaderPendingSinceMs_ = 0;

    // ===== ESP‑NOW pack =====
    uint32_t hb_ = 0; // gossip heartbeat counter
    struct GossipMsg
    {
        uint8_t proto;     // protocol version (1)
        char site[8];      // site namespace (e.g., "ELEC520")
        uint8_t floor_id;  // ordinal floor index (0..7)
        uint32_t rank;     // election rank (lower is better)
        uint8_t mac[6];    // full sender MAC (used for robust election ordering)
        uint8_t is_leader; // sender's current view (hint only)
        uint32_t hb;      // sender heartbeat
    } __attribute__((packed));

    static FloorController *self_;
    static void onEspNowRx_(const uint8_t *mac, const uint8_t *data, int len);

    // ===== Wi‑Fi / MQTT =====
    WiFiClient tcp_;
    WiFiClientSecure tls_;
    PubSubClient mqtt_{USE_TLS ? (Client &)tls_ : (Client &)tcp_};
    uint32_t nextConnTryMs_ = 0; // next time to attempt connect
    uint32_t backoffMs_ = 500;   // exponential backoff (max ~10 s)

    // ===== Cadence =====
    uint32_t lastGossipMs_ = 0;
    uint32_t lastSummaryMs_ = 0;
    uint32_t lastI2CPollMs_ = 0;
    uint32_t lastSimMs_ = 0; // last time simulation generated a line
    uint32_t lastRoomTxMs_ = 0; // throttle periodic room rebroadcast

    // ===== Room I²C bus =====
    RoomBusI2C roombus_;
    String lastLocalRoomLine_[MAX_ROOMS]; // cache latest lines from local Nanos
    uint8_t rrRoom_ = 0;                  // round‑robin index for periodic resend

    // ===== Gossip de‑dup (prevents re‑broadcast storms) =====
    struct Recent
    {
        uint32_t h;
        uint32_t ts;
    } recent_[RECENT_CACHE_SIZE]{};
    uint8_t recentHead_ = 0;
    static uint32_t fnv1a_(const uint8_t *buf, int len);
    bool seenRecently_(uint32_t h, uint32_t nowMs);
    void remember_(uint32_t h, uint32_t nowMs);

    // ===== Helpers =====
    void initEspNow_();                                   // bring up ESP‑NOW broadcast (opt. encrypted)
    void sendGossip_();                                   // emit compact election packet
    void ensureLeaderNet_();                              // if leader: ensure Wi‑Fi+MQTT; if follower: disable
    void maybePublishSummary_();                          // leader publishes floor summaries via protocol
    void evaluateLeader_();                               // update isLeader_ from observed ranks
    void handleEspPayload_(const uint8_t *data, int len); // parse application payload
    void pollI2CRooms_();                                 // read compact room strings from Nanos (if any)
    void sendRoomLine_(const String &line);               // broadcast a compact room string over ESP‑NOW
};

#endif // ELEC520_CLASSFLOORCONTROLLER_H