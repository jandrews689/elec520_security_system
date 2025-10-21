#include "elec520_protocol.h"

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
static bool parseUint32(const char* s, uint32_t& out){
  if (!s || !*s) return false;
  unsigned long v=strtoul(s,nullptr,10);
  out = (uint32_t)v;
  return true;
}

// ---------------- Global ----------------
ProtocolModel MODEL;

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

// ---------------- System-level setters (kept) ----------------
bool setSystemState(uint8_t s) { MODEL.systemState = s; return true; }
bool setKeypad(uint8_t k)      { MODEL.keypad = k;     return true; }
bool setNetwork(uint8_t n)     { MODEL.network = n;    return true; }
bool setMac(const String& mac) { MODEL.mac = mac;      return true; }

void resetModel(){ MODEL = ProtocolModel(); }

// ---------------- Topic builders (Node) ----------------
String nodeTopicSystemState()                    { return "s/st"; }
String nodeTopicKeypad()                         { return "s/ke"; }
String nodeTopicNetwork()                        { return "n/st"; }
String nodeTopicMac()                            { return "n/mc"; }
String nodeTopicFloorConnection(uint8_t f_id)    { return "f/"+String(f_id)+"/cs"; }
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/cs"; }
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id); }
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id); }
String nodeTopicFloorTimestamp(uint8_t f_id)     { return "f/"+String(f_id)+"/ts"; }
String nodeTopicRoomTimestamp(uint8_t f_id,uint8_t r_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/ts"; }

// ---------------- Topic builders (Cloud) ----------------
static inline String _CLOUD_BASE(){ return "ELEC520/security/"; }

String cloudTopicFloor(uint8_t f_id) {
  return "ELEC520/security/f/" + String(f_id);
}
String cloudTopicSystemState()                    { return _CLOUD_BASE()+"s/st"; }
String cloudTopicKeypad()                         { return _CLOUD_BASE()+"s/ke"; }
String cloudTopicNetwork()                        { return _CLOUD_BASE()+"n/st"; }
String cloudTopicMac()                            { return _CLOUD_BASE()+"n/mc"; }
String cloudTopicFloorConnection(uint8_t f_id)    { return _CLOUD_BASE()+"f/"+String(f_id)+"/cs"; }
String cloudTopicRoomConnection(uint8_t f_id,uint8_t r_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/cs"; }
String cloudTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id); }
String cloudTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id); }
String cloudTopicFloorTimestamp(uint8_t f_id)     { return _CLOUD_BASE()+"f/"+String(f_id)+"/ts"; }
String cloudTopicRoomTimestamp(uint8_t f_id,uint8_t r_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/ts"; }

// ---------------- Parsers (INLINED MODEL UPDATES) ----------------
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
  if (n-base==2 && p[base]=="s" && p[base+1]=="st"){ uint8_t v; if(!parseByte(payloadC,v)) return false; MODEL.systemState=v; return true; }
  if (n-base==2 && p[base]=="s" && p[base+1]=="ke"){ uint8_t v; if(!parseByte(payloadC,v)) return false; MODEL.keypad=v;      return true; }
  if (n-base==2 && p[base]=="n" && p[base+1]=="st"){ uint8_t v; if(!parseByte(payloadC,v)) return false; MODEL.network=v;     return true; }
  if (n-base==2 && p[base]=="n" && p[base+1]=="mc"){ MODEL.mac = String(payloadC); return true; }

  // f/{f}/cs
  if (n-base==3 && p[base]=="f" && p[base+2]=="cs"){
    uint8_t f = (uint8_t)p[base+1].toInt();
    bool b; if(!parseBool01(payloadC,b)) return false;
    if (!addFloor(f)) return false;
    MODEL.floors[f].connected = b;
    return true;
  }

  // f/{f}/ts (Unix)
  if (n-base==3 && p[base]=="f" && p[base+2]=="ts"){
    uint8_t f = (uint8_t)p[base+1].toInt();
    uint32_t ts; if(!parseUint32(payloadC, ts)) return false;
    if (!addFloor(f)) return false;
    MODEL.floors[f].ts = ts;
    return true;
  }

  // f/{f}/r/{r}/cs | u/{u} | h/{h} | ts
  if (n-base==5 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="cs"){
    uint8_t f=(uint8_t)p[base+1].toInt(), r=(uint8_t)p[base+3].toInt();
    bool b; if(!parseBool01(payloadC,b)) return false;
    if (!addRoom(f,r)) return false;
    MODEL.floors[f].rooms[r].connected = b;
    return true;
  }
  if (n-base==6 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="u"){
    uint8_t f=(uint8_t)p[base+1].toInt(), r=(uint8_t)p[base+3].toInt(), u=(uint8_t)p[base+5].toInt();
    uint8_t v; if(!parseByte(payloadC,v)) return false;
    if (!addUltra(f,r,u)) return false;
    MODEL.floors[f].rooms[r].ultra[u].value = v;
    return true;
  }
  if (n-base==6 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="h"){
    uint8_t f=(uint8_t)p[base+1].toInt(), r=(uint8_t)p[base+3].toInt(), h=(uint8_t)p[base+5].toInt();
    bool b; if(!parseBool01(payloadC,b)) return false;
    if (!addHall(f,r,h)) return false;
    MODEL.floors[f].rooms[r].hall[h].open = b;
    return true;
  }
  if (n-base==6 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="ts"){
    uint8_t f=(uint8_t)p[base+1].toInt(), r=(uint8_t)p[base+3].toInt();
    uint32_t ts; if(!parseUint32(payloadC, ts)) return false;
    if (!addRoom(f,r)) return false;
    MODEL.floors[f].rooms[r].ts = ts;
    return true;
  }

  return false;
}

bool parseNode (const char* topic, const char* rawPayload){ return parseCore(topic, rawPayload, false); }
bool parseCloud(const char* topic, const char* rawPayload){ return parseCore(topic, rawPayload, true ); }

// ================= Room ESP compact format (build + parse) =================
String buildRoomEspString(uint8_t f_id, uint8_t r_id) {
  addRoom(f_id, r_id);

  String msg; msg.reserve(64);
  msg  = "f/"; msg += String(f_id);
  msg += "/r/"; msg += String(r_id);
  msg += "/cs:"; msg += (MODEL.floors[f_id].rooms[r_id].connected ? "1" : "0");

  for (uint8_t u = 0; u < SMP_MAX_SENSORS; u++) {
    if (MODEL.floors[f_id].rooms[r_id].ultra[u].used) {
      msg += ";u/"; msg += String(u);
      msg += ":";   msg += String(MODEL.floors[f_id].rooms[r_id].ultra[u].value);
    }
  }
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

  // Find f_id and r_id
  bool gotFR = false; uint8_t f_id = 0, r_id = 0;
  auto parseFR = [&](const String& t)->bool {
    int fpos = t.indexOf("f/"); if (fpos < 0) return false;
    int rpos = t.indexOf("/r/", fpos + 2); if (rpos < 0) return false;
    String fstr = t.substring(fpos + 2, rpos);
    int endR = t.indexOf('/', rpos + 3);
    int endC = t.indexOf(':', rpos + 3);
    int end   = t.length();
    if (endR >= 0) end = min(end, endR);
    if (endC >= 0) end = min(end, endC);
    String rstr = t.substring(rpos + 3, end);
    long f = fstr.toInt(), r = rstr.toInt();
    if (f<0 || f>255 || r<0 || r>255) return false;
    f_id = (uint8_t)f; r_id = (uint8_t)r; return true;
  };
  for (int i = 0; i < count && !gotFR; i++) gotFR = parseFR(tokens[i]);
  if (!gotFR) return false;

  // Ensure existence
  addRoom(f_id, r_id);

  // Apply tokens
  for (int i = 0; i < count; i++) {
    const String& t = tokens[i];

    // header "f/<f>/r/<r>/cs:<v>" OR "cs:<v>"
    int csi = t.indexOf("cs:");
    if (csi >= 0) {
      bool cs = (t.substring(csi + 3).toInt() != 0);
      MODEL.floors[f_id].rooms[r_id].connected = cs;
      continue;
    }

    if (t.startsWith("u/")) {
      int colon = t.indexOf(':');
      if (colon > 2) {
        uint8_t u_id = (uint8_t)t.substring(2, colon).toInt();
        uint8_t val  = (uint8_t)t.substring(colon + 1).toInt();
        if (addUltra(f_id, r_id, u_id)) {
          MODEL.floors[f_id].rooms[r_id].ultra[u_id].value = val;
        }
      }
      continue;
    }

    if (t.startsWith("h/")) {
      int colon = t.indexOf(':');
      if (colon > 2) {
        uint8_t hs_id = (uint8_t)t.substring(2, colon).toInt();
        bool open01   = (t.substring(colon + 1).toInt() != 0);
        if (addHall(f_id, r_id, hs_id)) {
          MODEL.floors[f_id].rooms[r_id].hall[hs_id].open = open01;
        }
      }
      continue;
    }
  }

  return true;
}

// ---------------- MQTT full-system compact string ----------------
String buildSystemMqttString() { return String(); }

// ---------------- Parse MQTT full-system compact string ----------------
bool parseSystemMqttString(const String& systemData) {
  if (systemData.length() == 0) return false;

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
  for (int i = 0; i < count; i++) {
    const String& token = tokens[i];
    int colon = token.indexOf(':');
    if (colon <= 0) continue;

    String topic = token.substring(0, colon);
    String payload = token.substring(colon + 1);

    topic.trim();
    payload.trim();

    if (parseNode(topic.c_str(), payload.c_str())) {
      anyParsed = true;
    }
  }

  return anyParsed;
}
