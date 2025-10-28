#ifndef ELEC520_PROTOCOL_H
#define ELEC520_PROTOCOL_H

#include <Arduino.h>

// -------- Limits --------
#define SMP_MAX_FLOORS   8
#define SMP_MAX_ROOMS    8
#define SMP_MAX_SENSORS  8

// -------- Enums --------
enum SystemState : uint8_t { DISARMED=0, ARMED=1, ALARM=2, OTHER=3 };
enum KeypadState : uint8_t { NO_INPUT=0, ACCEPTED=1, DECLINED=2 };

// -------- Data Structures --------
struct UltraSensor {
  bool     used  = false;
  uint8_t  value = 0;
};
struct HallSensor {
  bool used = false;
  bool open = false;
};
struct RoomNode {
  bool     used       = false;
  bool     connected  = false; // f/{f}/r/{r}/cs
  uint32_t ts         = 0;     // f/{f}/r/{r}/ts (Unix)
  UltraSensor ultra[SMP_MAX_SENSORS];
  HallSensor  hall [SMP_MAX_SENSORS];
};
struct FloorNode {
  bool     used       = false;
  bool     connected  = false; // f/{f}/cs
  uint32_t ts         = 0;     // f/{f}/ts (Unix)
  RoomNode rooms[SMP_MAX_ROOMS];
};
struct ProtocolModel {
  uint8_t   systemState = DISARMED; // s/st
  uint8_t   keypad      = NO_INPUT; // s/ke
  uint8_t   network     = 0;        // n/st
  String    mac         = "";       // n/mc
  FloorNode floors[SMP_MAX_FLOORS]; // f/{f}
};

// -------- Global MODEL --------
extern ProtocolModel MODEL;

// -------- Add --------
bool addFloor(uint8_t f_id);
bool addRoom(uint8_t f_id, uint8_t r_id);
bool addUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id);
bool addHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id);

// -------- System-level Set (kept) --------
bool setSystemState(uint8_t s);
bool setKeypad(uint8_t k);
bool setNetwork(uint8_t n);
bool setMac(const String& mac);

void resetModel();

// -------- Parsers --------
bool parseNode (const char* topic, const char* rawPayload);
bool parseCloud(const char* topic, const char* rawPayload);

// Convenience parser for single "topic:value" strings, e.g. "f/1/r/1/h/1:1"
bool parseTokenLine(const String& tokenLine);

// -------- Topic builders (Node, no prefix) --------
String nodeTopicSystemState();                               // s/st
String nodeTopicKeypad();                                    // s/ke
String nodeTopicNetwork();                                   // n/st
String nodeTopicMac();                                       // n/mc
String nodeTopicFloorConnection(uint8_t f_id);               // f/{f}/cs
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id);   // f/{f}/r/{r}/cs
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id);   // f/{f}/r/{r}/u/{u}
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id);   // f/{f}/r/{r}/h/{h}
String nodeTopicFloorTimestamp(uint8_t f_id);                // f/{f}/ts
String nodeTopicRoomTimestamp(uint8_t f_id,uint8_t r_id);    // f/{f}/r/{r}/ts

// -------- Topic builders (Cloud, with prefix) --------
String cloudTopicFloor(uint8_t f_id);                        // ELEC520/security/f/{f}
String cloudTopicSystemState();                              // ELEC520/security/s/st
String cloudTopicKeypad();                                   // ELEC520/security/s/ke
String cloudTopicNetwork();                                  // ELEC520/security/n/st
String cloudTopicMac();                                      // ELEC520/security/n/mc
String cloudTopicFloorConnection(uint8_t f_id);              // ELEC520/security/f/{f}/cs
String cloudTopicRoomConnection(uint8_t f_id,uint8_t r_id);  // ELEC520/security/f/{f}/r/{r}/cs
String cloudTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id); // ELEC520/security/f/{f}/r/{r}/u/{u}
String cloudTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id); // ELEC520/security/f/{f}/r/{r}/h/{h}
String cloudTopicFloorTimestamp(uint8_t f_id);               // ELEC520/security/f/{f}/ts
String cloudTopicRoomTimestamp(uint8_t f_id,uint8_t r_id);   // ELEC520/security/f/{f}/r/{r}/ts

// -------- ESP-NOW per-room compact string --------
String buildRoomEspString(uint8_t f_id, uint8_t r_id);
bool   parseRoomEspString(const String& roomData);

// -------- MQTT full-system compact string --------
String buildSystemMqttString();
bool   parseSystemMqttString(const String& systemData);

// -------- MQTT per-floor compact string (payload for ELEC520/security/f/{f}) --------
String buildFloorMqttString(uint8_t f_id);


// ----- Debug helpers -----
// Pretty, multi-line dump of the entire MODEL to any Arduino Stream (e.g., Serial)
void debugPrintModel(Stream& out);


#endif // ELEC520_PROTOCOL_H
