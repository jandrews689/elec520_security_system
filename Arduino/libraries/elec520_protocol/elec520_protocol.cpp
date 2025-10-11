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

// ---------------- ESP-NOW: per-room compact string ----------------
// (unchanged)
// ... (no changes below this line for bulk formats)

// -------- RSSI extraction helpers --------
static bool _isCloudPrefix(const String& p0, const String& p1) {
  return (p0 == "ELEC520" && p1 == "security");
}

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
  if (n >= 3 && _isCloudPrefix(parts[0], parts[1])) {
    base = 2; // skip "ELEC520/security"
  }

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

  // Try split by space first: "topic payload"
  int sp = line.indexOf(' ');
  if (sp < 0) sp = line.indexOf('=');  // also allow '='
  String topic, payload;
  if (sp > 0) {
    topic   = line.substring(0, sp);
    payload = line.substring(sp + 1);
  } else {
    // Try colon style: "topic:payload"
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