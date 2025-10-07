#ifndef ELEC520_PROTOCOL
#define ELEC520_PROTOCOL

#include <Arduino.h>

// ---------- Limits ----------
#define SMP_MAX_FLOORS   8
#define SMP_MAX_ROOMS    8
#define SMP_MAX_SENSORS  8

// ---------- Enums (BYTEENUM over the wire) ----------
enum SystemState : uint8_t { DISARMED=0, ARMED=1, ALARM=2, OTHER=3 };
enum KeypadState : uint8_t { NO_INPUT=0, ACCEPTED=1, DECLINED=2 };

// ---------- Data Structures ----------
struct UltraSensor {
  bool     used  = false;
  uint8_t  id    = 0;      // topic path {u_id}
  uint8_t  value = 0;      // BYTE (bitfield)
};

struct HallSensor {
  bool used  = false;
  uint8_t id = 0;          // topic path {hs_id}
  bool  open = false;      // BOOL (true=open, false=closed)
};

struct RoomNode {
  bool     used        = false;
  uint8_t  roomId      = 0;     // topic path {r_id}
  bool     connected   = false; // f/{f_id}/r/{r_id}/cs
  uint8_t  idByte      = 0;     // f/{f_id}/r/{r_id}/id  (bitfield)
  UltraSensor ultra[SMP_MAX_SENSORS];
  HallSensor  hall [SMP_MAX_SENSORS];
};

struct FloorNode {
  bool     used        = false;
  uint8_t  floorId     = 0;     // topic path {f_id}
  bool     connected   = false; // f/{f_id}/cs
  uint8_t  idByte      = 0;     // f/{f_id}/id (bitfield)
  RoomNode rooms[SMP_MAX_ROOMS];
};

struct ProtocolModel {
  uint8_t     systemState = DISARMED; // BYTEENUM
  uint8_t     keypad      = NO_INPUT; // BYTEENUM
  uint8_t     network     = 0;        // BYTEENUM (CONNECTED/DISCONNECT as 0/1 or defined mapping)
  String      mac         = "";       // STRING (e.g., "00:00:00:00:00:00")
  FloorNode   floors[SMP_MAX_FLOORS];
};

// ---------- Global ----------
extern ProtocolModel MODEL;

// ---------- Add / Set (minimal) ----------
bool addFloor(uint8_t f_id);
bool addRoom(uint8_t f_id, uint8_t r_id);
bool addUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id);
bool addHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id);

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

// ---------- Parsers (RAW payloads, no JSON) ----------
// Cloud topics MUST start with "ELEC520/security/..."
// Node topics are headerless ("s/st", "f/1/r/2/u/0", etc.)
bool parseCloud(const char* topic, const char* rawPayload);
bool parseNode (const char* topic, const char* rawPayload);

// ---------- Topic builders (Cloud, with header) ----------
String topicSystemState();                          // ELEC520/security/s/st
String topicKeypad();                               // ELEC520/security/s/ke
String topicNetwork();                              // ELEC520/security/n/st
String topicMac();                                  // ELEC520/security/n/mc
String topicFloorIdByte(uint8_t f_id);              // ELEC520/security/f/{f_id}/id
String topicFloorConnection(uint8_t f_id);          // ELEC520/security/f/{f_id}/cs
String topicRoomIdByte(uint8_t f_id,uint8_t r_id);  // ELEC520/security/f/{f_id}/r/{r_id}/id
String topicRoomConnection(uint8_t f_id,uint8_t r_id);// ELEC520/security/f/{f_id}/r/{r_id}/cs
String topicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id);// ELEC520/security/f/{f_id}/r/{r_id}/u/{u_id}
String topicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id); // ELEC520/security/f/{f_id}/r/{r_id}/h/{hs_id}

// ---------- Topic builders (Node, NO header) ----------
String nodeTopicSystemState();                          // s/st
String nodeTopicKeypad();                               // s/ke
String nodeTopicNetwork();                              // n/st
String nodeTopicMac();                                  // n/mc
String nodeTopicFloorIdByte(uint8_t f_id);              // f/{f_id}/id
String nodeTopicFloorConnection(uint8_t f_id);          // f/{f_id}/cs
String nodeTopicRoomIdByte(uint8_t f_id,uint8_t r_id);  // f/{f_id}/r/{r_id}/id
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id);// f/{f_id}/r/{r_id}/cs
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id);// f/{f_id}/r/{r_id}/u/{u_id}
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id); // f/{f_id}/r/{r_id}/h/{hs_id}

#endif
