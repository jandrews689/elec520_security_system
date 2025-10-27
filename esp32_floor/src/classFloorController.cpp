// -------- esp32_floor/src/classFloorController.cpp --------
#include "classFloorController.h"

// --- debug macros ---
#if DEBUG_LEVEL >= 1
#define LOGE(...)                                               \
    do                                                         \
    {                                                         \
        Serial.printf("\n[ERROR %010lu] ", (unsigned long)millis());\
        Serial.printf(__VA_ARGS__);                           \
        Serial.println();                                     \
        Serial.println();                                     \
    } while (0)
#else
#define LOGE(...) \
    do            \
    {             \
    } while (0)
#endif

#if DEBUG_LEVEL >= 2
#define LOGI(...)                                                \
    do                                                          \
    {                                                          \
        Serial.printf("\n[INFO  %010lu] ", (unsigned long)millis());\
        Serial.printf(__VA_ARGS__);                            \
        Serial.println();                                      \
    } while (0)
#else
#define LOGI(...) \
    do            \
    {             \
    } while (0)
#endif

#if DEBUG_LEVEL >= 2
#define LOGV(...)                          \
    do                                     \
    {                                      \
        Serial.printf("[V] " __VA_ARGS__); \
        Serial.println();                  \
    } while (0)
#else
#define LOGV(...) \
    do            \
    {             \
    } while (0)
#endif

#if DEBUG_LEVEL >= 4
#define LOGT(...)                                                  \
    do                                                            \
    {                                                            \
        Serial.printf("[TRACE %010lu] ", (unsigned long)millis());\
        Serial.printf(__VA_ARGS__);                              \
        Serial.println();                                        \
    } while (0)
#else
#define LOGT(...) \
    do            \
    {             \
    } while (0)
#endif

// Hex dump helper (only compiles in VERBOSE+)
#if DEBUG_LEVEL >= 3
static void hexdump(const uint8_t *p, int n, int max = 64)
{
    if (!p || n <= 0)
        return;
    if (n > max)
        n = max;
    for (int i = 0; i < n; i++)
    {
        if ((i & 0x0F) == 0)
        {
            Serial.printf(i ? "\n    %04X: " : "    %04X: ", i);
        }
        Serial.printf("%02X ", p[i]);
    }
    Serial.println();
}
#endif

FloorController *FloorController::self_ = nullptr;

// --- leader election policy ---
static constexpr uint32_t LEADER_PEER_TIMEOUT_MS = 3000; // fallback to self after 3 s without peers
// Broadcast address for ESP‑NOW (6 bytes of 0xFF)
static const uint8_t ESPNOW_BROADCAST_ADDR[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
// Hysteresis delay for role switches (ms)
static constexpr uint32_t ROLE_SWITCH_MS = 1000;

// Lexicographic MAC comparison: returns -1 if a<b, 0 if equal, 1 if a>b
static inline int compareMac(const uint8_t *a, const uint8_t *b)
{
  for (int i = 0; i < 6; ++i)
  {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}
void FloorController::begin()
{
    Serial.begin(115200);
    delay(30);
    LOGI("Booting FloorController (DEBUG_LEVEL=%d)", (int)DEBUG_LEVEL);

    self_ = this;
  esp_read_mac(mac_, ESP_MAC_WIFI_STA);
  selfRank_ = ((uint32_t)mac_[3] << 16) | ((uint32_t)mac_[4] << 8) | (uint32_t)mac_[5];
  lowestRank_ = selfRank_;
  // initialize lowestMac_ to our own MAC (will be updated when peers are seen)
  memcpy(lowestMac_, mac_, 6);

    LOGI("MAC=%02X:%02X:%02X:%02X:%02X:%02X  rank=0x%06X  floor=%u  site=%s",
         mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5], (unsigned)selfRank_, (unsigned)FLOOR_ID, SITE_ID);

    // 1) ESP-NOW init (always on; followers operate with Wi-Fi off)
    initEspNow_();

    // 2) I²C room bus (optional; enabled by USE_I2C_ROOMBUS)
#if USE_I2C_ROOMBUS
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);
    LOGI("I2C started: SDA=%d SCL=%d @ %lu Hz", I2C_SDA_PIN, I2C_SCL_PIN, (unsigned long)I2C_CLOCK_HZ);
#else
    LOGI("I2C disabled (USE_I2C_ROOMBUS=0)");
#endif

    // 3) MQTT client pre-config (actual connect handled in ensureLeaderNet_)
    mqtt_.setServer(MQTT_HOST, MQTT_PORT);
    if (USE_TLS)
    {
        tls_.setInsecure(); // demo mode
        LOGI("MQTT target: %s:%d (TLS, insecure)", MQTT_HOST, MQTT_PORT);
    }
    else
    {
        LOGI("MQTT target: %s:%d (TCP)", MQTT_HOST, MQTT_PORT);
    }

    LOGI("begin() complete");
}

void FloorController::loop(){
  const uint32_t now = millis();
  
  // Status heartbeat - print every ~5 seconds at INFO level
  static uint32_t lastStatusMs = 0;
  if (now - lastStatusMs > 5000) {
    lastStatusMs = now;
    LOGI("Status: [%s] Floor=%u MAC=%02X:%02X:%02X:%02X:%02X:%02X", 
         isLeader_ ? "LEADER" : "FOLLOWER",
         FLOOR_ID,
         mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
  }

  LOGT("=== Loop Start === [t=%lu ms]", (unsigned long)now);
  
  // 1) Recompute leadership based on recent lowest MAC (lexicographic)
  LOGV("Leadership: current=%s, lowestMAC=%02X:%02X:%02X:%02X:%02X:%02X, self=%02X:%02X:%02X:%02X:%02X:%02X", 
    isLeader_ ? "LEADER" : "FOLLOWER", 
    lowestMac_[0], lowestMac_[1], lowestMac_[2], lowestMac_[3], lowestMac_[4], lowestMac_[5],
    mac_[0], mac_[1], mac_[2], mac_[3], mac_[4], mac_[5]);
  evaluateLeader_();

  // 2) Leader ensures Wi-Fi+MQTT, follower switches them off
  LOGT("Network Status: WiFi=%s, MQTT=%s", 
       WiFi.isConnected() ? "CONNECTED" : "DISCONNECTED",
       mqtt_.connected() ? "CONNECTED" : "DISCONNECTED");
  ensureLeaderNet_();

  // 3) Periodic gossip with per-node jitter to avoid collisions
  if (now - lastGossipMs_ > (GOSSIP_BASE_MS + (selfRank_%GOSSIP_JITTER_MS))) {
    lastGossipMs_ = now;
    LOGT("gossip tick hb=%lu", (unsigned long)hb_);
    sendGossip_();
  }

  // 3b) Poll I²C room adapters periodically and feed protocol model
#if USE_I2C_ROOMBUS
  if (now - lastI2CPollMs_ > I2C_POLL_MS) {
    lastI2CPollMs_ = now;
    static const uint8_t ADDRS[ROOM_ADDR_COUNT] = ROOM_I2C_ADDRS;
    for (uint8_t i=0; i<ROOM_ADDR_COUNT; ++i) {
      uint8_t addr = ADDRS[i];
      if (!addr) continue;
      char buf[I2C_RX_MAX+1];
      int n = roombus_.readLine(addr, (uint8_t*)buf, I2C_RX_MAX, I2C_REQUEST_LEN, I2C_REPLY_WAIT_MS);
      if (n > 0) {
        buf[n] = '\0';
        String line(buf); line.trim();
        LOGV("I2C addr=0x%02X bytes=%d line='%.*s'%s", addr, n,
             min((int)line.length(), (int)DEBUG_TRUNC), line.c_str(),
             (line.length()>(int)DEBUG_TRUNC? "…":""));
        if (line.length()) {
          parseRoomEspString(line);
          lastLocalRoomLine_[i % MAX_ROOMS] = line;
          sendRoomLine_(line);
        }
      } else {
        LOGT("I2C addr=0x%02X no-data", addr);
      }
    }
  }

  // 3b-2) Synthetic I2C simulation for testing (optional)
#if SIMULATE_I2C
  if (now - lastSimMs_ > SIMULATE_I2C_MS) {
    lastSimMs_ = now;
    // Simple rotating simulation: toggle room connection and random ultra values
    static uint8_t simRoom = 0;
    // mark model nodes used - only for our assigned floor
    addRoom(FLOOR_ID, simRoom);
    // toggle connection state
    bool cs = (now / SIMULATE_I2C_MS) & 1;
    setRoomConnection(FLOOR_ID, simRoom, cs);
    // set a pseudo-random ultrasonic value
    uint8_t uval = (uint8_t)((now / 37) & 0xFF);
    setUltraValue(FLOOR_ID, simRoom, 0, uval);
    // set a hall sensor toggle 
    setHallOpen(FLOOR_ID, simRoom, 0, ((now / 1234) & 1));

    // Build a compact ESP-NOW room string and feed it through the same path
    String simLine = buildRoomEspString(FLOOR_ID, simRoom);
    LOGI("SIM I2C -> Floor %u Room %u: %s", FLOOR_ID, simRoom, simLine.c_str());
    // Local parse (as if read from I2C)
    parseRoomEspString(simLine);
    lastLocalRoomLine_[simRoom % MAX_ROOMS] = simLine;
    // Also emulate broadcasting the line over ESP-NOW so other nodes see it
    sendRoomLine_(simLine);

    // advance room index only - stay on our floor
    simRoom = (simRoom + 1) % MAX_ROOMS;
  }
#endif
  // 3c) Re-gossip one cached local room line (round-robin) to drive convergence
  if (now - lastRoomTxMs_ > GOSSIP_ROOM_MS) {
    lastRoomTxMs_ = now;
    if (lastLocalRoomLine_[rrRoom_].length()) {
      LOGT("re-gossip room idx=%u", rrRoom_);
      sendRoomLine_(lastLocalRoomLine_[rrRoom_]);
    }
    rrRoom_ = (rrRoom_ + 1) % MAX_ROOMS;
  }
#endif

  // 4) Leader publishes summaries (floor snapshots) to MQTT
  maybePublishSummary_();

  if (mqtt_.connected()) mqtt_.loop();
}

void FloorController::initEspNow_(){
  // Ensure WiFi is in STA mode; ESP-NOW operates alongside STA/AP modes.
  WiFi.mode(WIFI_STA);

  // If ESP-NOW was previously initialized, deinit to ensure a clean reinit.
  esp_err_t dier = esp_now_deinit();
  if (dier == ESP_OK) {
    LOGT("esp_now_deinit succeeded (reinit)");
  }

#if ESPNOW_ENCRYPT
  esp_now_set_pmk((const uint8_t*)ESPNOW_PMK);
#endif
  esp_err_t ie = esp_now_init();
  if (ie!=ESP_OK){
    LOGE("esp_now_init failed: %d", (int)ie);
    // Don't crash the whole device; log and return so caller can retry.
    return;
  }
  LOGI("ESP-NOW init ok");

  esp_now_register_recv_cb(+[](const uint8_t* mac,const uint8_t* data,int len){
    onEspNowRx_(mac,data,len);
  });

  esp_now_peer_info_t p{}; memset(&p,0,sizeof(p)); memset(p.peer_addr,0xFF,6);
  p.channel=0; p.encrypt = ESPNOW_ENCRYPT ? 1 : 0;
#if ESPNOW_ENCRYPT
  memcpy(p.lmk, ESPNOW_LTK, 16);
#endif
  esp_err_t pe = esp_now_add_peer(&p);
  LOGI("Add broadcast peer: %s (rc=%d)", (pe==ESP_OK? "ok":"fail"), (int)pe);
}


void FloorController::onEspNowRx_(const uint8_t* mac, const uint8_t* data, int len){
  LOGT("=== ESP-NOW Receive === [len=%d bytes]", len);
  LOGT("Source MAC: %02X:%02X:%02X:%02X:%02X:%02X", 
       mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (len == (int)sizeof(GossipMsg)){
    const GossipMsg* g = (const GossipMsg*)data;
    LOGT("Message Type: Gossip Message");

    if (strncmp(g->site, SITE_ID, strlen(SITE_ID))==0){
      LOGT("Site Match: %s", g->site);
      // compare full MACs lexicographically for robust election
      int cmp = compareMac(g->mac, self_->lowestMac_);
      if (cmp < 0) {
        LOGT("!!! New Lowest MAC !!! Old=%02X:%02X:%02X:%02X:%02X:%02X New=%02X:%02X:%02X:%02X:%02X:%02X",
             self_->lowestMac_[0], self_->lowestMac_[1], self_->lowestMac_[2], self_->lowestMac_[3], self_->lowestMac_[4], self_->lowestMac_[5],
             g->mac[0], g->mac[1], g->mac[2], g->mac[3], g->mac[4], g->mac[5]);
        memcpy(self_->lowestMac_, g->mac, 6);
        self_->lowestRank_ = g->rank; // keep rank as diagnostic
      }
      self_->lastPeerSeenMs_ = millis();
      LOGT("Gossip Content: floor=%u rank=0x%06X mac=%02X:%02X:%02X:%02X:%02X:%02X hb=%lu leader=%u",
           g->floor_id, (unsigned)g->rank,
           g->mac[0], g->mac[1], g->mac[2], g->mac[3], g->mac[4], g->mac[5],
           (unsigned long)g->hb, (unsigned)g->is_leader);
    } else {
      LOGT("Site Mismatch: received=%s expected=%s", g->site, SITE_ID);
    }
    return;
  }

  // Application payload
  uint32_t h = fnv1a_(data, len);
  uint32_t now = millis();
  if (self_->seenRecently_(h, now)) {
    LOGT("RX room payload dup (hash=%08lX)", (unsigned long)h);
    return;
  }
  self_->remember_(h, now);

#if DEBUG_LEVEL >= 3
  LOGV("RX room payload len=%d hash=%08lX", len, (unsigned long)h);
  hexdump(data, len, 64);
#endif

  String s; s.reserve(len);
  for (int i=0;i<len;i++) s += (char)data[i];
  parseRoomEspString(s.c_str());
}


void FloorController::handleEspPayload_(const uint8_t *data, int len)
{
    // Not used in current path (onEspNowRx_ handles parsing directly).
    String s;
    s.reserve(len);
    for (int i = 0; i < len; i++)
        s += (char)data[i];
    parseRoomEspString(s.c_str());
}

void FloorController::sendGossip_(){
  LOGT("=== Gossip Send ===");
  GossipMsg g{}; g.proto=1; strncpy(g.site, SITE_ID, sizeof(g.site)); g.floor_id=FLOOR_ID;
  g.rank=selfRank_; memcpy(g.mac, mac_, 6); g.is_leader=isLeader_; g.hb=hb_++;
  
  LOGT("Gossip Content: site=%s, floor=%u, rank=0x%06X, leader=%u, hb=%lu",
       g.site, g.floor_id, (unsigned)g.rank, (unsigned)g.is_leader, (unsigned long)g.hb);
  
  esp_err_t rc = esp_now_send(ESPNOW_BROADCAST_ADDR, (uint8_t*)&g, sizeof(g));
  if (rc == ESP_ERR_ESPNOW_NOT_INIT) {
    LOGE("esp_now_send: not initialized (rc=%d) - attempting reinit", (int)rc);
    initEspNow_();
    esp_err_t rc2 = esp_now_send(ESPNOW_BROADCAST_ADDR, (uint8_t*)&g, sizeof(g));
    LOGT("Gossip Send Retry Result: hb=%lu status=%d (%s)",
         (unsigned long)g.hb, (int)rc2, rc2 == ESP_OK ? "OK" : "FAIL");
  } else {
    LOGT("Gossip Send Result: hb=%lu status=%d (%s)", 
         (unsigned long)g.hb, (int)rc, rc == ESP_OK ? "OK" : "FAIL");
  }
}

void FloorController::sendRoomLine_(const String& line){
  uint32_t h = fnv1a_((const uint8_t*)line.c_str(), line.length());
  uint32_t now = millis();
  if (seenRecently_(h, now)) { LOGT("TX room skipped dup hash=%08lX", (unsigned long)h); return; }
  remember_(h, now);
  esp_err_t rc = esp_now_send(ESPNOW_BROADCAST_ADDR, (const uint8_t*)line.c_str(), line.length());
  if (rc == ESP_ERR_ESPNOW_NOT_INIT) {
    LOGE("esp_now_send (room): not initialized (rc=%d) - attempting reinit", (int)rc);
    initEspNow_();
    esp_err_t rc2 = esp_now_send(ESPNOW_BROADCAST_ADDR, (const uint8_t*)line.c_str(), line.length());
    LOGV("TX room retry len=%u hash=%08lX rc=%d '%.*s'%s",
         (unsigned)line.length(), (unsigned long)h, (int)rc2,
         min((int)line.length(), (int)DEBUG_TRUNC), line.c_str(),
         (line.length()>(int)DEBUG_TRUNC? "…":""));
  } else {
    LOGV("TX room len=%u hash=%08lX rc=%d '%.*s'%s",
         (unsigned)line.length(), (unsigned long)h, (int)rc,
         min((int)line.length(), (int)DEBUG_TRUNC), line.c_str(),
         (line.length()>(int)DEBUG_TRUNC? "…":""));
  }
}


void FloorController::ensureLeaderNet_(){
  LOGT("=== Network Management ===");
  const uint32_t now = millis();
  
  if (!isLeader_) {
    LOGT("Role: FOLLOWER - checking network state");
    // Do NOT switch WiFi mode off entirely — ESP-NOW requires the WiFi driver
    // to remain available. Previously switching to WIFI_OFF could disable
    // ESP-NOW and cause missed gossip (leading to flip/flop leadership).
    if (WiFi.isConnected()){
      LOGV("Follower → disconnect WiFi (keep driver for ESP-NOW)");
      WiFi.disconnect();
    } else {
      LOGV("Follower: WiFi not connected (no action)");
    }
    return;
  }
  LOGT("Role: LEADER - managing network connections");

  // Ensure device is in STA mode and trigger a connect attempt whenever
  // we're leader and not currently connected. Previously we only called
  // WiFi.begin() when transitioning from WIFI_OFF -> WIFI_STA which meant
  // cases where mode was already STA but not connected never retriggered
  // a connection attempt.
  if (WiFi.getMode() == WIFI_OFF) {
    LOGI("Leader → WiFi STA begin SSID='%s'", WIFI_SSID);
    LOGT("Initializing WiFi: Mode=STA SSID='%s'", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  } else {
    if (WiFi.getMode() != WIFI_STA) {
      LOGT("Switching WiFi mode to STA (was mode=%d)", (int)WiFi.getMode());
      WiFi.mode(WIFI_STA);
    }
    // If not connected, call begin() to (re)start a connection attempt.
    if (!WiFi.isConnected()) {
      LOGT("WiFi not connected - calling WiFi.begin() (status=%d)", WiFi.status());
      WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
  }

  if (!WiFi.isConnected()){
    static uint32_t last = 0;
    if (now - last > 1000) {
      last = now;
      LOGT("WiFi Status: Connecting... [status=%d RSSI=%d dBm, Channel=%d]", 
           WiFi.status(), WiFi.RSSI(), WiFi.channel());
    }
    return;
  }

  static bool ipLogged=false;
  if (!ipLogged){ ipLogged=true; LOGI("WiFi connected: %s", WiFi.localIP().toString().c_str()); }

  if (!mqtt_.connected()){
    String cid = String("esp-")+String((uint32_t)ESP.getEfuseMac(),HEX);
    LOGI("MQTT connect cid=%s host=%s:%d", cid.c_str(), MQTT_HOST, MQTT_PORT);
    if (!mqtt_.connect(cid.c_str(), MQTT_USER, MQTT_PASS)){
      LOGE("MQTT connect failed, state=%d; retrying backoff=%lu ms",
           mqtt_.state(), (unsigned long)backoffMs_);
      if (millis() > nextConnTryMs_) { nextConnTryMs_ = millis() + backoffMs_; backoffMs_ = min<uint32_t>(backoffMs_*2, 10000); }
      return;
    }
    backoffMs_ = 500;
    LOGI("MQTT connected");
  }
}


void FloorController::maybePublishSummary_(){
  const uint32_t now = millis();
  LOGT("=== MQTT Summary Check === [t=%lu ms]", (unsigned long)now);
  
  if (!isLeader_) {
    LOGT("Skip: Not leader");
    return;
  }
  if (!mqtt_.connected()) {
    LOGT("Skip: MQTT not connected");
    return;
  }
  
  if (now - lastSummaryMs_ < MQTT_SUMMARY_MS) {
    LOGT("Skip: Too soon (next in %lu ms)", 
         (unsigned long)(MQTT_SUMMARY_MS - (now - lastSummaryMs_)));
    return;
  }
  
  LOGT("Publishing floor summaries...");
  lastSummaryMs_ = now;

  for (uint8_t f=0; f<MAX_FLOORS; ++f){
    String topic   = cloudTopicFloor(f);
    String payload = buildFloorMqttString(f);
    
    LOGT("Floor %u: Topic='%s'", f, topic.c_str());
    
    if (payload.length()){
      LOGT("Publishing payload (%u bytes): %s%s", 
           (unsigned)payload.length(),
           payload.substring(0, std::min(50, (int)payload.length())).c_str(),
           payload.length() > 50 ? "..." : "");
      
      bool ok = mqtt_.publish(topic.c_str(), payload.c_str());
      LOGI("MQTT pub %s (%u bytes): %s", 
           topic.c_str(), (unsigned)payload.length(), ok? "ok":"fail");
    } else {
      LOGT("Skip empty floor %u", f);
    }
  }
}


void FloorController::evaluateLeader_() {
  const uint32_t now = millis();
  bool wasLeader = isLeader_;
  uint32_t timeSinceLastPeer = now - lastPeerSeenMs_;

  LOGV("=== Leader Evaluation ===");
  LOGV("Time since last peer: %lu ms (timeout=%lu ms)", 
       (unsigned long)timeSinceLastPeer, 
       (unsigned long)LEADER_PEER_TIMEOUT_MS);

  bool desiredLeader;
  if (timeSinceLastPeer > LEADER_PEER_TIMEOUT_MS) {
    // Only log timeout if we're not already leader
    if (!isLeader_) {
      LOGI("Peer timeout exceeded (%lu ms) - candidate leadership", (unsigned long)timeSinceLastPeer);
    }
    // Candidate leadership when no peers seen recently
    desiredLeader = true;
  } else {
    // Determine desired leadership by lexicographic MAC ordering (lower MAC wins)
    int cmp = compareMac(mac_, lowestMac_);
    desiredLeader = (cmp <= 0);
  }

  // Hysteresis: require desired state to persist for ROLE_SWITCH_MS before switching
  if (desiredLeader != isLeader_) {
    if (!leaderPending_ || pendingLeaderState_ != desiredLeader) {
      leaderPending_ = true;
      pendingLeaderState_ = desiredLeader;
      leaderPendingSinceMs_ = now;
      LOGI("Leader change pending -> desired=%s (waiting %lums)", desiredLeader? "LEADER":"FOLLOWER", (unsigned long)ROLE_SWITCH_MS);
    } else if (now - leaderPendingSinceMs_ >= ROLE_SWITCH_MS) {
      // Apply pending state
      isLeader_ = pendingLeaderState_;
      leaderPending_ = false;
      LOGE("!!! Leader state changed: %s !!! [t=%lu ms]", isLeader_? "LEADER":"FOLLOWER", (unsigned long)now);
      // When assuming leadership reset lowest trackers
      if (isLeader_) {
        lowestRank_ = selfRank_;
        memcpy(lowestMac_, mac_, 6);
      }
    }
  } else {
    // Desired and actual match - clear any pending
    if (leaderPending_) {
      leaderPending_ = false;
      LOGT("Leader change cancelled - desired already satisfied");
    }
  }
}


// --- hashing & dedup (FNV-1a 32-bit) ---
static inline uint32_t fnv1a32(const uint8_t *d, int n)
{
    uint32_t h = 0x811C9DC5u;
    for (int i = 0; i < n; i++)
    {
        h ^= d[i];
        h *= 0x01000193u;
    }
    return h;
}
uint32_t FloorController::fnv1a_(const uint8_t *buf, int len) { return fnv1a32(buf, len); }
bool FloorController::seenRecently_(uint32_t h, uint32_t nowMs)
{
    for (uint8_t i = 0; i < RECENT_CACHE_SIZE; i++)
    {
        if (recent_[i].h == h && (nowMs - recent_[i].ts) < RECENT_TTL_MS)
            return true;
    }
    return false;
}
void FloorController::remember_(uint32_t h, uint32_t nowMs)
{
    recent_[recentHead_].h = h;
    recent_[recentHead_].ts = nowMs;
    recentHead_ = (recentHead_ + 1) % RECENT_CACHE_SIZE;
}