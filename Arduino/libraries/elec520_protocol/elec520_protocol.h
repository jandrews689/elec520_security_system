#ifndef ELEC520_PROTOCOL_H
#define ELEC520_PROTOCOL_H

#include <Arduino.h>

// ---------- Limits ----------
#define SMP_MAX_FLOORS   8
#define SMP_MAX_ROOMS    8
#define SMP_MAX_SENSORS  8

// ---------- Enums (BYTEENUM on wire) ----------
enum SystemState : uint8_t { DISARMED=0, ARMED=1, ALARM=2, OTHER=3 };
enum KeypadState : uint8_t { NO_INPUT=0, ACCEPTED=1, DECLINED=2 };

// ---------- Data Structures ----------
struct UltraSensor {
  bool     used  = false;
  uint8_t  id    = 0;      // {u_id}
  uint8_t  value = 0;      // BYTE (bitfield 0..255)
};

struct HallSensor {
  bool     used  = false;
  uint8_t  id    = 0;      // {hs_id}
  bool     open  = false;  // true=open, false=closed
};

struct RoomNode {
  bool     used        = false;
  uint8_t  roomId      = 0;     // {r_id}
  bool     connected   = false; // r/{r_id}/cs
  uint8_t  idByte      = 0;     // r/{r_id}/id  (bitfield, optional)
  UltraSensor ultra[SMP_MAX_SENSORS];
  HallSensor  hall [SMP_MAX_SENSORS];
};

struct FloorNode {
  bool     used        = false;
  uint8_t  floorId     = 0;     // {f_id}
  bool     connected   = false; // f/{f_id}/cs
  uint8_t  idByte      = 0;     // f/{f_id}/id  (bitfield, optional)
  RoomNode rooms[SMP_MAX_ROOMS];
};

struct ProtocolModel {
  uint8_t   systemState = DISARMED; // s/st
  uint8_t   keypad      = NO_INPUT; // s/ke
  uint8_t   network     = 0;        // n/st (0=disc,1=conn or your mapping)
  String    mac         = "";       // n/mc ("AA:BB:CC:DD:EE:FF")
  FloorNode floors[SMP_MAX_FLOORS];
};

// ---------- Global MODEL ----------
extern ProtocolModel MODEL;

// ---------- Adders ----------
bool addFloor(uint8_t f_id);
bool addRoom(uint8_t f_id, uint8_t r_id);
bool addUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id);
bool addHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id);

// ---------- Setters ----------
bool setSystemState(uint8_t s);
bool setKeypad(uint8_t k);
bool setNetwork(uint8_t n);
bool setMac(const String& mac);

bool setFloorIdByte(uint8_t f_id, uint8_t idByte);
bool setFloorConnection(uint8_t f_id, bool cs);
bool setRoomIdByte(uint8_t f_id, uint8_t r_id, uint8_t idByte);
bool setRoomConnection(uint8_t f_id, uint8_t r_id, bool cs);
bool setUltraValue(uint8_t f_id, uint8_t r_id, uint8_t u_id, uint8_t val);
bool setHallOpen(uint8_t f_id, uint8_t r_id, uint8_t hs_id, bool open);

void resetModel();

// ---------- Topic builders (Node: NO header) ----------
String nodeTopicSystemState();                           // s/st
String nodeTopicKeypad();                                // s/ke
String nodeTopicNetwork();                               // n/st
String nodeTopicMac();                                   // n/mc
String nodeTopicFloorIdByte(uint8_t f_id);               // f/{f_id}/id
String nodeTopicFloorConnection(uint8_t f_id);           // f/{f_id}/cs
String nodeTopicRoomIdByte(uint8_t f_id,uint8_t r_id);   // f/{f_id}/r/{r_id}/id
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id);// f/{f_id}/r/{r_id}/cs
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id);// f/{f_id}/r/{r_id}/u/{u_id}
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id); // f/{f_id}/r/{r_id}/h/{hs_id}

// ---------- Topic builders (Cloud: WITH header once each topic) ----------
String cloudTopicSystemState();                           // ELEC520/security/s/st
String cloudTopicKeypad();                                // ELEC520/security/s/ke
String cloudTopicNetwork();                               // ELEC520/security/n/st
String cloudTopicMac();                                   // ELEC520/security/n/mc
String cloudTopicFloorIdByte(uint8_t f_id);               // ELEC520/security/f/{f_id}/id
String cloudTopicFloorConnection(uint8_t f_id);           // ELEC520/security/f/{f_id}/cs
String cloudTopicRoomIdByte(uint8_t f_id,uint8_t r_id);   // ELEC520/security/f/{f_id}/r/{r_id}/id
String cloudTopicRoomConnection(uint8_t f_id,uint8_t r_id);// ELEC520/security/f/{f_id}/r/{r_id}/cs
String cloudTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id);// ELEC520/security/f/{f_id}/r/{r_id}/u/{u_id}
String cloudTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id); // ELEC520/security/f/{f_id}/r/{r_id}/h/{hs_id}

// ---------- Parsers for single topic/value ----------
bool parseNode(const char* topic, const char* rawPayload);    // expects NO header
bool parseCloud(const char* topic, const char* rawPayload);   // expects ELEC520/security/ header

// ---------- Floor (ESP-NOW) compact strings ----------
String buildFloorEspString(uint8_t floorId);                 // f/1/r/1/h/1:true;/r/2/u/0:128;...
bool   parseFloorEspString(const String& floorData);         // updates MODEL

// ---------- System (MQTT) compact string (single message) ----------
String buildSystemMqttString();                              // ELEC520/security/f/1/...; /f/2/...;
bool   parseSystemMqttString(const String& systemData);      // updates MODEL

#endif // ELEC520_PROTOCOL_H
