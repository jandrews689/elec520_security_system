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
// Format:  f/<f>/r/<r>/cs:<0|1>;u/<u0>:<0..255>;u/<u1>:<...>;h/<h0>:<0|1>;...
String buildRoomEspString(uint8_t f_id, uint8_t r_id){
  if (!inRange(f_id,SMP_MAX_FLOORS) || !inRange(r_id,SMP_MAX_ROOMS)) return "";
  FloorNode& f = MODEL.floors[f_id];
  if (!f.used) return "";
  RoomNode&  r = f.rooms[r_id];
  if (!r.used) return "";

  String out = "f/"; out += f_id; out += "/r/"; out += r_id;

  // room connection first
  out += "/cs:"; out += (r.connected ? "1" : "0"); out += ";";

  // ultrasonic (only used)
  for (uint8_t u=0; u<SMP_MAX_SENSORS; ++u){
    if (!r.ultra[u].used) continue;
    out += "u/"; out += u; out += ":"; out += r.ultra[u].value; out += ";";
  }
  // hall (only used)
  for (uint8_t h=0; h<SMP_MAX_SENSORS; ++h){
    if (!r.hall[h].used) continue;
    out += "h/"; out += h; out += ":"; out += (r.hall[h].open ? "1" : "0"); out += ";";
  }

  if (out.endsWith(";")) out.remove(out.length()-1);
  // out += '\0';
  return out;
}

// Parse: f/<f>/r/<r>/cs:0;u/0:128;h/1:1;...
bool parseRoomEspString(const String& roomData){
  if (roomData.length()==0) return false;

  // Expect header "f/<f>/r/<r>"
  if (!roomData.startsWith("f/")) return false;
  int pos = 2;
  int slash = roomData.indexOf('/', pos);
  if (slash < 0) return false;
  uint8_t f_id = (uint8_t)roomData.substring(pos, slash).toInt();

  if (roomData.indexOf("/r/", slash) != slash) return false;
  pos = slash + 3;
  slash = roomData.indexOf('/', pos);
  uint8_t r_id;
  int afterHeader;
  if (slash < 0){
    // rare: header without trailing path
    int colon = roomData.indexOf(':', pos);
    if (colon < 0) return false;
    r_id = (uint8_t)roomData.substring(pos, colon).toInt();
    afterHeader = colon; // start of key:value
  } else {
    r_id = (uint8_t)roomData.substring(pos, slash).toInt();
    afterHeader = slash; // first key/value begins here
  }

  if (!addRoom(f_id, r_id)) return false;

  // Now iterate over key:value pairs separated by ';'
  int start = afterHeader + 1; // skip '/'
  while (start < (int)roomData.length()){
    int semi  = roomData.indexOf(';', start);
    String kv = (semi<0) ? roomData.substring(start) : roomData.substring(start, semi);
    if (kv.length()==0) break;

    int colon = kv.indexOf(':');
    if (colon > 0){
      String key = kv.substring(0, colon);
      String val = kv.substring(colon+1);

      if (key == "cs"){
        bool b; if (!parseBool01(val.c_str(), b)) return false;
        setRoomConnection(f_id, r_id, b);
      } else if (key.startsWith("u/")){
        uint8_t u_id = (uint8_t)key.substring(2).toInt();
        uint8_t v; if (!parseByte(val.c_str(), v)) return false;
        setUltraValue(f_id, r_id, u_id, v);
      } else if (key.startsWith("h/")){
        uint8_t h_id = (uint8_t)key.substring(2).toInt();
        bool b; if (!parseBool01(val.c_str(), b)) return false;
        setHallOpen(f_id, r_id, h_id, b);
      } else {
        // unknown key -> ignore
      }
    }

    if (semi < 0) break;
    start = semi + 1;
  }

  return true;
}

// ---------------- MQTT: full-system compact string ----------------
// One prefix only: "ELEC520/security/" then floors/rooms using numeric payloads.
// Example:
// ELEC520/security/f/0/r/0/cs:1;u/0:128;h/0:1;/r/1/cs:0;h/0:0;/f/1/r/0/cs:1;...
String buildSystemMqttString(){
  String out = _CLOUD_BASE(); // "ELEC520/security/"
  bool firstFloor = true;

  for (uint8_t f=0; f<SMP_MAX_FLOORS; ++f){
    if (!MODEL.floors[f].used) continue;

    if (firstFloor){ out += "f/"; firstFloor=false; }
    else            { out += "/f/"; }
    out += f;

    // floor connection (optional—include if you want)
    out += "/cs:"; out += (MODEL.floors[f].connected ? "1" : "0"); out += ";";

    for (uint8_t r=0; r<SMP_MAX_ROOMS; ++r){
      if (!MODEL.floors[f].rooms[r].used) continue;
      RoomNode& rm = MODEL.floors[f].rooms[r];

      out += "/r/"; out += r; out += "/cs:"; out += (rm.connected ? "1" : "0"); out += ";";

      for (uint8_t u=0; u<SMP_MAX_SENSORS; ++u){
        if (!rm.ultra[u].used) continue;
        out += "u/"; out += u; out += ":"; out += rm.ultra[u].value; out += ";";
      }
      for (uint8_t h=0; h<SMP_MAX_SENSORS; ++h){
        if (!rm.hall[h].used) continue;
        out += "h/"; out += h; out += ":"; out += (rm.hall[h].open ? "1" : "0"); out += ";";
      }
    }
  }
  if (out.endsWith(";")) out.remove(out.length()-1);
  return out;
}

// Parse the compact system string back into MODEL
bool parseSystemMqttString(const String& systemData){
  const String base = _CLOUD_BASE();
  if (!systemData.startsWith(base)) return false;

  String s = systemData.substring(base.length()); // after "ELEC520/security/"
  int currentF = -1;
  int currentR = -1;

  // s contains sequences like: f/<f>/cs:1; /r/<r>/cs:1; u/0:128; h/0:1; /f/<f>/...
  int i = 0;
  while (i < (int)s.length()){
    // find next ';'
    int semi = s.indexOf(';', i);
    String token = (semi<0) ? s.substring(i) : s.substring(i, semi);
    token.trim();
    if (token.length()==0) break;

    // token could be "f/<f>" (header continuation) or "/f/<f>" or "/r/<r>" segment or key:value
    if (token.startsWith("f/") || token.startsWith("/f/")){
      int pos = token.startsWith("f/") ? 2 : 3;
      int nxt = token.indexOf('/', pos);
      String fstr = (nxt<0) ? token.substring(pos) : token.substring(pos, nxt);
      currentF = fstr.toInt();
      if (!addFloor((uint8_t)currentF)) return false;

      // If trailing "/cs:.." included in same token (rare), handle below when we parse key:val
      // Reset currentR when floor changes
      currentR = -1;
    } else if (token.startsWith("/r/")){
      int pos = 3;
      int nxt = token.indexOf('/', pos);
      String rstr = (nxt<0) ? token.substring(pos) : token.substring(pos, nxt);
      currentR = rstr.toInt();
      if (!addRoom((uint8_t)currentF, (uint8_t)currentR)) return false;

      // If trailing "/cs:.." in same token, also handled via key:val parsing
    } else {
      // key:value relative to current F/R
      int colon = token.indexOf(':');
      if (colon > 0){
        String key = token.substring(0, colon);
        String val = token.substring(colon+1);

        if (key == "cs"){
          // applies to last header: floor if currentR<0, else room
          bool b; if (!parseBool01(val.c_str(), b)) return false;
          if (currentR < 0) setFloorConnection((uint8_t)currentF, b);
          else              setRoomConnection((uint8_t)currentF, (uint8_t)currentR, b);
        } else if (key.startsWith("u/")){
          uint8_t u = (uint8_t)key.substring(2).toInt();
          uint8_t v; if (!parseByte(val.c_str(), v)) return false;
          setUltraValue((uint8_t)currentF, (uint8_t)currentR, u, v);
        } else if (key.startsWith("h/")){
          uint8_t h = (uint8_t)key.substring(2).toInt();
          bool b; if (!parseBool01(val.c_str(), b)) return false;
          setHallOpen((uint8_t)currentF, (uint8_t)currentR, h, b);
        } else {
          // unknown -> ignore
        }
      }
    }

    if (semi < 0) break;
    i = semi + 1;
  }

  return true;
}
