#include "elec520_protocol.h"

// ---------- Global ----------
ProtocolModel MODEL;

// ---------- Internal helpers ----------
static int splitTopic(const String& t, String out[], int maxParts=12) {
  int n=0, start=0, idx=0;
  while ((idx=t.indexOf('/', start))>=0 && n<maxParts) {
    out[n++] = t.substring(start, idx);
    start = idx+1;
  }
  if (start <= (int)t.length() && n<maxParts) out[n++] = t.substring(start);
  return n;
}

static bool parseBoolStrict(const char* s, bool& out) {
  if (!s) return false;
  if (strcmp(s, "true")==0)  { out = true;  return true; }
  if (strcmp(s, "false")==0) { out = false; return true; }
  return false; // strict: only exact "true"/"false"
}

static bool parseByte(const char* s, uint8_t& out) {
  if (!s || !*s) return false;
  long v = strtol(s, nullptr, 10);
  if (v < 0 || v > 255) return false;
  out = (uint8_t)v;
  return true;
}

static FloorNode* findFloor(uint8_t f_id) {
  for (int i=0;i<SMP_MAX_FLOORS;i++)
    if (MODEL.floors[i].used && MODEL.floors[i].floorId==f_id) return &MODEL.floors[i];
  return nullptr;
}
static RoomNode* findRoom(FloorNode* f, uint8_t r_id) {
  if (!f) return nullptr;
  for (int i=0;i<SMP_MAX_ROOMS;i++)
    if (f->rooms[i].used && f->rooms[i].roomId==r_id) return &f->rooms[i];
  return nullptr;
}
static UltraSensor* findUltra(RoomNode* r, uint8_t u_id) {
  if (!r) return nullptr;
  for (int i=0;i<SMP_MAX_SENSORS;i++)
    if (r->ultra[i].used && r->ultra[i].id==u_id) return &r->ultra[i];
  return nullptr;
}
static HallSensor* findHall(RoomNode* r, uint8_t hs_id) {
  if (!r) return nullptr;
  for (int i=0;i<SMP_MAX_SENSORS;i++)
    if (r->hall[i].used && r->hall[i].id==hs_id) return &r->hall[i];
  return nullptr;
}

// ---------- Add / Set ----------
bool addFloor(uint8_t f_id) {
  if (findFloor(f_id)) return true;
  for (int i=0;i<SMP_MAX_FLOORS;i++) {
    if (!MODEL.floors[i].used) {
      MODEL.floors[i].used    = true;
      MODEL.floors[i].floorId = f_id;
      MODEL.floors[i].connected = false;
      MODEL.floors[i].idByte  = 0;
      return true;
    }
  }
  return false;
}
bool addRoom(uint8_t f_id, uint8_t r_id) {
  if (!addFloor(f_id)) return false;
  FloorNode* f = findFloor(f_id);
  if (findRoom(f, r_id)) return true;
  for (int i=0;i<SMP_MAX_ROOMS;i++) {
    if (!f->rooms[i].used) {
      f->rooms[i].used      = true;
      f->rooms[i].roomId    = r_id;
      f->rooms[i].connected = false;
      f->rooms[i].idByte    = 0;
      return true;
    }
  }
  return false;
}
bool addUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id) {
  if (!addRoom(f_id, r_id)) return false;
  RoomNode* r = findRoom(findFloor(f_id), r_id);
  if (findUltra(r, u_id)) return true;
  for (int i=0;i<SMP_MAX_SENSORS;i++) {
    if (!r->ultra[i].used) {
      r->ultra[i].used = true;
      r->ultra[i].id   = u_id;
      r->ultra[i].value= 0;
      return true;
    }
  }
  return false;
}
bool addHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id) {
  if (!addRoom(f_id, r_id)) return false;
  RoomNode* r = findRoom(findFloor(f_id), r_id);
  if (findHall(r, hs_id)) return true;
  for (int i=0;i<SMP_MAX_SENSORS;i++) {
    if (!r->hall[i].used) {
      r->hall[i].used = true;
      r->hall[i].id   = hs_id;
      r->hall[i].open = false;
      return true;
    }
  }
  return false;
}

bool setSystemState(uint8_t s)      { MODEL.systemState = s; return true; }
bool setKeypad(uint8_t k)           { MODEL.keypad      = k; return true; }
bool setNetwork(uint8_t n)          { MODEL.network     = n; return true; }
bool setMac(const String& mac)      { MODEL.mac         = mac; return true; }
bool setFloorIdByte(uint8_t f_id, uint8_t idByte) {
  addFloor(f_id); FloorNode* f = findFloor(f_id); if (!f) return false; f->idByte = idByte; return true;
}
bool setFloorConnection(uint8_t f_id, bool cs) {
  addFloor(f_id); FloorNode* f = findFloor(f_id); if (!f) return false; f->connected = cs; return true;
}
bool setRoomIdByte(uint8_t f_id, uint8_t r_id, uint8_t idByte) {
  addRoom(f_id,r_id); RoomNode* r = findRoom(findFloor(f_id), r_id); if (!r) return false; r->idByte = idByte; return true;
}
bool setRoomConnection(uint8_t f_id, uint8_t r_id, bool cs) {
  addRoom(f_id,r_id); RoomNode* r = findRoom(findFloor(f_id), r_id); if (!r) return false; r->connected = cs; return true;
}
bool setUltraValue(uint8_t f_id, uint8_t r_id, uint8_t u_id, uint8_t val) {
  addUltra(f_id,r_id,u_id); UltraSensor* u = findUltra(findRoom(findFloor(f_id), r_id), u_id); if (!u) return false; u->value = val; return true;
}
bool setHallOpen(uint8_t f_id, uint8_t r_id, uint8_t hs_id, bool open) {
  addHall(f_id,r_id,hs_id); HallSensor* h = findHall(findRoom(findFloor(f_id), r_id), hs_id); if (!h) return false; h->open = open; return true;
}

void resetModel() { MODEL = ProtocolModel(); }

// ---------- Core parser (shared by cloud/node) ----------
static bool parseCore(const char* topicC, const char* rawPayload, bool cloud) {
  if (!topicC || !rawPayload) return false;
  String t = topicC;
  String p[12];
  int n = splitTopic(t, p, 12);
  if (n <= 0) return false;

  int base = 0;
  if (cloud) {
    if (n < 3 || p[0]!="ELEC520" || p[1]!="security") return false;
    base = 2;
  }

  // s/st  (BYTEENUM)
  if (n-base==2 && p[base]=="s" && p[base+1]=="st") {
    uint8_t v; if (!parseByte(rawPayload, v)) return false;
    return setSystemState(v);
  }
  // s/ke  (BYTEENUM)
  if (n-base==2 && p[base]=="s" && p[base+1]=="ke") {
    uint8_t v; if (!parseByte(rawPayload, v)) return false;
    return setKeypad(v);
  }
  // n/st  (BYTEENUM)
  if (n-base==2 && p[base]=="n" && p[base+1]=="st") {
    uint8_t v; if (!parseByte(rawPayload, v)) return false;
    return setNetwork(v);
  }
  // n/mc  (STRING)
  if (n-base==2 && p[base]=="n" && p[base+1]=="mc") {
    return setMac(String(rawPayload));
  }

  // f/{f_id}/id  (BYTE bitfield)
  if (n-base==3 && p[base]=="f" && p[base+2]=="id") {
    uint8_t f_id = (uint8_t)p[base+1].toInt();
    uint8_t v; if (!parseByte(rawPayload, v)) return false;
    return setFloorIdByte(f_id, v);
  }
  // f/{f_id}/cs  (BOOL)
  if (n-base==3 && p[base]=="f" && p[base+2]=="cs") {
    uint8_t f_id = (uint8_t)p[base+1].toInt();
    bool b; if (!parseBoolStrict(rawPayload, b)) return false;
    return setFloorConnection(f_id, b);
  }

  // f/{f_id}/r/{r_id}/id  (BYTE)
  if (n-base==5 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="id") {
    uint8_t f_id = (uint8_t)p[base+1].toInt();
    uint8_t r_id = (uint8_t)p[base+3].toInt();
    uint8_t v; if (!parseByte(rawPayload, v)) return false;
    return setRoomIdByte(f_id, r_id, v);
  }
  // f/{f_id}/r/{r_id}/cs  (BOOL)
  if (n-base==5 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="cs") {
    uint8_t f_id = (uint8_t)p[base+1].toInt();
    uint8_t r_id = (uint8_t)p[base+3].toInt();
    bool b; if (!parseBoolStrict(rawPayload, b)) return false;
    return setRoomConnection(f_id, r_id, b);
  }
  // f/{f_id}/r/{r_id}/u/{u_id}  (BYTE)
  if (n-base==6 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="u") {
    uint8_t f_id = (uint8_t)p[base+1].toInt();
    uint8_t r_id = (uint8_t)p[base+3].toInt();
    uint8_t u_id = (uint8_t)p[base+5].toInt();
    uint8_t v; if (!parseByte(rawPayload, v)) return false;
    return setUltraValue(f_id, r_id, u_id, v);
  }
  // f/{f_id}/r/{r_id}/h/{hs_id}  (BOOL)
  if (n-base==6 && p[base]=="f" && p[base+2]=="r" && p[base+4]=="h") {
    uint8_t f_id = (uint8_t)p[base+1].toInt();
    uint8_t r_id = (uint8_t)p[base+3].toInt();
    uint8_t hs_id= (uint8_t)p[base+5].toInt();
    bool b; if (!parseBoolStrict(rawPayload, b)) return false;
    return setHallOpen(f_id, r_id, hs_id, b);
  }

  return false; // not matched
}

bool parseCloud(const char* topic, const char* rawPayload) { return parseCore(topic, rawPayload, true ); }
bool parseNode (const char* topic, const char* rawPayload) { return parseCore(topic, rawPayload, false); }

// ---------- Topic builders ----------
static inline String _CLOUD(const String& tail){ return "ELEC520/security/" + tail; }

// Cloud
String topicSystemState()                           { return _CLOUD("s/st"); }
String topicKeypad()                                { return _CLOUD("s/ke"); }
String topicNetwork()                               { return _CLOUD("n/st"); }
String topicMac()                                   { return _CLOUD("n/mc"); }
String topicFloorIdByte(uint8_t f_id)               { return _CLOUD("f/"+String(f_id)+"/id"); }
String topicFloorConnection(uint8_t f_id)           { return _CLOUD("f/"+String(f_id)+"/cs"); }
String topicRoomIdByte(uint8_t f_id,uint8_t r_id)   { return _CLOUD("f/"+String(f_id)+"/r/"+String(r_id)+"/id"); }
String topicRoomConnection(uint8_t f_id,uint8_t r_id){ return _CLOUD("f/"+String(f_id)+"/r/"+String(r_id)+"/cs"); }
String topicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return _CLOUD("f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id)); }
String topicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return _CLOUD("f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id)); }

// Node (no header)
String nodeTopicSystemState()                           { return "s/st"; }
String nodeTopicKeypad()                                { return "s/ke"; }
String nodeTopicNetwork()                               { return "n/st"; }
String nodeTopicMac()                                   { return "n/mc"; }
String nodeTopicFloorIdByte(uint8_t f_id)               { return "f/"+String(f_id)+"/id"; }
String nodeTopicFloorConnection(uint8_t f_id)           { return "f/"+String(f_id)+"/cs"; }
String nodeTopicRoomIdByte(uint8_t f_id,uint8_t r_id)   { return "f/"+String(f_id)+"/r/"+String(r_id)+"/id"; }
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/cs"; }
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/u/"+String(u_id); }
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id){ return "f/"+String(f_id)+"/r/"+String(r_id)+"/h/"+String(hs_id); }
