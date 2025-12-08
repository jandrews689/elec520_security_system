#include "classFloorController.h"
#include "config.h"
#include "i2c_roombus.h"

// Threshold for ultrasound trigger (matches reference system)
#ifndef ULTRA_THRESHOLDS
#define ULTRA_THRESHOLDS 20
#endif

// Normalize various compact room line formats into the semicolon-delimited
// canonical form expected by parseRoomEspString(). Examples we accept:
//  - "f/1/r/1;u/1:5" (already canonical)
//  - "f/1/r/1/u/1:5" (single token; convert to "f/1/r/1;u/1:5")
//  - "f/1 r/1 u/1:5" (space separated; convert to semicolons)
static String normalizeRoomLine(const String &input)
{
  String s = input;
  if (s.indexOf(';') >= 0)
    return s; // already normalized
  s.replace(" ", ";");
  int pos = 0;
  while ((pos = s.indexOf("/u/", pos)) >= 0)
  {
    if (pos == 0 || s.charAt(pos - 1) != ';')
    {
      s = s.substring(0, pos) + ";" + s.substring(pos + 1);
      pos += 3;
    }
    else
    {
      pos += 3;
    }
  }
  pos = 0;
  while ((pos = s.indexOf("/h/", pos)) >= 0)
  {
    if (pos == 0 || s.charAt(pos - 1) != ';')
    {
      s = s.substring(0, pos) + ";" + s.substring(pos + 1);
      pos += 3;
    }
    else
    {
      pos += 3;
    }
  }
  return s;
}

// ---------- Utils ----------
static inline int cmpMac(const uint8_t *a, const uint8_t *b)
{
  for (int i = 0; i < 6; ++i)
  {
    if (a[i] < b[i])
      return -1;
    if (a[i] > b[i])
      return 1;
  }
  return 0;
}
// Convert a hex nibble char to a value 0..15, returns -1 on error
// encryption is intentionally disabled: no parsing or keys used
static inline int extractFloor(const String &s)
{
  // expect "f/<n>" at start or after a space
  int p = s.indexOf("f/");
  if (p < 0)
    return -1;
  p += 2;
  int q = p;
  while (q < (int)s.length() && isdigit((unsigned char)s[q]))
    q++;
  if (q == p)
    return -1;
  long v = s.substring(p, q).toInt();
  return (v >= 0 && v < MAX_FLOORS) ? (int)v : -1;
}

FloorController *FloorController::self_ = nullptr;

// ---------- Begin ----------
void FloorController::begin()
{
  Serial.flush();
  self_ = this;
  LOGT("Initializing FloorController");

  esp_read_mac(mac_, ESP_MAC_WIFI_STA);
  selfRank_ = ((uint32_t)mac_[3] << 16) | ((uint32_t)mac_[4] << 8) | mac_[5];
  lowestRank_ = selfRank_;
  memcpy(lowestMac_, mac_, 6);

  // Update protocol MODEL with our MAC and a baseline floor count
  char bootMacBuf[18];
  snprintf(bootMacBuf, sizeof(bootMacBuf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
  setMac(String(bootMacBuf));

  LOGI("Boot Floor %u site=%s MAC=%02X:%02X:%02X:%02X:%02X:%02X rank=0x%06X",
       (unsigned)FLOOR_ID, SITE_ID,
       mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5], (unsigned)selfRank_);

  // ESP-NOW + Wi-Fi driver up
  LOGT("Setting WiFi mode to STA");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();      // keep driver on
  WiFi.persistent(false); // avoid stale creds in NVS
  WiFi.setSleep(false);   // keep radio awake during connect
  WiFi.setAutoReconnect(true);
  initEspNow();

// I2C
#if USE_I2C_ROOMBUS
  LOGT("Initializing I2C room bus");
  rb_.begin(); // this guards SIMULATE_I2C internally
#if !SIMULATE_I2C
  LOGI("I2C @%lu Hz on SDA=%d SCL=%d", (unsigned long)I2C_CLOCK_HZ, I2C_SDA_PIN, I2C_SCL_PIN);
#else
  LOGI("I2C SIM enabled (no Wire.begin)");
#endif
#endif

  // MQTT
  LOGT("Configuring MQTT client");
  mqtt_.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_.setKeepAlive(60);
  mqtt_.setSocketTimeout(30);
  mqtt_.setBufferSize(1024);
  if (USE_TLS)
  {
    tls_.setInsecure();
  }

  // Alarm output pin (active when MODEL.systemState == ALARM)
  pinMode(23, OUTPUT);
  digitalWrite(23, LOW);

  LOGI("Begin complete");
}

// ---------- Loop ----------
void FloorController::loop()
{
  const uint32_t now = millis();

  // Keep ESP-NOW alive even if Wi-Fi state flaps
  static uint32_t lastEspNowCheck = 0;
  if (now - lastEspNowCheck > 2000)
  {
    lastEspNowCheck = now;
    if (!espnowInit_)
    {
      LOGE("ESP-NOW not init (periodic) -> reinit");
      initEspNow();
    }
  }
  // Periodically ensure broadcast peer exists for robust gossip
  if (now - lastPeerEnsureMs_ > 5000)
  {
    lastPeerEnsureMs_ = now;
    LOGT("Periodic ensureBroadcastPeer check");
    ensureBroadcastPeer();
  }
  // Periodic diagnostics dump every 15s
  static uint32_t lastDiagMs = 0;
  if (now - lastDiagMs > 15000)
  {
    lastDiagMs = now;
    dumpEspNowStatus();
  }

  // Status heartbeat
  static uint32_t lastHB = 0;
  if (now - lastHB > 5000)
  {
    lastHB = now;
    LOGI("Status: %s Floor=%u", isLeader_ ? "LEADER" : "FOLLOWER", (unsigned)FLOOR_ID);
  }

  // Election & gossip
  if (now - lastGossipMs_ > (GOSSIP_BASE_MS + (selfRank_ % GOSSIP_JITTER_MS)))
  {
    lastGossipMs_ = now;
    LOGT("Sending gossip");
    sendGossip();
  }
  evalLeader();

  // Followers: keep radio in STA so ESP-NOW stays alive; do NOT associate to AP
  if (!isLeader_)
  {
    if (WiFi.getMode() != WIFI_STA)
    {
      WiFi.mode(WIFI_STA); // keep driver up for ESP-NOW
    }
    if (WiFi.isConnected())
    {
      // Just disconnect from AP, don't remove WiFi config or shut off driver
      WiFi.disconnect(); // ensures we’re not on the AP but keeps driver/ESP-NOW up
    }
    static bool logged = false;
    if (!logged)
    {
      LOGI("Follower   WiFi STA (disconnected)");
      logged = true;
    }
    // Ensure ESP-NOW remains initialized and has broadcast peer even as follower
    if (!espnowInit_)
    {
      LOGV("Follower: espnowInit_=false; initEspNow()");
      initEspNow();
    }
    ensureBroadcastPeer(); // make sure the broadcast peer is present
    // no WIFI_OFF here; leave ESP-NOW running
  }

// I2C polling & re-gossip
#if USE_I2C_ROOMBUS
  tickRoomI2C();
  if (now - lastRoomTxMs_ > GOSSIP_ROOM_MS)
  {
    lastRoomTxMs_ = now;
    if (lastLocal_[rr_].length())
    {
      LOGT("Gossiping room data from room %u", (unsigned)rr_);
      sendRoomLine(lastLocal_[rr_], true);
    }
    rr_ = (rr_ + 1) % MAX_ROOMS;
  }
#endif

  // Leader network & publish
  if (isLeader_)
  {
    tickLeaderNet();
    // Run alarm state machine before publishing so triggers are emitted promptly
    alarmSystemStateMachine();
    tickPublish();
    if (mqtt_.connected())
      mqtt_.loop();
  }

  triggerAlarm();
}

// ---------- ESP-NOW ----------
void FloorController::initEspNow()
{
  if (espnowInit_)
    return; // already up
  if (WiFi.getMode() != WIFI_STA)
    WiFi.mode(WIFI_STA);
  esp_err_t rc = esp_now_init();
  if (rc != ESP_OK)
  {
    LOGE("ESP-NOW init failed rc=%d, attempting recover", (int)rc);
    // Try a single deinit+init recovery
    esp_err_t rc2 = esp_now_deinit();
    LOGT("ESP-NOW deinit rc=%d", (int)rc2);
    delay(10);
    rc = esp_now_init();
    if (rc != ESP_OK)
    {
      LOGE("ESP-NOW init retry failed rc=%d", (int)rc);
      espnowInit_ = false;
      return;
    }
  }
  espnowInit_ = true;

  esp_now_register_recv_cb([](const uint8_t *mac, const uint8_t *data, int len)
                           { onRx_(mac, data, len); });
  // Register a send callback to capture error codes when sends fail
  esp_now_register_send_cb([](const uint8_t *mac_addr, esp_now_send_status_t status)
                           {
                             if (status != ESP_NOW_SEND_SUCCESS) {
                               LOGE("ESP-NOW send cb reported failure status=%d; scheduling reinit", (int)status);
                               if (FloorController::self_) FloorController::self_->espnowInit_ = false;
                             } else {
                               LOGT("ESP-NOW send cb status=OK");
                             } });

  ensureBroadcastPeer();
  // Encryption is intentionally disabled; we do not set PMK or LTK at runtime
  LOGI("ESP-NOW ready");
}

void FloorController::ensureBroadcastPeer()
{
  if (!espnowInit_)
  {
    LOGE("ensureBroadcastPeer: ESP-NOW not init -> reinit");
    initEspNow();
    if (!espnowInit_)
      return;
  }
  static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  esp_now_peer_info_t p{};
  memcpy(p.peer_addr, BCAST, 6);
  // set to current WiFi channel (0 == any, but some HW/SDK combos require explicit channel)
  int ch = WiFi.channel();
  p.channel = (uint8_t)(ch < 0 ? 0 : ch);
  LOGT("ensureBroadcastPeer: using channel=%d (WiFi.channel=%d)", (int)p.channel, ch);
  // Broadcast/multicast addresses cannot be encrypted on many SDK/HCW combos.
  // Always keep broadcast peer plaintext (plain) and disable encryption.
  // We intentionally do not support ESP-NOW encryption in this firmware.
  p.encrypt = false;
  p.ifidx = WIFI_IF_STA;
  esp_err_t rc_del = esp_now_del_peer(BCAST);
  LOGT("ensureBroadcastPeer: esp_now_del_peer rc=%d", (int)rc_del);
  esp_err_t rc = esp_now_add_peer(&p);
  if (rc == ESP_OK || rc == ESP_ERR_ESPNOW_EXIST)
  {
    LOGT("Broadcast peer ensured rc=%d", (int)rc);
    // Verify peer info
    esp_now_peer_info_t got{};
    esp_err_t rcg = esp_now_get_peer(BCAST, &got);
    if (rcg == ESP_OK)
    {
      LOGT("Broadcast peer info: encrypt=%d channel=%d", (int)got.encrypt, (int)got.channel);
    }
    else
    {
      LOGV("Could not get Broadcast peer info rc=%d", (int)rcg);
    }
  }
  else
  {
    LOGE("Add ESP-NOW peer failed rc=%d", (int)rc);
  }
}

void FloorController::onRxMQTT_(char *topicC, byte *payload, unsigned int length)
{
  static char buf[256];
  unsigned int n = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';

  if (bool messageRX = parseCloud(topicC, buf))
  {
    LOGV("MQTT Callback [%s]: %s", topicC, buf);

    // New priority rules:
    // - Cloud (MQTT) controls ARMED/DISARMED and can always DISARM.
    // - ESP/leader can raise ALARM when cloud state is ARMED.
    // - Once ALARM, it remains until cloud sends DISARM.
    if (self_)
    {
      // Only handle state-specific logic if this is actually a system state message
      // Check if topic contains "s/st" (system state), not other fields like keypad, network, mac
      String topicStr(topicC);
      if (topicStr.indexOf("s/st") >= 0)
      {
        uint8_t incoming = MODEL.systemState;
        self_->lastMqttSystemState_ = incoming;
        LOGV("MQTT system state update recorded: %u", (unsigned)incoming);

        if (incoming == (uint8_t)SystemState::DISARMED)
        {
          // Cloud requests DISARM: immediately clear any ALARM and honor cloud
          LOGI("MQTT DISARM received: clearing ALARM and honoring cloud state");
          setSystemState((uint8_t)SystemState::DISARMED);
          self_->clearAlarm();
          // Also clear the trigger location so we don't auto-retrigger
          setTriggerLoc(0xFF, 0xFF, 0xFF, 0xFF);
        }
        else if (incoming == (uint8_t)SystemState::ARMED)
        {
          LOGV("MQTT ARMED received: recorded as lastMqttSystemState");
        }
        // If incoming is ALARM (should be rare), just record it
      }
    }
  }
}

void FloorController::onRx_(const uint8_t *mac, const uint8_t *data, int len)
{
  if (!self_)
    return;

  if (len == (int)sizeof(GossipMsg))
  {
    const GossipMsg *g = (const GossipMsg *)data;
    LOGV("Rx gossip: floor=%u rank=0x%06X leader=%u hb=%u", (unsigned)g->floor_id, (unsigned)g->rank, (unsigned)g->is_leader, (unsigned)g->hb);

    // Validate site (bounded compare against zero-terminated sender field)
    if (strncmp(g->site, SITE_ID, sizeof(g->site) - 1) != 0)
      return;

    self_->lastPeerSeenMs_ = millis();

    // Update our peer table with the details from this gossip message
    self_->updatePeer(g->mac, g->rank, g->floor_id, g->is_leader != 0);

    // Use payload's MAC/rank for election windowing (deterministic)
    // Maintain a legacy snapshot of the lowest mac/rank for logs; the peer
    // table will be used for the actual election logic.
    if (cmpMac(g->mac, self_->lowestMac_) < 0 ||
        (cmpMac(g->mac, self_->lowestMac_) == 0 && g->rank < self_->lowestRank_))
    {
      memcpy(self_->lowestMac_, g->mac, 6);
      self_->lowestRank_ = g->rank;
      LOGV("Updated lowest peer (legacy): MAC=%02X:%02X:%02X:%02X:%02X:%02X rank=0x%06X floor=%u leader=%u",
           g->mac[0], g->mac[1], g->mac[2], g->mac[3], g->mac[4], g->mac[5],
           (unsigned)g->rank, (unsigned)g->floor_id, (unsigned)g->is_leader);
    }
    return;
  }

  // Application payload (room or system-level compact strings)
  uint32_t h = fnv1a(data, len), now = millis();
  // mark we've seen a peer activity; this keeps the leader election
  // clock moving even if a peer isn't sending explicit GossipMsg messages.
  self_->lastPeerSeenMs_ = millis();
  // Derive rank/floor for the sender from its MAC and the line to update peer table
  uint32_t senderRank = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
  if (self_->seen(h, now))
    return;
  self_->remember(h, now);
  String line((const char *)data, len);
  int senderFloor = extractFloor(line);
  self_->updatePeer(mac, senderRank, (senderFloor < 0 ? 0 : (uint8_t)senderFloor), false);
  LOGT("Rx app payload (raw): %s", line.c_str());
  String normLine = normalizeRoomLine(line);
  if (normLine != line)
    LOGV("Rx normalized: %s -> %s", line.c_str(), normLine.c_str());
  LOGT("Rx payload (norm): %s", normLine.c_str());

  // Try room parser first (most common). If it fails, fall back to system parser.
  bool ok = false;
  bool isSystemMessage = (normLine.indexOf("f/") < 0); // system messages have no floor token

  if (!isSystemMessage)
  {
    ok = parseRoomEspString(normLine.c_str());
    if (ok)
    {
      LOGT("parseRoomEspString OK for: %s", normLine.c_str());
      // Extract floor and room IDs to mark them as connected via gossip
      int fpos = normLine.indexOf("f/");
      int rpos = normLine.indexOf("/r/");
      if (fpos >= 0 && rpos >= 0)
      {
        String fstr = normLine.substring(fpos + 2, rpos);
        int rend = normLine.indexOf('/', rpos + 3);
        if (rend < 0)
          rend = normLine.indexOf(':', rpos + 3);
        if (rend < 0)
          rend = normLine.indexOf(';', rpos + 3);
        if (rend < 0)
          rend = normLine.length();
        String rstr = normLine.substring(rpos + 3, rend);
        uint8_t f = (uint8_t)fstr.toInt();
        uint8_t r = (uint8_t)rstr.toInt();
        if (f < SMP_MAX_FLOORS && r < SMP_MAX_ROOMS)
        {
          // Mark both room and floor as connected
          MODEL.floors[f].connected = true;
          MODEL.floors[f].rooms[r].connected = true;
          LOGT("Marked f/%u/r/%u as connected via ESP-NOW gossip", (unsigned)f, (unsigned)r);
        }
      }
      // Re-gossip room messages so they propagate across the mesh
      self_->sendRoomLine(normLine, false);
    }
    else
    {
      LOGV("parseRoomEspString failed, trying parseSystemMqttString for: %s", normLine.c_str());
      ok = parseSystemMqttString(normLine);
      if (ok)
        LOGT("parseSystemMqttString OK for: %s", normLine.c_str());
      else
        LOGV("Both parsers failed for: %s", normLine.c_str());
    }
  }
  else
  {
    // No floor token -> system-level compact string (from leader gossip)
    // Followers: accept and parse, but do NOT re-gossip back
    ok = parseSystemMqttString(normLine);
    if (ok)
    {
      LOGT("parseSystemMqttString OK (leader gossip): %s", normLine.c_str());
      // System messages are leader-only gossip: followers parse but don't re-transmit
      return;
    }
    else
    {
      // Fallback to room parser in case formatting differs
      ok = parseRoomEspString(normLine.c_str());
      if (ok)
      {
        LOGT("Fallback parseRoomEspString OK for: %s", normLine.c_str());
        // Re-gossip if fallback parse succeeded
        self_->sendRoomLine(normLine, false);
      }
      else
        LOGV("Both parsers failed for: %s", normLine.c_str());
    }
  }
}

void FloorController::sendGossip()
{
  if (!espnowInit_)
  {
    LOGV("sendGossip: espnowInit=false, lastEspNowRecoveryMs=%u", (unsigned)lastEspNowRecoveryMs_);
    return;
  }
  GossipMsg g{};
  g.proto = 1;
  memset(g.site, 0, sizeof(g.site));
  strncpy(g.site, SITE_ID, sizeof(g.site) - 1);
  g.floor_id = FLOOR_ID;
  g.rank = selfRank_;
  memcpy(g.mac, mac_, 6);
  g.is_leader = isLeader_;
  g.hb = hb_++;
  static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (!espnowInit_)
  {
    initEspNow();
    if (!espnowInit_)
      return;
  }
  // Ensure broadcast peer is present before sending
  ensureBroadcastPeer();
  LOGV("Gossip send: floor=%u rank=0x%06X leader=%u hb=%u", (unsigned)g.floor_id, (unsigned)g.rank, (unsigned)g.is_leader, (unsigned)g.hb);
  ++sendAttemptCount_;
  esp_err_t rc = esp_now_send(BCAST, (uint8_t *)&g, sizeof(g));
  static uint32_t nextEspNowReinitMs = 0;
  if (rc != ESP_OK)
  {
    LOGE("Gossip send rc=%d -> reinit ESP-NOW", (int)rc);
    bool peerExists = esp_now_is_peer_exist(BCAST);
    LOGE("Gossip send fail diagnostic: espnowInit=%d WiFi.mode=%d WiFi.status=%d WiFi.connected=%d channel=%d peerExists=%d",
         (int)espnowInit_, (int)WiFi.getMode(), (int)WiFi.status(), (int)WiFi.isConnected(), (int)WiFi.channel(), (int)peerExists);
    ++sendFailCount_;
    ++sendConsecFails_;
    lastSendFailMs_ = millis();
    if (millis() >= nextEspNowReinitMs)
    {
      espnowInit_ = false;
      initEspNow();
      nextEspNowReinitMs = millis() + 500; // half-second backoff
    }
    // If we keep failing, try a deeper reinit (deinit + init) every 5s
    if (sendConsecFails_ >= 3 && millis() - lastEspNowRecoveryMs_ > 5000)
    {
      LOGE("Gossip: consecutive send failures >=3; performing deeper ESP-NOW recovery");
      lastEspNowRecoveryMs_ = millis();
      espnowInit_ = false;
      esp_err_t d = esp_now_deinit();
      LOGT("esp_now_deinit rc=%d", (int)d);
      delay(50);
      initEspNow();
    }
  }
  else
  {
    LOGV("Gossip send OK");
    ++sendSuccessCount_;
    sendConsecFails_ = 0;
  }
}

void FloorController::updatePeer(const uint8_t *mac, uint32_t rank, uint8_t floor, bool is_leader)
{
  const uint32_t now = millis();
  // Try to find an existing entry
  int emptyIndex = -1;
  int oldestIndex = -1;
  uint32_t oldestTs = UINT32_MAX;
  for (uint8_t i = 0; i < MAX_FLOORS; ++i)
  {
    if (!peers_[i].valid)
    {
      if (emptyIndex < 0)
        emptyIndex = i;
      continue;
    }
    if (memcmp(peers_[i].mac, mac, 6) == 0)
    {
      // Update existing entry
      peers_[i].rank = rank;
      peers_[i].floor_id = floor;
      peers_[i].lastSeenMs = now;
      peers_[i].valid = true;
      return;
    }
    if (peers_[i].lastSeenMs < oldestTs)
    {
      oldestTs = peers_[i].lastSeenMs;
      oldestIndex = i;
    }
  }
  int idx = (emptyIndex >= 0) ? emptyIndex : oldestIndex;
  if (idx < 0 || idx >= MAX_FLOORS)
    return; // shouldn't happen
  memcpy(peers_[idx].mac, mac, 6);
  peers_[idx].rank = rank;
  peers_[idx].floor_id = floor;
  peers_[idx].lastSeenMs = now;
  peers_[idx].valid = true;
  LOGV("updatePeer: idx=%d mac=%02X:%02X:%02X:%02X:%02X:%02X rank=0x%06X floor=%d", idx,
       peers_[idx].mac[0], peers_[idx].mac[1], peers_[idx].mac[2], peers_[idx].mac[3], peers_[idx].mac[4], peers_[idx].mac[5], (unsigned)peers_[idx].rank, (int)peers_[idx].floor_id);
}

// Diagnostic helper: dump esp-now and wifi/gossip metrics to serial
void FloorController::dumpEspNowStatus()
{
  static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool peer = esp_now_is_peer_exist(BCAST);
  LOGI("ESP-NOW status: init=%d, peer=%d, attempts=%u ok=%u fail=%u consecFail=%u, wifiMode=%d wifiStat=%d wifiConnected=%d ch=%d",
       (int)espnowInit_, (int)peer,
       (unsigned)sendAttemptCount_, (unsigned)sendSuccessCount_, (unsigned)sendFailCount_, (unsigned)sendConsecFails_,
       (int)WiFi.getMode(), (int)WiFi.status(), (int)WiFi.isConnected(), (int)WiFi.channel());
  // Print the peer table entries (lastSeen within LEADER_PEER_TIMEOUT_MS)
  const uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_FLOORS; ++i)
  {
    const auto &p = peers_[i];
    if (!p.valid)
      continue;
    const bool expired = ((now - p.lastSeenMs) > LEADER_PEER_TIMEOUT_MS);
    LOGT("Peer %d: mac=%02X:%02X:%02X:%02X:%02X:%02X rank=0x%06X floor=%u lastSeenMs=%u expired=%d",
         (int)i, p.mac[0], p.mac[1], p.mac[2], p.mac[3], p.mac[4], p.mac[5], (unsigned)p.rank, (unsigned)p.floor_id, (unsigned)p.lastSeenMs, (int)expired);
  }

#if defined(DEBUG_PRINT_MODEL) && (DEBUG_PRINT_MODEL)
  // Optional: print the full protocol MODEL for deep diagnostics when enabled
  debugPrintModel(Serial);
#endif
}

void FloorController::evalLeader()
{
  const uint32_t now = millis();
  const uint32_t since = now - lastPeerSeenMs_;

  bool wantLeader = false;

  // Compute the current peer candidate (lowest MAC/rank) considering only
  // peers we've seen recently (within LEADER_PEER_TIMEOUT_MS). Include
  // ourselves in the candidate set.
  uint8_t candMac[6];
  memcpy(candMac, mac_, 6);
  uint32_t candRank = selfRank_;
  uint8_t peerCount = 0;
  for (uint8_t i = 0; i < MAX_FLOORS; ++i)
  {
    auto &p = peers_[i];
    if (!p.valid)
      continue;
    if ((now - p.lastSeenMs) > LEADER_PEER_TIMEOUT_MS)
    {
      // mark expired entries as invalid for a cleaner view and accurate peerCount
      p.valid = false;
      continue;
    }
    peerCount++;
    // pick the minimum mac/rank
    const int cm = cmpMac(p.mac, candMac);
    if (cm < 0 || (cm == 0 && p.rank < candRank))
    {
      memcpy(candMac, p.mac, 6);
      candRank = p.rank;
    }
  }

  if (peerCount == 0)
  {
    // No peers recently → we are alone; take leadership immediately.
    wantLeader = true;
  }
  else
  {
    const int cm = cmpMac(mac_, candMac);
    wantLeader = (cm < 0) || (cm == 0 && selfRank_ <= candRank);
  }
  // Debug info: show candidate and our state to help troubleshoot election
  LOGT("evalLeader: peers=%d wantLeader=%d cand=%02X:%02X:%02X:%02X:%02X:%02X candRank=0x%06X self= %02X:%02X:%02X:%02X:%02X:%02X selfRank=0x%06X since=%u",
       (int)peerCount, (int)wantLeader,
       candMac[0], candMac[1], candMac[2], candMac[3], candMac[4], candMac[5], (unsigned)candRank,
       mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5], (unsigned)selfRank_, (unsigned)since);

  if (wantLeader != isLeader_)
  {
    // If alone, flip immediately; if peers exist, use small hysteresis
    const bool peersExist = (peerCount > 0);
    if (!peersExist)
    {
      isLeader_ = true;
      leaderPending_ = false;
      LOGI("Role -> LEADER (no peers)");
      ensureBroadcastPeer(); // defensive refresh
    }
    else
    {
      // peers exist: keep your pending window to avoid flapping
      if (!leaderPending_ || pendingLeaderState_ != wantLeader)
      {
        leaderPending_ = true;
        pendingLeaderState_ = wantLeader;
        leaderPendingSinceMs_ = now;
        LOGI("Role change pending -> %s", wantLeader ? "LEADER" : "FOLLOWER");
      }
      else if (now - leaderPendingSinceMs_ >= ROLE_SWITCH_MS)
      {
        isLeader_ = wantLeader;
        leaderPending_ = false;
        LOGI("Role -> %s", isLeader_ ? "LEADER" : "FOLLOWER");
        ensureBroadcastPeer();
      }
    }
  }
  else
  {
    leaderPending_ = false;
  }
}

// ---------- Wi-Fi / MQTT ----------
void FloorController::tickLeaderNet()
{
  // Leader keeps radio in STA mode
  if (WiFi.getMode() != WIFI_STA)
    WiFi.mode(WIFI_STA);

  // Retry Wi-Fi.begin() no more than once every 5 s
  static uint32_t nextBeginMs = 0;
  static bool loggedIP = false;

  // (Re)start association when not connected
  if (!WiFi.isConnected())
  {
    loggedIP = false; // force IP log next time we connect
    const wl_status_t st = WiFi.status();

    // reflect offline network state in protocol MODEL
    setNetwork(0);

    if (millis() >= nextBeginMs)
    {
      // Stable bring-up knobs
      WiFi.persistent(false);
      WiFi.setSleep(false);
      WiFi.setAutoReconnect(true);

      LOGI("WiFi.begin SSID='%s'", WIFI_SSID);
      WiFi.begin(WIFI_SSID, WIFI_PASS);

      // don’t call begin again for 5 s
      nextBeginMs = millis() + 5000;
    }
    else
    {
      static uint32_t lastLog = 0;
      if (millis() - lastLog > 2000)
      {
        lastLog = millis();
        LOGI("WiFi status=%d (0 idle, 3 got IP) waiting AP…", (int)st);
      }
    }
    return; // wait for GOT_IP
  }

  // Connected
  if (!loggedIP)
  {
    loggedIP = true;
    LOGI("WiFi connected %s", WiFi.localIP().toString().c_str());
    ensureBroadcastPeer(); // after STA assoc, refresh peer entry

    // Update protocol model with the STA MAC and mark network=WiFi
    uint8_t staMac[6];
    WiFi.macAddress(staMac);
    char macbuf[18];
    snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X",
             staMac[0], staMac[1], staMac[2], staMac[3], staMac[4], staMac[5]);
    setMac(String(macbuf));
    setNetwork(1);
  }

  // MQTT connect with backoff
  if (!mqtt_.connected())
  {
    const uint32_t now = millis();
    if (now - lastMqttTryMs_ >= mqttBackoffMs_)
    {
      lastMqttTryMs_ = now;
      // String cid = String("esp-") + String((uint32_t)ESP.getEfuseMac(), HEX);
      String cid = MQTT_CLIENT_ID;
      LOGI("MQTT connect %s:%d cid=%s", MQTT_HOST, MQTT_PORT, cid.c_str());
      if (mqtt_.connect(cid.c_str(), MQTT_USER, MQTT_PASS))
      {
        LOGI("MQTT connected");

        mqtt_.setCallback(onRxMQTT_);
        mqtt_.subscribe("ELEC520/security/#");
        mqttBackoffMs_ = 500;
        mqttStabilizeMs_ = now;

        // Mark MQTT up in protocol MODEL
        setNetwork(2);

        // ---- SMOKE TEST: publish MAC + floor (retained) ----
        // Build MAC string
        uint8_t mac[6];
        WiFi.macAddress(mac); // STA MAC (since we're in STA mode)
        char macbuf[18];
        snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        // Topic & payload
        String topic = String(SITE_ID) + "/smoke";
        String payload = String("{\"mac\":\"") + macbuf + "\",\"floor\":" + String((int)FLOOR_ID) + "}";

        // Retain so it’s visible immediately to late subscribers
        const bool ok = mqtt_.publish(topic.c_str(), payload.c_str(), /*retain=*/true);
        {
          const unsigned payloadLen = (unsigned)payload.length();
          const int showLen = (int)min<unsigned>(payloadLen, DEBUG_TRUNC);
          LOGI("Smoke publish %s (len=%u) %s payload=%.*s",
               topic.c_str(), payloadLen, ok ? "OK" : "FAIL", showLen, payload.c_str());
        }
        // ---- end smoke ----
      }
      else
      {
        LOGE("MQTT connect failed state=%d", mqtt_.state());
        mqttBackoffMs_ = min<uint32_t>(mqttBackoffMs_ * 2, 10000);
      }
    }
    return;
  }

  // small settle window before publishing
  if (millis() - mqttStabilizeMs_ < 2000)
    return;
}

void FloorController::tickPublish()
{
  const uint32_t now = millis();
  if (!mqtt_.connected())
    return;
  if (now - lastSummaryMs_ < MQTT_SUMMARY_MS)
    return;
  lastSummaryMs_ = now;
  
  // Publish system state (ARMED/DISARMED/ALARM) so Node-RED can see confirmations
  // and echo back the state as an ACK. This is the single source of truth for state.
  {
    String topic = cloudTopicSystemState();
    String payload = String((int)MODEL.systemState);
    mqtt_.publish(topic.c_str(), payload.c_str());
    LOGI("Publish system state=%d to MQTT topic=%s", (int)MODEL.systemState, topic.c_str());
  }
  
  // Note: We don't publish keypad/network/mac periodically since they create
  // feedback loops when the MQTT broker retains them and sends them back.
  // Only system state (s/st) is published for Node-RED to confirm/echo.

  // Also gossip the system-level compact string via ESP-NOW so followers
  // that aren't MQTT-connected receive system updates and converge.
  {
    String sys;
    sys.reserve(128);
    sys += String("s/st:") + String((int)MODEL.systemState);
    sys += ";s/ke:" + String((int)MODEL.keypad);
    sys += ";n/st:" + String((int)MODEL.network);
    sys += ";n/mc:" + MODEL.mac;
    // Gossip trigger location if alarm is active so followers know where alarm occurred
    if (MODEL.systemState == SystemState::ALARM)
    {
      sys += ";s/tr:" + MODEL.triggerLoc;
    }
    sendSystemLine(sys, true);
    LOGT("Gossiped system compact string: %s", sys.c_str());
  }

  // Publish snapshots for ALL floors, not just our own.
  for (uint8_t f = 0; f < MAX_FLOORS; ++f)
  {
    String topic = cloudTopicFloor(f);
    String payload = buildFloorMqttString(f);

    const unsigned payloadLen = (unsigned)payload.length();
    const int showLen = (int)min<unsigned>(payloadLen, DEBUG_TRUNC);
    if (payloadLen > 0)
    {
      LOGI("Publish tick: floor=%u topic=%s payloadLen=%u payload=%.*s",
           (unsigned)f, topic.c_str(), payloadLen, showLen, payload.c_str());
    }
    else
    {
      LOGV("Publish tick: floor=%u topic=%s payloadLen=0", (unsigned)f, topic.c_str());
    }

    if (!payload.length())
      continue; // nothing known for this floor yet

    const bool ok = mqtt_.publish(topic.c_str(), payload.c_str(), /*retain*/ false);
    if (ok)
    {
      LOGI("Published floor %u (%uB)", (unsigned)f, (unsigned)payload.length());
    }
    else
    {
      LOGE("Publish failed floor=%u state=%d — forcing reconnect", (unsigned)f, mqtt_.state());
      mqtt_.disconnect();
      // reflect loss of MQTT but keep WiFi marked as available
      setNetwork(1);
      return; // let tickLeaderNet() reconnect
    }
  }
}

// ---------- Room I2C ----------
void FloorController::tickRoomI2C()
{
  const uint32_t now = millis();
  if (now - lastI2CPollMs_ < I2C_POLL_MS)
    return;
  lastI2CPollMs_ = now;

  static const uint8_t ADDR[ROOM_ADDR_COUNT] = ROOM_I2C_ADDRS;
  static bool addrPresent[ROOM_ADDR_COUNT] = {false};
  static bool addressesChecked = false;

#if SIMULATE_I2C
  // -------------------------
  // PROTOCOL-DRIVEN SIMULATOR
  // -------------------------
  // For each configured "room slot", synthesize a protocol-compliant room compact string
  // by first mutating the protocol MODEL, then calling buildRoomEspString(floor, room).
  //
  // This exercises: MODEL <-[parseRoomEspString]—room lines (via our common path)
  // and later: MQTT publishes via buildFloorMqttString(f) for ALL floors.

  for (uint8_t i = 0; i < ROOM_ADDR_COUNT; i++)
  {
    if (!ADDR[i])
      continue;                           // skip unused slot
    const uint8_t roomId = i % MAX_ROOMS; // one room per slot

    // --- Mutate the protocol model for our floor/room ---
    addRoom(FLOOR_ID, roomId);
    // mark room connected and bump timestamps (Unix-like seconds)
    MODEL.floors[FLOOR_ID].rooms[roomId].used = true;
    MODEL.floors[FLOOR_ID].rooms[roomId].connected = true;
    MODEL.floors[FLOOR_ID].rooms[roomId].ts = (uint32_t)(now / 1000);

    // Ultrasound demo channel u/0: sawtooth 0..200
    const uint8_t uval = (uint8_t)((now / 200) % 200);
    addUltra(FLOOR_ID, roomId, 0);
    MODEL.floors[FLOOR_ID].rooms[roomId].ultra[0].value = uval;

    // Hall demo channel h/0: toggles every ~2.5 s
    const bool hopen = ((now / 2500) & 1) != 0;
    addHall(FLOOR_ID, roomId, 0);
    MODEL.floors[FLOOR_ID].rooms[roomId].hall[0].open = hopen;

    // --- Build a COMPACT room string identical to what a Nano should send ---
    String line = buildRoomEspString(FLOOR_ID, roomId);

    // Log + ingest via the *system* parser so the common path is identical
    LOGT("I2C[SIM] room %u (raw): %s", (unsigned)roomId, line.c_str());
    // Normalize token formatting for older Nano test code that uses spaces or embedded /u/ tokens.
    String normLine = normalizeRoomLine(line);
    if (normLine != line)
      LOGV("I2C[SIM] normalized room %u: %s -> %s", (unsigned)roomId, line.c_str(), normLine.c_str());
    bool ok = parseRoomEspString(normLine.c_str());
    if (!ok)
      LOGV("I2C: parseRoomEspString failed for: %s", normLine.c_str());
    else
    {
      LOGT("I2C: parseRoomEspString OK for: %s", normLine.c_str());
      // Explicitly mark room and floor as connected since data was received
      MODEL.floors[FLOOR_ID].connected = true;
      MODEL.floors[FLOOR_ID].rooms[roomId].connected = true;
    }
    // Cache + gossip so other floors ingest it too
    lastLocal_[i % MAX_ROOMS] = normLine;
    sendRoomLine(normLine, true);
  }

#else
  // -------------------------
  // REAL I²C PATH (unchanged)
  // -------------------------
  if (!addressesChecked)
  {
    addressesChecked = true;
    for (uint8_t i = 0; i < ROOM_ADDR_COUNT; ++i)
    {
      const uint8_t a = ADDR[i];
      if (!a)
      {
        addrPresent[i] = false;
        continue;
      }
#if SIMULATE_I2C
      addrPresent[i] = true; // simulate presence if slot non-zero
#else
      Wire.beginTransmission(a);
      uint8_t tx = Wire.endTransmission(true);
      addrPresent[i] = (tx == 0);
#endif
      LOGV("I2C: probe addr 0x%02X present=%d", a, addrPresent[i]);
    }
    LOGI("I2C: finished address presence scan");
  }

  for (uint8_t i = 0; i < ROOM_ADDR_COUNT; i++)
  {
    // Ensure broadcast peer exists and is up-to-date
    ensureBroadcastPeer();
    const uint8_t a = ADDR[i];
    if (!a || !addrPresent[i])
      continue;
    char buf[I2C_RX_MAX + 1]{0};
    int n = rb_.readLine(a, (uint8_t *)buf, I2C_RX_MAX, I2C_REQUEST_LEN, I2C_REPLY_WAIT_MS);
    if (n <= 0)
      continue;
    buf[n] = 0;
    String line(buf);
    line.trim();
    if (!line.length())
      continue;

    int f = extractFloor(line);
    if (f != FLOOR_ID)
      continue;

    LOGT("I2C room data from addr 0x%02X: %s", (unsigned)a, line.c_str());

    // Normalize potential compact single-token lines from older Nano firmware
    String normLine = normalizeRoomLine(line);
    if (normLine != line)
      LOGT("I2C normalized room: %s -> %s", line.c_str(), normLine.c_str());
    // IMPORTANT: feed via protocol’s system parser (unifies ingestion path)
    bool ok = parseRoomEspString(normLine.c_str());
    if (!ok)
      LOGT("Rx: parseRoomEspString failed for: %s", normLine.c_str());
    else
    {
      LOGT("Rx: parseRoomEspString OK for: %s", normLine.c_str());
      // Explicitly mark this room as connected since it sent data
      // Extract room ID from line (format: "f/{f}/r/{r}/cs:{c}...")
      int rpos = normLine.indexOf("/r/");
      if (rpos >= 0)
      {
        int rend = normLine.indexOf('/', rpos + 3);
        if (rend < 0)
          rend = normLine.indexOf(':', rpos + 3);
        if (rend < 0)
          rend = normLine.indexOf(';', rpos + 3);
        if (rend < 0)
          rend = normLine.length();
        String rstr = normLine.substring(rpos + 3, rend);
        uint8_t roomId = (uint8_t)rstr.toInt();
        if (roomId < MAX_ROOMS)
        {
          // Mark both floor and room as connected
          MODEL.floors[FLOOR_ID].connected = true;
          MODEL.floors[FLOOR_ID].rooms[roomId].connected = true;
        }
      }
    }

    lastLocal_[i % MAX_ROOMS] = normLine; // cache last per-room
    sendRoomLine(normLine, true);         // gossip to peers (force local re-gossip)
  }
#endif
}

void FloorController::sendRoomLine(const String &line, bool force)
{
  if (extractFloor(line) != FLOOR_ID)
    return;
  uint32_t h = fnv1a((const uint8_t *)line.c_str(), line.length()), now = millis();
  // If we've already seen this line and caller didn't request a forced resend,
  // skip doing the send. But rate-limit the 'skipping' log to avoid spam.
  if (seen(h, now) && !force)
  {
    static uint32_t lastDuplicateLogMs = 0;
    const uint32_t DUPLICATE_LOG_MS = 10000; // only log a duplicate message every 10s
    uint32_t t = millis();
    if (t - lastDuplicateLogMs > DUPLICATE_LOG_MS)
    {
      lastDuplicateLogMs = t;
      LOGV("sendRoomLine: skipping duplicate line h=0x%08X len=%u", h, (unsigned)line.length());
    }
    return;
  }
  // Mark payload as seen so we don't re-process it from peers.
  remember(h, now);
  static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (!espnowInit_)
  {
    initEspNow();
    if (!espnowInit_)
      return;
  }

  // Ensure broadcast peer is present before sending
  ensureBroadcastPeer();
  ++sendAttemptCount_;
  esp_err_t rc = esp_now_send(BCAST, (const uint8_t *)line.c_str(), line.length());
  static uint32_t nextEspNowReinitMs = 0;
  if (rc != ESP_OK)
  {
    LOGE("Gossip/Room send rc=%d -> reinit ESP-NOW", (int)rc);
    ++sendFailCount_;
    ++sendConsecFails_;
    lastSendFailMs_ = millis();
    if (millis() >= nextEspNowReinitMs)
    {
      espnowInit_ = false;
      initEspNow();
      nextEspNowReinitMs = millis() + 500; // half-second backoff
    }
    if (sendConsecFails_ >= 3 && millis() - lastEspNowRecoveryMs_ > 5000)
    {
      LOGE("Room gossip: consecutive send failures >=3; performing deeper ESP-NOW recovery");
      lastEspNowRecoveryMs_ = millis();
      espnowInit_ = false;
      esp_err_t d = esp_now_deinit();
      LOGT("esp_now_deinit rc=%d", (int)d);
      delay(50);
      initEspNow();
    }
  }
  else
  {
    LOGT("Room line gossiped (%u bytes)", (unsigned)line.length());
    ++sendSuccessCount_;
    sendConsecFails_ = 0;
  }
}

// Send system-level compact strings (s/st, s/ke, n/st, n/mc, etc.) over ESP-NOW
// so followers that aren't MQTT-connected can converge on the same MODEL.
void FloorController::sendSystemLine(const String &line, bool force)
{
  uint32_t h = fnv1a((const uint8_t *)line.c_str(), line.length()), now = millis();
  if (self_ == nullptr)
    return;
  if (self_->seen(h, now) && !force)
  {
    static uint32_t lastDup = 0;
    if (millis() - lastDup > 10000)
    {
      lastDup = millis();
      LOGV("sendSystemLine: skipping duplicate line h=0x%08X len=%u", h, (unsigned)line.length());
    }
    return;
  }
  // Mark seen
  self_->remember(h, now);
  static const uint8_t BCAST[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  if (!espnowInit_)
  {
    initEspNow();
    if (!espnowInit_)
      return;
  }
  ensureBroadcastPeer();
  ++sendAttemptCount_;
  esp_err_t rc = esp_now_send(BCAST, (const uint8_t *)line.c_str(), line.length());
  if (rc != ESP_OK)
  {
    LOGE("Gossip/System send rc=%d -> reinit ESP-NOW", (int)rc);
    ++sendFailCount_;
    ++sendConsecFails_;
    lastSendFailMs_ = millis();
    if (sendConsecFails_ >= 3 && millis() - lastEspNowRecoveryMs_ > 5000)
    {
      LOGE("System gossip: consecutive send failures >=3; deeper recovery");
      lastEspNowRecoveryMs_ = millis();
      espnowInit_ = false;
      esp_err_t d = esp_now_deinit();
      LOGT("esp_now_deinit rc=%d", (int)d);
      delay(50);
      initEspNow();
    }
  }
  else
  {
    LOGT("System line gossiped (%u bytes)", (unsigned)line.length());
    ++sendSuccessCount_;
    sendConsecFails_ = 0;
  }
}

// Alarm / state-machine: scan MODEL for triggers when we're leader and cloud says ARMED
// Behavior change: the ESP leader only raises an ALARM when the last MQTT/cloud state
// is ARMED. If cloud reports DISARMED, ESP sensor triggers are ignored. Once ALARM
// is raised it persists until the cloud explicitly DISARMs.
void FloorController::alarmSystemStateMachine()
{
  if (!isLeader_){
    return;
  }

  // Only attempt to raise an alarm when the cloud/Node-RED state is ARMED.
  if (MODEL.systemState != SystemState::ARMED){
    return;
  }

  // Rate-limit MQTT alarm trigger publishes to avoid spamming the broker.
  // The interval is configurable via `ALARM_MQTT_MS` in `config.h`.
  const uint32_t now = millis();
  if (now - lastAlarmPublishMs_ < ALARM_MQTT_MS)
    return;
  lastAlarmPublishMs_ = now;


  String topic;
  String payload;

  // Scan configured floors/rooms/sensors continuously
  for (uint8_t i = 0; i < SMP_MAX_FLOORS; ++i)
  {
    if (!MODEL.floors[i].used || !MODEL.floors[i].connected)
    {
      LOGV("Skipping floor %u: used=%d connected=%d", (unsigned)i,
           (int)MODEL.floors[i].used,
           (int)MODEL.floors[i].connected);
      continue;
    }
    for (uint8_t j = 0; j < SMP_MAX_ROOMS; ++j)
    {
      if (!MODEL.floors[i].rooms[j].used || !MODEL.floors[i].rooms[j].connected)
      {
        LOGV("Skipping floor %u room %u: used=%d connected=%d", (unsigned)i, (unsigned)j,
             (int)MODEL.floors[i].rooms[j].used,
             (int)MODEL.floors[i].rooms[j].connected);
        continue;
      }
      for (uint8_t k = 1; k < 2; ++k)
      {
        // Hall sensors
        // if (MODEL.floors[i].rooms[j].hall[k].open)
        // {
        //   // First-detected trigger: set trigger location, publish, and elevate
        //   // system state to ALARM. Use protocol setters to keep MODEL in sync.
        //   setTriggerLoc(i, j, 0, k);
        //   topic = cloudTopicTrigger();
        //   payload = MODEL.triggerLoc;
        //   mqtt_.publish(topic.c_str(), payload.c_str());
        //   LOGE("Alarm detected (hall) at %s -> publishing trigger", payload.c_str());

        //   // Mark system as ALARM and remember timestamp for auto-clear
        //   setSystemState((uint8_t)SystemState::ALARM);
        //   lastAlarmStateChangeMs_ = now;
        //   // Immediately gossip system state so followers converge quickly
        //   {
        //     String sys = String("s/st:") + String((int)MODEL.systemState) + ";s/ke:" + String((int)MODEL.keypad) + ";n/st:" + String((int)MODEL.network) + ";n/mc:" + MODEL.mac;
        //     sendSystemLine(sys, true);
        //   }
        //   // Stop scanning further sensors this pass to avoid multiple publishes
        //   return;
        // }
        // Ultra sensors
        if (MODEL.floors[i].rooms[j].ultra[k].value <= ULTRA_THRESHOLDS)
        {
          setTriggerLoc(i, j, k, 0);
          topic = cloudTopicTrigger();
          payload = MODEL.triggerLoc;
          mqtt_.publish(topic.c_str(), payload.c_str());
          //LOGE("Alarm detected (ultra) at %s -> publishing trigger location only", payload.c_str());
          LOGE("Alarm detected (ultra) at %s -> floor=%u room=%u sensor=%u value=%u", payload.c_str(), (unsigned)i, (unsigned)j, (unsigned)k, (unsigned)MODEL.floors[i].rooms[j].ultra[k].value);

          // Set local ALARM state (this is what we detected, not what MQTT said)
          setSystemState((uint8_t)SystemState::ALARM);
          lastAlarmStateChangeMs_ = now;
          
          // DO NOT gossip system state - MQTT is the source of truth for state
          // Only publish trigger location to MQTT (already done above)
          // Followers will receive this via MQTT if they're subscribed
          LOGE("ALARM raised locally; awaiting MQTT DISARM to clear");
          return;
        }
      }
    }
  }
}

// Trigger or clear physical alarm output
void FloorController::triggerAlarm()
{
  // Mirror reference behavior: only drive the alarm output when the
  // global MODEL.systemState == SystemState::ALARM. Print a periodic
  // message (once per second) with the triggered location for visibility.
  const uint32_t now = millis();
  if (MODEL.systemState == SystemState::ALARM)
  {
    digitalWrite(23, HIGH);
    static unsigned long publishMsg = 0;
    if (now - publishMsg >= 1000)
    {
      String alarmLocation = MODEL.triggerLoc;
      publishMsg = now;
      LOGE("ALARM TRIGGERED in zone %s", alarmLocation.c_str());
    }

    // Per new behavior: DO NOT auto-clear the ALARM here. The system will remain
    // in ALARM until the cloud/Node-RED explicitly sends a DISARM. This prevents
    // local auto-clears from re-triggering or conflicting with cloud intent.
    // (If you want auto-clear behavior in the future, re-enable the block above.)
  }
  else
  {
    digitalWrite(23, LOW);
  }
}
void FloorController::clearAlarm()
{
  digitalWrite(23, LOW);
}

// ---------- dedupe ----------
uint32_t FloorController::fnv1a(const uint8_t *d, int n)
{
  uint32_t h = 0x811C9DC5u;
  for (int i = 0; i < n; i++)
  {
    h ^= d[i];
    h *= 0x01000193u;
  }
  return h;
}
bool FloorController::seen(uint32_t h, uint32_t now)
{
  for (uint8_t i = 0; i < RECENT_CACHE_SIZE; i++)
    if (recent_[i].h == h && (now - recent_[i].ts) < RECENT_TTL_MS)
      return true;
  return false;
}
void FloorController::remember(uint32_t h, uint32_t now)
{
  recent_[rHead_] = {h, now};
  rHead_ = (rHead_ + 1) % RECENT_CACHE_SIZE;
}
