#include "json_formatting.h"

// -------- Global --------
ProtocolModel MODEL;

//Used to store MODEL externally.
ProtocolModel StoreMODEL(){
  return MODEL;
}

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
  return OTHER;
}
static const char* toString(KeypadState k) {
  switch (k) {
    case NO_INPUT: return "NO_INPUT";
    case ACCEPTED: return "ACCEPTED";
    case DECLINED: return "DECLINED";
  }
  return "NO_INPUT";
}
static KeypadState parseKeypad(const JsonVariantConst& v) {
  if (v.is<const char*>()) {
    String s = v.as<const char*>(); s.toUpperCase();
    if (s=="NO_INPUT") return NO_INPUT;
    if (s=="ACCEPTED") return ACCEPTED;
    if (s=="DECLINED") return DECLINED;
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
bool setSystemKeypad(KeypadState key) { MODEL.keypad = key; return true; }
bool setNetworkStatus(bool connected) { MODEL.networkStatus = connected; return true; }
bool setNetworkMac(const String& mac) { MODEL.macAddress = mac; return true; }
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

  if (n < 2 || parts[0] != "ELEC520" || parts[1] != "security") return false;

  // system state
  if (parts[2]=="system" && n>=4) {
    if (parts[3]=="state" && root.containsKey("STATE")) {
      MODEL.systemState=parseSystemState(root["STATE"]); 
      return true;
    }
    if (parts[3]=="keypad" && root.containsKey("KEYPAD")) {
      MODEL.keypad=parseKeypad(root["KEYPAD"]); 
      return true;
    }
  }

  // network
  if (parts[2]=="network" && n>=4) {
    if (parts[3]=="status" && root.containsKey("STATUS")) {
      MODEL.networkStatus=parseBoolLoose(root["STATUS"]); 
      return true;
    }
    if (parts[3]=="mac" && n==5 && parts[4]=="adr" && root.containsKey("ADRRESS")) {
      MODEL.macAddress=root["ADRRESS"].as<const char*>(); 
      return true;
    }
  }

  // floor
  if (parts[2]=="flr" && n>=5) {
    int flr_id=parts[3].toInt();
    addFloor(flr_id);
    FloorNode* f=findFloor(flr_id);

    if (parts[4]=="id" && root.containsKey("FLR_ID")) {
      f->floorId=root["FLR_ID"]; 
      return true;
    }
    if (parts[4]=="con_state" && root.containsKey("CON_STATE")) {
      f->connection=parseBoolLoose(root["CON_STATE"]); 
      return true;
    }

    if (parts[4]=="rm" && n>=7) {
      int rm_id=parts[5].toInt();
      addRoom(flr_id,rm_id);
      RoomNode* r=findRoom(f,rm_id);

      if (parts[6]=="id" && root.containsKey("RM_ID")) {
        r->roomId=root["RM_ID"]; 
        return true;
      }
      if (parts[6]=="con_state" && root.containsKey("CON_STATE")) {
        r->connection=parseBoolLoose(root["CON_STATE"]); 
        return true;
      }
      if (parts[6]=="ultra" && n==8 && root.containsKey("U_STATE")) {
        int uId=parts[7].toInt();
        addUltrasonic(flr_id,rm_id,uId);
        UltraSensor* u=findUltra(r,uId);
        if(u){u->state=parseBoolLoose(root["U_STATE"]); return true;}
      }
      if (parts[6]=="hall" && n==8 && root.containsKey("HS_STATE")) {
        int hsId=parts[7].toInt();
        addHall(flr_id,rm_id,hsId);
        HallSensor* h=findHall(r,hsId);
        if(h){h->state=parseBoolLoose(root["HS_STATE"]); return true;}
      }
    }
  }

  return false;
}

// -------- Build Snapshot --------
String buildSnapshot() {
  DynamicJsonDocument doc(8192);

  JsonObject sys=doc.createNestedObject("System");
  sys["STATE"]=toString(MODEL.systemState);
  sys["KEYPAD"]=toString(MODEL.keypad);

  JsonObject net=doc.createNestedObject("Network");
  net["STATUS"]=MODEL.networkStatus;
  net["ADRRESS"]=MODEL.macAddress;

  JsonArray floors=doc.createNestedArray("Floors");
  for (int i=0;i<SMP_MAX_FLOORS;i++) if(MODEL.floors[i].used) {
    JsonObject jf=floors.createNestedObject();
    jf["FLR_ID"]=MODEL.floors[i].floorId;
    jf["CON_STATE"]=MODEL.floors[i].connection;
    JsonArray rooms=jf.createNestedArray("Rooms");
    for (int j=0;j<SMP_MAX_ROOMS;j++) if(MODEL.floors[i].rooms[j].used) {
      JsonObject jr=rooms.createNestedObject();
      jr["RM_ID"]=MODEL.floors[i].rooms[j].roomId;
      jr["CON_STATE"]=MODEL.floors[i].rooms[j].connection;
      JsonArray jU=jr.createNestedArray("Ultra");
      for (int k=0;k<SMP_MAX_SENSORS;k++) if(MODEL.floors[i].rooms[j].ultra[k].used) {
        JsonObject ju=jU.createNestedObject();
        ju["U_STATE"]=MODEL.floors[i].rooms[j].ultra[k].state;
      }
      JsonArray jH=jr.createNestedArray("Hall");
      for (int k=0;k<SMP_MAX_SENSORS;k++) if(MODEL.floors[i].rooms[j].hall[k].used) {
        JsonObject jh=jH.createNestedObject();
        jh["HS_STATE"]=MODEL.floors[i].rooms[j].hall[k].state;
      }
    }
  }
  String out;
  serializeJson(doc,out);
  return out;
}



// -------- Topic Builders --------
String topicSystemState(){return "ELEC520/security/system/state";}
String topicSystemKeypad(){return "ELEC520/security/system/keypad";}
String topicNetworkStatus(){return "ELEC520/security/network/status";}
String topicNetworkMac(){return "ELEC520/security/network/mac/adr";}
String topicFloorId(int flr_id){return "ELEC520/security/flr/"+String(flr_id)+"/id";}
String topicFloorConnection(int flr_id){return "ELEC520/security/flr/"+String(flr_id)+"/con_state";}
String topicRoomId(int flr_id,int rm_id){return "ELEC520/security/flr/"+String(flr_id)+"/rm/"+String(rm_id)+"/id";}
String topicRoomConnection(int flr_id,int rm_id){return "ELEC520/security/flr/"+String(flr_id)+"/rm/"+String(rm_id)+"/con_state";}
String topicUltrasonic(int flr_id,int rm_id,int u_id){return "ELEC520/security/flr/"+String(flr_id)+"/rm/"+String(rm_id)+"/ultra/"+String(u_id);}
String topicHall(int flr_id,int rm_id,int hs_id){return "ELEC520/security/flr/"+String(flr_id)+"/rm/"+String(rm_id)+"/hall/"+String(hs_id);}
