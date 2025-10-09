#include "elec520_protocol.h"

// ---------- Global ----------
ProtocolModel MODEL;

// ---------- Internal helpers ----------
static int splitTopic(const String& t, String out[], int maxParts=12) {
  int n=0, start=0, i;
  while ((i=t.indexOf('/', start))>=0 && n<maxParts) { out[n++]=t.substring(start,i); start=i+1; }
  if (start <= (int)t.length() && n<maxParts) out[n++]=t.substring(start);
  return n;
}
static bool parseBoolStrict(const char* s, bool& out) {
  if (!s) return false;
  if (strcmp(s,"true")==0)  { out=true;  return true; }
  if (strcmp(s,"false")==0) { out=false; return true; }
  return false;
}
static bool parseByte(const char* s, uint8_t& out) {
  if (!s || !*s) return false;
  long v=strtol(s,nullptr,10); if (v<0 || v>255) return false; out=(uint8_t)v; return true;
}

static FloorNode* findFloor(uint8_t f_id) {
  for (int i=0;i<SMP_MAX_FLOORS;i++) if (MODEL.floors[i].used && MODEL.floors[i].floorId==f_id) return &MODEL.floors[i];
  return nullptr;
}
static RoomNode* findRoom(FloorNode* f, uint8_t r_id) {
  if (!f) return nullptr;
  for (int i=0;i<SMP_MAX_ROOMS;i++) if (f->rooms[i].used && f->rooms[i].roomId==r_id) return &f->rooms[i];
  return nullptr;
}
static UltraSensor* findUltra(RoomNode* r, uint8_t u_id) {
  if (!r) return nullptr;
  for (int i=0;i<SMP_MAX_SENSORS;i++) if (r->ultra[i].used && r->ultra[i].id==u_id) return &r->ultra[i];
  return nullptr;
}
static HallSensor* findHall(RoomNode* r, uint8_t hs_id) {
  if (!r) return nullptr;
  for (int i=0;i<SMP_MAX_SENSORS;i++) if (r->hall[i].used && r->hall[i].id==hs_id) return &r->hall[i];
  return nullptr;
}

// ---------- Adders ----------
bool addFloor(uint8_t f_id) {
  if (findFloor(f_id)) return true;
  for (int i=0;i<SMP_MAX_FLOORS;i++) if (!MODEL.floors[i].used) {
    MODEL.floors[i].used=true; MODEL.floors[i].floorId=f_id; MODEL.floors[i].connected=false; MODEL.floors[i].idByte=0; return true;
  }
  return false;
}
bool addRoom(uint8_t f_id, uint8_t r_id) {
  if (!addFloor(f_id)) return false;
  FloorNode* f=findFloor(f_id); if (!f) return false;
  if (findRoom(f, r_id)) return true;
  for (int i=0;i<SMP_MAX_ROOMS;i++) if (!f->rooms[i].used) {
    f->rooms[i].used=true; f->rooms[i].roomId=r_id; f->rooms[i].connected=false; f->rooms[i].idByte=0; return true;
  }
  return false;
}
bool addUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id) {
  if (!addRoom(f_id,r_id)) return false;
  RoomNode* r=findRoom(findFloor(f_id), r_id); if (!r) return false;
  if (findUltra(r,u_id)) return true;
  for (int i=0;i<SMP_MAX_SENSORS;i++) if (!r->ultra[i].used) {
    r->ultra[i].used=true; r->ultra[i].id=u_id; r->ultra[i].value=0; return true;
  }
  return false;
}
bool addHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id) {
  if (!addRoom(f_id,r_id)) return false;
  RoomNode* r=findRoom(findFloor(f_id), r_id); if (!r) return false;
  if (findHall(r,hs_id)) return true;
  for (int i=0;i<SMP_MAX_SENSORS;i++) if (!r->hall[i].used) {
    r->hall[i].used=true; r->hall[i].id=hs_id; r->hall[i].open=false; return true;
  }
  return false;
}

// ---------- Setters ----------
bool setSystemState(uint8_t s)          { MODEL.systemState=s; return true; }
bool setKeypad(uint8_t k)               { MODEL.keypad=k;      return true; }
bool setNetwork(uint8_t n)              { MODEL.network=n;     return true; }
bool setMac(const String& mac)          { MODEL.mac=mac;       return true; }

bool setFloorIdByte(uint8_t f_id, uint8_t idByte) { addFloor(f_id); FloorNode* f=findFloor(f_id); if(!f) return false; f->idByte=idByte; return true; }
bool setFloorConnection(uint8_t f_id, bool cs)    { addFloor(f_id); FloorNode* f=findFloor(f_id); if(!f) return false; f->connected=cs; return true; }
bool setRoomIdByte(uint8_t f_id, uint8_t r_id, uint8_t idByte) { addRoom(f_id,r_id); RoomNode* r=findRoom(findFloor(f_id), r_id); if(!r) return false; r->idByte=idByte; return true; }
bool setRoomConnection(uint8_t f_id, uint8_t r_id, bool cs)    { addRoom(f_id,r_id); RoomNode* r=findRoom(findFloor(f_id), r_id); if(!r) return false; r->connected=cs; return true; }
bool setUltraValue(uint8_t f_id, uint8_t r_id, uint8_t u_id, uint8_t val) { addUltra(f_id,r_id,u_id); UltraSensor* u=findUltra(findRoom(findFloor(f_id), r_id), u_id); if(!u) return false; u->value=val; return true; }
bool setHallOpen(uint8_t f_id, uint8_t r_id, uint8_t hs_id, bool open)    { addHall(f_id,r_id,hs_id); HallSensor* h=findHall(findRoom(findFloor(f_id), r_id), hs_id); if(!h) return false; h->open=open; return true; }

void resetModel(){ MODEL = ProtocolModel(); }

// ---------- Topic builders (Node) ----------
String nodeTopicSystemState()                         { return "s/st"; }
String nodeTopicKeypad()                              { return "s/ke"; }
String nodeTopicNetwork()                             { return "n/st"; }
String nodeTopicMac()                                 { return "n/mc"; }
String nodeTopicFloorIdByte(uint8_t f_id)             { return "f/"+String(f_id)+"/id"; }
String nodeTopicFloorConnection(uint8_t f_id)         { return "f/"+String(f_id)+"/cs"; }
String nodeTopicRoomIdByte(uint8_t f_id,uint8_t r_id) { return "f/"+String(f_id)+"/r/"+String(r_id)+"/id"; }
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/cs"; }
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id); }
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id); }

// ---------- Topic builders (Cloud) ----------
static inline String _CLOUD_BASE(){ return "ELEC520/security/"; }
String cloudTopicSystemState()                         { return _CLOUD_BASE()+"s/st"; }
String cloudTopicKeypad()                              { return _CLOUD_BASE()+"s/ke"; }
String cloudTopicNetwork()                             { return _CLOUD_BASE()+"n/st"; }
String cloudTopicMac()                                 { return _CLOUD_BASE()+"n/mc"; }
String cloudTopicFloorIdByte(uint8_t f_id)             { return _CLOUD_BASE()+"f/"+String(f_id)+"/id"; }
String cloudTopicFloorConnection(uint8_t f_id)         { return _CLOUD_BASE()+"f/"+String(f_id)+"/cs"; }
String cloudTopicRoomIdByte(uint8_t f_id,uint8_t r_id) { return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/id"; }
String cloudTopicRoomConnection(uint8_t f_id,uint8_t r_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/cs"; }
String cloudTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id); }
String cloudTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return _CLOUD_BASE()+"f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id); }

// ---------- Parsers for single topic/value ----------
static bool parseCore(const char* topicC, const char* payloadC, bool cloud) {
  if (!topicC || !payloadC) return false;

  String topic = topicC;
  String parts[12];
  int n = splitTopic(topic, parts, 12);
  if (n <= 0) return false;

  int base = 0;
  if (cloud) {
    if (n < 3 || parts[0]!="ELEC520" || parts[1]!="security") return false;
    base = 2;
  }

  // s/st, s/ke, n/st, n/mc
  if (n-base==2 && parts[base]=="s" && parts[base+1]=="st") { uint8_t v; if(!parseByte(payloadC,v)) return false; return setSystemState(v); }
  if (n-base==2 && parts[base]=="s" && parts[base+1]=="ke") { uint8_t v; if(!parseByte(payloadC,v)) return false; return setKeypad(v); }
  if (n-base==2 && parts[base]=="n" && parts[base+1]=="st") { uint8_t v; if(!parseByte(payloadC,v)) return false; return setNetwork(v); }
  if (n-base==2 && parts[base]=="n" && parts[base+1]=="mc") { return setMac(String(payloadC)); }

  // f/{f}/id  | f/{f}/cs
  if (n-base==3 && parts[base]=="f" && parts[base+2]=="id") { uint8_t f=(uint8_t)parts[base+1].toInt(); uint8_t v; if(!parseByte(payloadC,v)) return false; return setFloorIdByte(f,v); }
  if (n-base==3 && parts[base]=="f" && parts[base+2]=="cs") { uint8_t f=(uint8_t)parts[base+1].toInt(); bool b; if(!parseBoolStrict(payloadC,b)) return false; return setFloorConnection(f,b); }

  // f/{f}/r/{r}/id | cs | u/{u} | h/{h}
  if (n-base==5 && parts[base]=="f" && parts[base+2]=="r" && parts[base+4]=="id") { uint8_t f=(uint8_t)parts[base+1].toInt(); uint8_t r=(uint8_t)parts[base+3].toInt(); uint8_t v; if(!parseByte(payloadC,v)) return false; return setRoomIdByte(f,r,v); }
  if (n-base==5 && parts[base]=="f" && parts[base+2]=="r" && parts[base+4]=="cs") { uint8_t f=(uint8_t)parts[base+1].toInt(); uint8_t r=(uint8_t)parts[base+3].toInt(); bool b; if(!parseBoolStrict(payloadC,b)) return false; return setRoomConnection(f,r,b); }
  if (n-base==6 && parts[base]=="f" && parts[base+2]=="r" && parts[base+4]=="u")  { uint8_t f=(uint8_t)parts[base+1].toInt(); uint8_t r=(uint8_t)parts[base+3].toInt(); uint8_t u=(uint8_t)parts[base+5].toInt(); uint8_t v; if(!parseByte(payloadC,v)) return false; return setUltraValue(f,r,u,v); }
  if (n-base==6 && parts[base]=="f" && parts[base+2]=="r" && parts[base+4]=="h")  { uint8_t f=(uint8_t)parts[base+1].toInt(); uint8_t r=(uint8_t)parts[base+3].toInt(); uint8_t h=(uint8_t)parts[base+5].toInt(); bool b; if(!parseBoolStrict(payloadC,b)) return false; return setHallOpen(f,r,h,b); }

  return false;
}

bool parseNode (const char* topic, const char* rawPayload) { return parseCore(topic, rawPayload, false); }
bool parseCloud(const char* topic, const char* rawPayload) { return parseCore(topic, rawPayload, true ); }

// ---------- FLOOR (ESP-NOW) COMPACT STRING ----------
// Format: f/<f>/r/<r>/h/<h>:true;/r/<r>/u/<u>:128;...
// First item for a floor starts with "f/<f>", subsequent items start with "/r/...".
String buildFloorEspString(uint8_t floorId) {
  FloorNode* f = findFloor(floorId);
  if (!f || !f->used) return "";

  String out;

  // Optional: include floor ID bitfield & connection first
  out += "f/" + String(floorId) + "/id:" + String(f->idByte) + ";";
  out += "/cs:" + String(f->connected ? "true" : "false") + ";";

  bool firstInRoomSet = false;

  for (int j=0; j<SMP_MAX_ROOMS; j++) {
    if (!f->rooms[j].used) continue;
    RoomNode* r = &f->rooms[j];

    // room id/conn
    out += "/r/" + String(r->roomId) + "/id:" + String(r->idByte) + ";";
    out += "/r/" + String(r->roomId) + "/cs:" + String(r->connected ? "true" : "false") + ";";

    // ultra
    for (int k=0; k<SMP_MAX_SENSORS; k++) {
      if (!r->ultra[k].used) continue;
      out += "/r/" + String(r->roomId) + "/u/" + String(r->ultra[k].id) + ":" + String(r->ultra[k].value) + ";";
    }
    // hall
    for (int k=0; k<SMP_MAX_SENSORS; k++) {
      if (!r->hall[k].used) continue;
      out += "/r/" + String(r->roomId) + "/h/" + String(r->hall[k].id) + ":" + String(r->hall[k].open ? "true" : "false") + ";";
    }
  }

  // collapse multiple leading "/..." into first item "f/<f>/..."
  // already done by starting with f/<f> at the very first pair
  // remove trailing ';'
  if (out.endsWith(";")) out.remove(out.length()-1);
  return out;
}

// Parse a single floor ESP string into MODEL
// Accepts either: "f/1/..." (recommended) OR starts with "/r/..." if caller prefixed floor previously
bool parseFloorEspString(const String& floorData) {
  if (floorData.length()==0) return false;

  // Extract floor id from first token that looks like f/<num>
  int fpos = floorData.indexOf("f/");
  if (fpos != 0) {
    // Allow strings that start with "/r/..." only if the caller sets current floor elsewhere
    // For robustness, require "f/<id>" at start:
    return false;
  }
  int slash = floorData.indexOf('/', 2);
  String fIdStr;
  if (slash < 0) {
    // maybe "f/1:id:5;..." -> read number after "f/"
    int colon = floorData.indexOf(':', 2);
    if (colon < 0) return false;
    fIdStr = floorData.substring(2, colon);
  } else {
    fIdStr = floorData.substring(2, slash);
  }
  uint8_t f_id = (uint8_t)fIdStr.toInt();
  if (!addFloor(f_id)) addFloor(f_id); // ensure exists

  // Walk pairs "topic:payload" separated by ';'
  int start = 0;
  while (start < floorData.length()) {
    int semi = floorData.indexOf(';', start);
    String pair = (semi<0) ? floorData.substring(start) : floorData.substring(start, semi);
    if (pair.length()==0) break;

    int colon = pair.indexOf(':');
    if (colon > 0) {
      String topic = pair.substring(0, colon);
      String payload = pair.substring(colon+1);

      // topic can start with "f/<f>..." or be relative like "/r/..."
      if (topic.startsWith("/")) {
        topic = "f/" + String(f_id) + topic;  // prefix current floor
      }
      parseNode(topic.c_str(), payload.c_str());
    }

    if (semi < 0) break;
    start = semi + 1;
  }
  return true;
}

// ---------- SYSTEM (MQTT) COMPACT STRING ----------
// Single message for all floors, with ONE base prefix only once.
// Example:
// ELEC520/security/f/1/id:5;/cs:true;/r/101/id:7;/r/101/u/0:128;/f/2/id:1;/r/5/h/0:false;
String buildSystemMqttString() {
  String out = _CLOUD_BASE(); // "ELEC520/security/"

  bool firstFloorEmitted = false;
  for (int i=0; i<SMP_MAX_FLOORS; i++) {
    if (!MODEL.floors[i].used) continue;
    FloorNode* f = &MODEL.floors[i];

    // floor header & basics
    if (!firstFloorEmitted) { out += "f/" + String(f->floorId); firstFloorEmitted = true; }
    else { out += "/f/" + String(f->floorId); }

    out += "/id:" + String(f->idByte) + ";";
    out += "/cs:" + String(f->connected ? "true" : "false") + ";";

    // rooms & sensors
    for (int j=0; j<SMP_MAX_ROOMS; j++) {
      if (!f->rooms[j].used) continue;
      RoomNode* r = &f->rooms[j];

      out += "/r/" + String(r->roomId) + "/id:" + String(r->idByte) + ";";
      out += "/r/" + String(r->roomId) + "/cs:" + String(r->connected ? "true" : "false") + ";";

      for (int k=0; k<SMP_MAX_SENSORS; k++) {
        if (r->ultra[k].used)
          out += "/r/" + String(r->roomId) + "/u/" + String(r->ultra[k].id) + ":" + String(r->ultra[k].value) + ";";
      }
      for (int k=0; k<SMP_MAX_SENSORS; k++) {
        if (r->hall[k].used)
          out += "/r/" + String(r->roomId) + "/h/" + String(r->hall[k].id) + ":" + String(r->hall[k].open ? "true" : "false") + ";";
      }
    }
  }
  // Trim trailing ';' if present
  if (out.endsWith(";")) out.remove(out.length()-1);
  return out;
}

// Parse full system MQTT compact string back into MODEL
// Accepts one base prefix "ELEC520/security/" then multiple floors with relative paths.
bool parseSystemMqttString(const String& systemData) {
  if (systemData.length()==0) return false;
  const String base = _CLOUD_BASE();

  if (!systemData.startsWith(base)) return false;
  // Strip base, process remainder with floor-relative logic
  String rest = systemData.substring(base.length()); // e.g. "f/1/id:5;/cs:true;/r/101/u/0:128;/f/2/..."

  int current_f = -1;
  int start = 0;
  while (start < (int)rest.length()) {
    int semi = rest.indexOf(';', start);
    String pair = (semi<0) ? rest.substring(start) : rest.substring(start, semi);
    if (pair.length()==0) break;

    int colon = pair.indexOf(':');
    if (colon > 0) {
      String topic = pair.substring(0, colon);     // e.g. "f/1/id" OR "/r/101/u/0"
      String payload = pair.substring(colon+1);

      // If this is a new floor header ("f/<n>/..."), update current_f
      if (topic.startsWith("f/")) {
        int slash = topic.indexOf('/', 2);
        String fid = (slash>0) ? topic.substring(2, slash) : topic.substring(2);
        current_f = fid.toInt();
        // pass through as node-topic (no cloud header)
        parseNode(topic.c_str(), payload.c_str());
      } else if (topic.startsWith("/")) {
        // relative to current floor
        if (current_f < 0) return false;
        String fullTopic = "f/" + String(current_f) + topic;
        parseNode(fullTopic.c_str(), payload.c_str());
      } else {
        // unexpected token
        return false;
      }
    }

    if (semi < 0) break;
    start = semi + 1;
  }
  return true;
}
