#include "json_formatting.h"

// -------- Global --------
ProtocolModel MODEL;

// -------- Helpers --------
static const char* toString(SystemState s) {
  switch (s) {
    case DISARMED: return "DISARMED";
    case ARMED:    return "ARMED";
    case ALARM:    return "ALARM";
    default:       return "OTHER";
  }
}
static SystemState parseSystemState(const JsonVariantConst& v) {
  if (v.is<const char*>()) {
    String s = v.as<const char*>(); s.toUpperCase();
    if (s=="DISARMED") return DISARMED;
    if (s=="ARMED")    return ARMED;
    if (s=="ALARM")    return ALARM;
    return OTHER;
  }
  if (v.is<int>()) {
    int x = v.as<int>();
    if (x==0) return DISARMED;
    if (x==1) return ARMED;
    if (x==2) return ALARM;
    return OTHER;
  }
  return OTHER;
}
static const char* toString(KeypadState k) {
  switch (k) {
    case NO_INPUT:      return "NO_INPUT";
    case PASS_ACCEPTED: return "PASS_ACCEPTED";
    case PASS_DECLINED: return "PASS_DECLINED";
  }
  return "NO_INPUT";
}
static KeypadState parseKeypad(const JsonVariantConst& v) {
  if (v.is<const char*>()) {
    String s = v.as<const char*>(); s.toUpperCase();
    if (s=="NO_INPUT")      return NO_INPUT;
    if (s=="PASS_ACCEPTED") return PASS_ACCEPTED;
    if (s=="PASS_DECLINED") return PASS_DECLINED;
  }
  if (v.is<int>()) {
    int x = v.as<int>();
    if (x==0) return NO_INPUT;
    if (x==1) return PASS_ACCEPTED;
    if (x==2) return PASS_DECLINED;
  }
  return NO_INPUT;
}
static bool parseBoolLoose(const JsonVariantConst& v, bool deflt=false) {
  if (v.is<bool>()) return v.as<bool>();
  if (v.is<int>())  return v.as<int>() != 0;
  if (v.is<const char*>()) {
    String s=v.as<const char*>(); s.toLowerCase();
    if (s=="true") return true;
    if (s=="false") return false;
  }
  return deflt;
}

// -------- Internal Finders --------
static FloorNode* findFloor(int floorId) {
  for (int i=0;i<SMP_MAX_FLOORS;i++)
    if (MODEL.floors[i].used && MODEL.floors[i].floorId==floorId) return &MODEL.floors[i];
  return nullptr;
}
static RoomNode* findRoom(FloorNode* f, int roomId) {
  if (!f) return nullptr;
  for (int i=0;i<SMP_MAX_ROOMS;i++)
    if (f->rooms[i].used && f->rooms[i].roomId==roomId) return &f->rooms[i];
  return nullptr;
}
static UltraSensor* findUltra(RoomNode* r, int uid) {
  if (!r) return nullptr;
  for (int i=0;i<SMP_MAX_SENSORS;i++)
    if (r->ultra[i].used && r->ultra[i].id==uid) return &r->ultra[i];
  return nullptr;
}
static HallSensor* findHall(RoomNode* r, int hid) {
  if (!r) return nullptr;
  for (int i=0;i<SMP_MAX_SENSORS;i++)
    if (r->hall[i].used && r->hall[i].id==hid) return &r->hall[i];
  return nullptr;
}

// -------- Reset --------
void resetModel() { MODEL = ProtocolModel(); }

// -------- Adders --------
bool addFloor(int floorId) {
  if (findFloor(floorId)) return true;
  for (int i=0;i<SMP_MAX_FLOORS;i++) {
    if (!MODEL.floors[i].used) {
      MODEL.floors[i].used=true;
      MODEL.floors[i].floorId=floorId;
      MODEL.floors[i].connection=false;
      return true;
    }
  }
  return false;
}
bool addRoom(int floorId, int roomId) {
  if (!addFloor(floorId)) return false;
  FloorNode* f=findFloor(floorId);
  if (findRoom(f, roomId)) return true;
  for (int i=0;i<SMP_MAX_ROOMS;i++) {
    if (!f->rooms[i].used) {
      f->rooms[i].used=true;
      f->rooms[i].roomId=roomId;
      f->rooms[i].connection=false;
      return true;
    }
  }
  return false;
}
bool addUltrasonic(int floorId, int roomId, int uId) {
  if (!addRoom(floorId, roomId)) return false;
  RoomNode* r=findRoom(findFloor(floorId), roomId);
  if (findUltra(r,uId)) return true;
  for (int i=0;i<SMP_MAX_SENSORS;i++) {
    if (!r->ultra[i].used) {
      r->ultra[i].used=true;
      r->ultra[i].id=uId;
      return true;
    }
  }
  return false;
}
bool addHall(int floorId, int roomId, int hsId) {
  if (!addRoom(floorId, roomId)) return false;
  RoomNode* r=findRoom(findFloor(floorId), roomId);
  if (findHall(r,hsId)) return true;
  for (int i=0;i<SMP_MAX_SENSORS;i++) {
    if (!r->hall[i].used) {
      r->hall[i].used=true;
      r->hall[i].id=hsId;
      return true;
    }
  }
  return false;
}

// -------- Setters --------
bool setSystemState(SystemState state) { MODEL.systemState = state; return true; }
bool setBaseConnection(bool connected) { MODEL.baseConnected = connected; return true; }
bool setBaseKeypad(KeypadState key) { MODEL.keypad = key; return true; }
bool setFloorConnection(int floorId, bool connected) {
  addFloor(floorId); FloorNode* f=findFloor(floorId);
  if(!f) return false; f->connection=connected; return true;
}
bool setRoomConnection(int floorId,int roomId,bool connected) {
  addRoom(floorId, roomId); RoomNode* r=findRoom(findFloor(floorId), roomId);
  if(!r) return false; r->connection=connected; return true;
}
bool setUltrasonicState(int floorId,int roomId,int uId,bool state) {
  addUltrasonic(floorId,roomId,uId);
  UltraSensor* u=findUltra(findRoom(findFloor(floorId), roomId), uId);
  if(!u) return false; u->state=state; return true;
}
bool setHallState(int floorId,int roomId,int hsId,bool state) {
  addHall(floorId,roomId,hsId);
  HallSensor* h=findHall(findRoom(findFloor(floorId), roomId), hsId);
  if(!h) return false; h->state=state; return true;
}

// -------- Parse incoming topic+JSON --------
bool parseMqtt(const char* topicC,const char* payloadC) {
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, payloadC)) return false;
  JsonObjectConst root=doc.as<JsonObjectConst>();

  String topic=topicC;
  int n=0; String parts[10];
  int start=0, idx=0;
  while ((idx=topic.indexOf('/',start))>=0 && n<10) {
    parts[n++]=topic.substring(start,idx); start=idx+1;
  }
  if (n<10 && start<(int)topic.length()) parts[n++]=topic.substring(start);
  if (n<3 || parts[0]!="home" || parts[1]!="security") return false;

  // system
  if (parts[2]=="system" && n==4 && parts[3]=="state" && root.containsKey("STATE")) {
    MODEL.systemState=parseSystemState(root["STATE"]); return true;
  }
  // basestation
  if (parts[2]=="basestation" && n==4) {
    if (parts[3]=="connection" && root.containsKey("ConnectionState")) {
      MODEL.baseConnected=parseBoolLoose(root["ConnectionState"]); return true;
    }
    if (parts[3]=="keypad" && root.containsKey("Keypad")) {
      MODEL.keypad=parseKeypad(root["Keypad"]); return true;
    }
  }
  // floor
  if (parts[2]=="floor" && n>=4) {
    int floorId=parts[3].toInt();
    addFloor(floorId);
    FloorNode* f=findFloor(floorId);
    if (n==5 && parts[4]=="connection" && root.containsKey("ConnectionState")) {
      f->connection=parseBoolLoose(root["ConnectionState"]); return true;
    }
    if (n>=6 && parts[4]=="room") {
      int roomId=parts[5].toInt();
      addRoom(floorId,roomId);
      RoomNode* r=findRoom(f,roomId);
      if (n==7 && parts[6]=="connection" && root.containsKey("ConnectionState")) {
        r->connection=parseBoolLoose(root["ConnectionState"]); return true;
      }
      if (n==8 && parts[6]=="ultrasonic" && root.containsKey("U_STATE")) {
        int uId=parts[7].toInt();
        addUltrasonic(floorId,roomId,uId);
        UltraSensor* u=findUltra(r,uId);
        if(u){u->state=parseBoolLoose(root["U_STATE"]); return true;}
      }
      if (n==8 && parts[6]=="hall" && root.containsKey("HS_STATE")) {
        int hsId=parts[7].toInt();
        addHall(floorId,roomId,hsId);
        HallSensor* h=findHall(r,hsId);
        if(h){h->state=parseBoolLoose(root["HS_STATE"]); return true;}
      }
    }
  }
  return false;
}

// -------- Build Snapshot --------
String buildSnapshot() {
  DynamicJsonDocument doc(16384);

  JsonObject sys=doc.createNestedObject("System");
  sys["STATE"]=toString(MODEL.systemState);

  JsonObject bs=doc.createNestedObject("BaseStation");
  bs["ConnectionState"]=MODEL.baseConnected;
  bs["Keypad"]=toString(MODEL.keypad);

  JsonArray floors=doc.createNestedArray("FloorNode");
  for (int i=0;i<SMP_MAX_FLOORS;i++) if(MODEL.floors[i].used) {
    JsonObject jf=floors.createNestedObject();
    jf["FLOOR_ID"]=MODEL.floors[i].floorId;
    jf["ConnectionState"]=MODEL.floors[i].connection;
    JsonArray rooms=jf.createNestedArray("RoomNode");
    for (int j=0;j<SMP_MAX_ROOMS;j++) if(MODEL.floors[i].rooms[j].used) {
      JsonObject jr=rooms.createNestedObject();
      jr["ROOM_ID"]=MODEL.floors[i].rooms[j].roomId;
      jr["ConnectionState"]=MODEL.floors[i].rooms[j].connection;
      JsonArray jU=jr.createNestedArray("UltraSensor");
      for (int k=0;k<SMP_MAX_SENSORS;k++) if(MODEL.floors[i].rooms[j].ultra[k].used) {
        JsonObject ju=jU.createNestedObject();
        ju["U_ID"]=MODEL.floors[i].rooms[j].ultra[k].id;
        ju["U_STATE"]=MODEL.floors[i].rooms[j].ultra[k].state;
      }
      JsonArray jH=jr.createNestedArray("HallSensor");
      for (int k=0;k<SMP_MAX_SENSORS;k++) if(MODEL.floors[i].rooms[j].hall[k].used) {
        JsonObject jh=jH.createNestedObject();
        jh["HS_ID"]=MODEL.floors[i].rooms[j].hall[k].id;
        jh["HS_STATE"]=MODEL.floors[i].rooms[j].hall[k].state;
      }
    }
  }
  String out;
  serializeJson(doc,out);
  return out;
}

// -------- Topic Builders --------
String topicSystemState(){return "home/security/system/state";}
String topicBaseConnection(){return "home/security/basestation/connection";}
String topicBaseKeypad(){return "home/security/basestation/keypad";}
String topicFloorConnection(int floorId){return "home/security/floor/"+String(floorId)+"/connection";}
String topicRoomConnection(int floorId,int roomId){return "home/security/floor/"+String(floorId)+"/room/"+String(roomId)+"/connection";}
String topicUltrasonic(int floorId,int roomId,int uId){return "home/security/floor/"+String(floorId)+"/room/"+String(roomId)+"/ultrasonic/"+String(uId);}
String topicHall(int floorId,int roomId,int hsId){return "home/security/floor/"+String(floorId)+"/room/"+String(roomId)+"/hall/"+String(hsId);}
