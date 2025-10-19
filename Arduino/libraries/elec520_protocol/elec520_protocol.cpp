#include "elec520_protocol.h"

// ---------------- Global ----------------
ProtocolModel MODEL;

// ---------------- Internals ----------------
static inline bool inRange(uint8_t v, uint8_t maxv){ return v < maxv; }

static int splitTopic(const String& t, String out[], int maxParts=12) {
  int n=0, start=0, i;
  while ((i=t.indexOf('/', start))>=0 && n<maxParts) { out[n++]=t.substring(start,i); start=i+1; }
  if (start <= (int)t.length() && n<maxParts) out[n++]=t.substring(start);
  return n;
}

static bool parseBool01(const char* s, bool& out){
  if (!s) return false;
  if (strcmp(s,"1")==0){ out=true;  return true; }
  if (strcmp(s,"0")==0){ out=false; return true; }
  return false;
}
static bool parseByte(const char* s, uint8_t& out){
  if (!s || !*s) return false;
  long v=strtol(s,nullptr,10);
  if (v<0 || v>255) return false;
  out=(uint8_t)v;
  return true;
}

static bool isCloudPrefix(const String& p0, const String& p1) {
  return (p0 == "ELEC520" && p1 == "security");
}

// ---------------- Adders ----------------
bool addFloor(uint8_t f_id){
  if (!inRange(f_id, SMP_MAX_FLOORS)) return false;
  MODEL.floors[f_id].used = true;
  return true;
}
bool addRoom(uint8_t f_id, uint8_t r_id){
  if (!addFloor(f_id)) return false;
  if (!inRange(r_id, SMP_MAX_ROOMS)) return false;
  MODEL.floors[f_id].rooms[r_id].used = true;
  return true;
}
bool addUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id){
  if (!addRoom(f_id, r_id)) return false;
  if (!inRange(u_id, SMP_MAX_SENSORS)) return false;
  MODEL.floors[f_id].rooms[r_id].ultra[u_id].used = true;
  return true;
}
bool addHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id){
  if (!addRoom(f_id, r_id)) return false;
  if (!inRange(hs_id, SMP_MAX_SENSORS)) return false;
  MODEL.floors[f_id].rooms[r_id].hall[hs_id].used = true;
  return true;
}

// ---------------- Setters ----------------
bool setSystemState(uint8_t s) { MODEL.systemState = s; return true; }
bool setKeypad(uint8_t k)      { MODEL.keypad      = k; return true; }
bool setNetwork(uint8_t n)     { MODEL.network     = n; return true; }
bool setMac(const String& mac) { MODEL.mac         = mac; return true; }

bool setFloorConnection(uint8_t f_id, bool cs01){
  if (!addFloor(f_id)) return false;
  MODEL.floors[f_id].connected = cs01;
  return true;
}
bool setRoomConnection(uint8_t f_id, uint8_t r_id, bool cs01){
  if (!addRoom(f_id, r_id)) return false;
  MODEL.floors[f_id].rooms[r_id].connected = cs01;
  return true;
}
bool setUltraValue(uint8_t f_id, uint8_t r_id, uint8_t u_id, uint8_t val){
  if (!addUltra(f_id, r_id, u_id)) return false;
  MODEL.floors[f_id].rooms[r_id].ultra[u_id].value = val;
  return true;
}
bool setHallOpen(uint8_t f_id, uint8_t r_id, uint8_t hs_id, bool open01){
  if (!addHall(f_id, r_id, hs_id)) return false;
  MODEL.floors[f_id].rooms[r_id].hall[hs_id].open = open01;
  return true;
}

// NEW: RSSI setter
bool setFloorRssi(uint8_t f_id, uint8_t rssi){
  if (!addFloor(f_id)) return false;
  MODEL.floors[f_id].rssi = rssi;
  return true;
}

void resetModel(){ MODEL = ProtocolModel(); }

// ---------------- Topic builders (Node) ----------------
String nodeTopicSystemState()                  { return "s/st"; }
String nodeTopicKeypad()                       { return "s/ke"; }
String nodeTopicNetwork()                      { return "n/st"; }
String nodeTopicMac()                          { return "n/mc"; }
String nodeTopicFloorConnection(uint8_t f_id)  { return "f/"+String(f_id)+"/cs"; }
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/cs"; }
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id); }
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id); }
// NEW: floor RSSI
String nodeTopicFloorRssi(uint8_t f_id)       { return "f/"+String(f_id)+"/rsi"; }

// ---------------- Topic builders (Cloud) ----------------
static inline String _CLOUD_BASE(){ return "ELEC520/security/"; }

//Builds topic for the floor. 
String cloudTopicFloor(uint8_t f_id) {
    return "ELEC520/security/f/" + String(f_id);
}

String cloudTopicSystemState()                  { return _CLOUD_BASE()+"s/st"; }
String cloudTopicKeypad()                       { return _CLOUD_BASE()+"s/ke"; }
String cloudTopicNetwork()                      { return _CLOUD_BASE()+"n/st"; }
String cloudTopicMac()                          { return _CLOUD_BASE()+"n/mc"; }
String cloudTopicFloorConnection(uint8_t f_id)  { return _CLOUD_BASE()+"f/"+String(f_id)+"/cs"; }
String cloudTopicRoomConnection(uint8_t f_id,uint8_t r_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/cs"; }
String cloudTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id); }
String cloudTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id); }
// NEW: floor RSSI
String cloudTopicFloorRssi(uint8_t f_id)       { return _CLOUD_BASE()+"f/"+String(f_id)+"/rsi"; }

// ---------------- Single-topic parsers (strict) ----------------
static bool parseCore(const char* topicC, const char* payloadC, bool cloud){
  if (!topicC || !payloadC) return false;

  String t = topicC;
  String p[12];
  int n = splitTopic(t, p, 12);
  if (n <= 0) return false;

  int base = 0;
  if (cloud){
    if (n < 3 || p[0]!="ELEC520" || p[1]!="security") return false;
    base = 2;
  }

  // s/st, s/ke, n/st, n/mc
  if (n-base==2 && p[base]=="s" && p[base+1]=="st"){ uint8_t v; if(!parseByte(payloadC,v)) return false; return setSystemState(v); }
  if (n-base==2 && p[base]=="s" && p[base+1]=="ke"){ uint8_t v; if(!parseByte(payloadC,v)) return false; return setKeypad(v); }
  if (n-base==2 && p[base]=="n" && p[base+1]=="st"){ uint8_t v; if(!parseByte(payloadC,v)) return false; return setNetwork(v); }
  if (n-base==2 && p[base]=="n" && p[base+1]=="mc"){ return setMac(String(payloadC)); }

  // f/{f}/cs
  if (n-base==3 && p[base]=="f" && p[base+2]=="cs"){
    uint8_t f = (uint8_t)p[base+1].toInt();
    bool b; if(!parseBool01(payloadC,b)) return false;
    return setFloorConnection(f,b);
  }

  // NEW: f/{f}/rsi
  if (n-base==3 && p[base]=="f" && p[base+2]=="rsi"){
    uint8_t f = (uint8_t)p[base+1].toInt();
    uint8_t v; if(!parseByte(payloadC,v)) return false;
    return setFloorRssi(f, v);
  }

  // f/{f}/r/{r}/cs | u/{u} | h/{h}
  if (n-base==5 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="cs"){
    uint8_t f=(uint8_t)p[base+1].toInt(), r=(uint8_t)p[base+3].toInt();
    bool b; if(!parseBool01(payloadC,b)) return false;
    return setRoomConnection(f,r,b);
  }
  if (n-base==6 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="u"){
    uint8_t f=(uint8_t)p[base+1].toInt(), r=(uint8_t)p[base+3].toInt(), u=(uint8_t)p[base+5].toInt();
    uint8_t v; if(!parseByte(payloadC,v)) return false;
    return setUltraValue(f,r,u,v);
  }
  if (n-base==6 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="h"){
    uint8_t f=(uint8_t)p[base+1].toInt(), r=(uint8_t)p[base+3].toInt(), h=(uint8_t)p[base+5].toInt();
    bool b; if(!parseBool01(payloadC,b)) return false;
    return setHallOpen(f,r,h,b);
  }

  return false;
}

bool parseNode (const char* topic, const char* rawPayload){ return parseCore(topic, rawPayload, false); }
bool parseCloud(const char* topic, const char* rawPayload){ return parseCore(topic, rawPayload, true ); }

// ================= RSSI extraction helpers (non-breaking) =================
bool extractFloorRssiFromTopicPayload(const char* topic,
                                      const char* payload,
                                      uint8_t& out_f_id,
                                      uint8_t& out_rssi)
{
  if (!topic || !payload) return false;

  String t = topic;
  String parts[8];
  int n = splitTopic(t, parts, 8);
  if (n <= 0) return false;

  int base = 0;
  if (n >= 3 && isCloudPrefix(parts[0], parts[1])) base = 2;

  // Expect: f/<f_id>/rsi
  if (n - base != 3) return false;
  if (parts[base] != "f" || parts[base + 2] != "rsi") return false;

  long f = parts[base + 1].toInt();
  if (f < 0 || f > 255) return false;

  uint8_t rssiVal;
  if (!parseByte(payload, rssiVal)) return false;

  out_f_id = (uint8_t)f;
  out_rssi = rssiVal;
  return true;
}

bool extractFloorRssiFromSingle(const String& line,
                                uint8_t& out_f_id,
                                uint8_t& out_rssi)
{
  if (line.length() == 0) return false;

  // Your format: "f/<f_id>/rsi:<val>"
  // Also accept space or '=' as separator.
  int sp = line.indexOf(' ');
  if (sp < 0) sp = line.indexOf('=');
  String topic, payload;
  if (sp > 0) {
    topic   = line.substring(0, sp);
    payload = line.substring(sp + 1);
  } else {
    int cp = line.indexOf(':');
    if (cp < 0) return false;
    topic   = line.substring(0, cp);
    payload = line.substring(cp + 1);
  }

  topic.trim(); payload.trim();
  if (topic.length() == 0 || payload.length() == 0) return false;

  return extractFloorRssiFromTopicPayload(topic.c_str(),
                                          payload.c_str(),
                                          out_f_id,
                                          out_rssi);
}

// ================= Room ESP compact format (build + parse) =================
//
// Format:
//   f/<f_id>/r/<r_id>/cs:<0|1>;u/<u_id>:<0..255>;h/<hs_id>:<0|1>;...
//

String buildRoomEspString(uint8_t f_id, uint8_t r_id) {
  // Ensure floor/room exist
  addRoom(f_id, r_id);

  String msg;
  msg.reserve(64);

  // Base header with connection state
  msg  = "f/"; msg += String(f_id);
  msg += "/r/"; msg += String(r_id);
  msg += "/cs:";
  msg += MODEL.floors[f_id].rooms[r_id].connected ? "1" : "0";

  // Ultrasonic sensors
  for (uint8_t u = 0; u < SMP_MAX_SENSORS; u++) {
    if (MODEL.floors[f_id].rooms[r_id].ultra[u].used) {
      msg += ";u/"; msg += String(u);
      msg += ":";   msg += String(MODEL.floors[f_id].rooms[r_id].ultra[u].value);
    }
  }

  // Hall sensors
  for (uint8_t h = 0; h < SMP_MAX_SENSORS; h++) {
    if (MODEL.floors[f_id].rooms[r_id].hall[h].used) {
      msg += ";h/"; msg += String(h);
      msg += ":";   msg += (MODEL.floors[f_id].rooms[r_id].hall[h].open ? "1" : "0");
    }
  }

  return msg;
}

static inline void _trimInPlace(String& s) { s.trim(); }

bool parseRoomEspString(const String& roomData) {
  // Example:
  //   "f/0/r/2/cs:1;u/0:87;h/1:0"
  if (roomData.length() == 0) return false;

  // Tokenize on ';'
  String tokens[40];
  int count = 0, start = 0;
  while (count < 40) {
    int idx = roomData.indexOf(';', start);
    String part = (idx == -1) ? roomData.substring(start)
                              : roomData.substring(start, idx);
    _trimInPlace(part);
    if (part.length() > 0) tokens[count++] = part;
    if (idx == -1) break;
    start = idx + 1;
  }
  if (count == 0) return false;

  // First pass: find f_id and r_id from a token that contains "f/.../r/..."
  bool gotFR = false;
  uint8_t f_id = 0, r_id = 0;

  auto parseFR = [&](const String& t)->bool {
    int fpos = t.indexOf("f/");
    if (fpos < 0) return false;
    int rpos = t.indexOf("/r/", fpos + 2);
    if (rpos < 0) return false;

    String fstr = t.substring(fpos + 2, rpos);

    int endR = t.indexOf('/', rpos + 3);
    int endC = t.indexOf(':', rpos + 3);
    int end   = t.length();
    if (endR >= 0) end = min(end, endR);
    if (endC >= 0) end = min(end, endC);

    String rstr = t.substring(rpos + 3, end);

    long f = fstr.toInt();
    long r = rstr.toInt();
    if (f < 0 || f > 255 || r < 0 || r > 255) return false;
    f_id = (uint8_t)f;
    r_id = (uint8_t)r;
    return true;
  };

  for (int i = 0; i < count && !gotFR; i++) gotFR = parseFR(tokens[i]);
  if (!gotFR) return false;

  // Ensure model nodes exist
  addRoom(f_id, r_id);

  // Second pass: process tokens
  for (int i = 0; i < count; i++) {
    const String& t = tokens[i];

    // header "f/<f>/r/<r>/cs:<v>" ?
    int csi = t.indexOf("cs:");
    if (csi >= 0) {
      String v = t.substring(csi + 3);
      v.trim();
      bool cs = (v.toInt() != 0);
      setRoomConnection(f_id, r_id, cs);
      // keep parsing other items
    }

    if (t.startsWith("cs:")) {
      bool cs = (t.substring(3).toInt() != 0);
      setRoomConnection(f_id, r_id, cs);
      continue;
    }

    if (t.startsWith("u/")) {
      int colon = t.indexOf(':');
      if (colon > 2) {
        uint8_t u_id = (uint8_t)t.substring(2, colon).toInt();
        uint8_t val  = (uint8_t)t.substring(colon + 1).toInt();
        setUltraValue(f_id, r_id, u_id, val);
      }
      continue;
    }

    if (t.startsWith("h/")) {
      int colon = t.indexOf(':');
      if (colon > 2) {
        uint8_t hs_id = (uint8_t)t.substring(2, colon).toInt();
        bool open01   = (t.substring(colon + 1).toInt() != 0);
        setHallOpen(f_id, r_id, hs_id, open01);
      }
      continue;
    }
  }

  return true;
}

// ---------------- MQTT full-system compact string (simple keep-alive versions) ----------------
// NOTE: Signatures are preserved to avoid breaking existing callers. These minimal
// implementations keep linkage intact; extend as needed for your full compact format.
String buildSystemMqttString() {
  // Example minimal payload; adjust to your original compact schema if needed.
  // We purposefully keep this very small to avoid changing any external expectations.
  return String();
}


// ---------------------------------------------------------------------------
// Parse MQTT system data string (semicolon-separated)
// Each token is topic-like and uses ':' to separate topic and value
// Example:
//   "s/st:1;s/ke:0;n/st:1;n/mc:AA:BB:CC;f/0/cs:1;f/0/rsi:187;f/0/r/0/cs:1;f/0/r/0/u/0:90;f/0/r/0/h/0:1"
// ---------------------------------------------------------------------------
bool parseSystemMqttString(const String& systemData) {
    if (systemData.length() == 0) return false;

    // Split on ';' into tokens
    String tokens[256];
    int count = 0, start = 0;
    while (count < 256) {
        int idx = systemData.indexOf(';', start);
        String part = (idx == -1) ? systemData.substring(start)
                                  : systemData.substring(start, idx);
        part.trim();
        if (part.length() > 0) tokens[count++] = part;
        if (idx == -1) break;
        start = idx + 1;
    }

    bool anyParsed = false;

    // Process each token
    for (int i = 0; i < count; i++) {
        const String& token = tokens[i];
        int colon = token.indexOf(':');
        if (colon <= 0) continue;

        String topic = token.substring(0, colon);
        String payload = token.substring(colon + 1);

        topic.trim();
        payload.trim();

        // Use the same internal logic that parses MQTT or ESP-NOW messages
        if (parseNode(topic.c_str(), payload.c_str())) {
            anyParsed = true;
        }
    }

    return anyParsed;
}



// ---------------------------------------------------------------------------
// Build ESP-NOW message for floor RSSI
// Format: f/<f_id>/rsi:<value>
// ---------------------------------------------------------------------------
String buildFloorRssiEspString(uint8_t f_id) {
    if (!addFloor(f_id)) return "";

    String msg;
    msg.reserve(20);
    msg = "f/";
    msg += String(f_id);
    msg += "/rsi:";
    msg += String(MODEL.floors[f_id].rssi);

    return msg;
}


// Build MQTT floor. 
String buildFloorMqttString(uint8_t f_id) {
    if (!MODEL.floors[f_id].used) return "";
    String msg;
    msg.reserve(512);

    // Floor connection and RSSI (no redundant "f/<f_id>/")
    msg += "cs:";
    msg += (MODEL.floors[f_id].connected ? "1" : "0");
    msg += ";rsi:";
    msg += String(MODEL.floors[f_id].rssi);

    // Each room on this floor
    for (uint8_t r = 0; r < SMP_MAX_ROOMS; r++) {
        if (!MODEL.floors[f_id].rooms[r].used) continue;

        // Build compact per-room string (but remove floor prefix)
        String roomPart = buildRoomEspString(f_id, r);

        // Remove redundant "f/<f_id>/" prefix if present
        String prefix = "f/" + String(f_id) + "/";
        if (roomPart.startsWith(prefix)) {
            roomPart.remove(0, prefix.length());
        }

        if (roomPart.length() > 0) {
            msg += ";";
            msg += roomPart;
        }
    }

    return msg;
}

