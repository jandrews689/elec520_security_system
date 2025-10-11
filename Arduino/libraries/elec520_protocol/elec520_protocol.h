#ifndef ELEC520_PROTOCOL_H
#define ELEC520_PROTOCOL_H

#include <Arduino.h>

// -------- Limits (adjust as needed) --------
#define SMP_MAX_FLOORS   8
#define SMP_MAX_ROOMS    8
#define SMP_MAX_SENSORS  8

// -------- Enums on the wire (BYTE values) --------
enum SystemState : uint8_t { DISARMED=0, ARMED=1, ALARM=2, OTHER=3 };
enum KeypadState : uint8_t { NO_INPUT=0, ACCEPTED=1, DECLINED=2 };

// -------- Data Structures (NO id fields inside components) --------
struct UltraSensor {
  bool     used  = false;
  uint8_t  value = 0;     // BYTE (bitfield 0..255)
};

struct HallSensor {
  bool used  = false;
  bool open  = false;     // 0/1 on wire
};

struct RoomNode {
  bool     used        = false;
  bool     connected   = false; // r/{r}/cs
  UltraSensor ultra[SMP_MAX_SENSORS];  // index == u_id
  HallSensor  hall [SMP_MAX_SENSORS];  // index == hs_id
};

struct FloorNode {
  bool     used        = false;
  bool     connected   = false; // f/{f}/cs
  uint8_t  rssi        = 0;     // NEW: f/{f}/rsi (BYTE)
  RoomNode rooms[SMP_MAX_ROOMS];      // index == r_id
};

struct ProtocolModel {
  uint8_t   systemState = DISARMED; // s/st
  uint8_t   keypad      = NO_INPUT; // s/ke
  uint8_t   network     = 0;        // n/st (0=disc,1=conn…)
  String    mac         = "";       // n/mc (string is OK; not sent in compact bulk)
  FloorNode floors[SMP_MAX_FLOORS]; // index == f_id
};

// -------- Global MODEL --------
extern ProtocolModel MODEL;

// -------- Add / Set (IDs are the indexes you pass) --------
bool addFloor(uint8_t f_id);
bool addRoom(uint8_t f_id, uint8_t r_id);
bool addUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id);
bool addHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id);

bool setSystemState(uint8_t s);
bool setKeypad(uint8_t k);
bool setNetwork(uint8_t n);
bool setMac(const String& mac);

bool setFloorConnection(uint8_t f_id, bool cs01);
bool setRoomConnection(uint8_t f_id, uint8_t r_id, bool cs01);
bool setUltraValue(uint8_t f_id, uint8_t r_id, uint8_t u_id, uint8_t val);
bool setHallOpen(uint8_t f_id, uint8_t r_id, uint8_t hs_id, bool open01);

// NEW: RSSI setter
bool setFloorRssi(uint8_t f_id, uint8_t rssi);

void resetModel();

// -------- Single-topic parsers (strict, raw numeric/bool as 0/1) --------
bool parseNode(const char* topic, const char* rawPayload);   // no header
bool parseCloud(const char* topic, const char* rawPayload);  // requires ELEC520/security/

// -------- Topic builders (Node: NO header) --------
String nodeTopicSystemState();                         // s/st
String nodeTopicKeypad();                              // s/ke
String nodeTopicNetwork();                             // n/st
String nodeTopicMac();                                 // n/mc
String nodeTopicFloorConnection(uint8_t f_id);         // f/{f}/cs
String nodeTopicRoomConnection(uint8_t f_id,uint8_t r_id); // f/{f}/r/{r}/cs
String nodeTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id);  // f/{f}/r/{r}/u/{u}
String nodeTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id);   // f/{f}/r/{r}/h/{h}
// NEW: floor RSSI
String nodeTopicFloorRssi(uint8_t f_id);               // f/{f}/rsi

// -------- Topic builders (Cloud: WITH header per topic, if you need singles) --------
String cloudTopicSystemState();                        // ELEC520/security/s/st
String cloudTopicKeypad();                             // ELEC520/security/s/ke
String cloudTopicNetwork();                            // ELEC520/security/n/st
String cloudTopicMac();                                // ELEC520/security/n/mc
String cloudTopicFloorConnection(uint8_t f_id);        // ELEC520/security/f/{f}/cs
String cloudTopicRoomConnection(uint8_t f_id,uint8_t r_id);// ELEC520/security/f/{f}/r/{r}/cs
String cloudTopicUltra(uint8_t f_id,uint8_t r_id,uint8_t u_id);// ELEC520/security/f/{f}/r/{r}/u/{u}
String cloudTopicHall(uint8_t f_id,uint8_t r_id,uint8_t hs_id); // ELEC520/security/f/{f}/r/{r}/h/{h}
// NEW: floor RSSI
String cloudTopicFloorRssi(uint8_t f_id);              // ELEC520/security/f/{f}/rsi

// -------- ESP-NOW per-room compact string --------
// Format: f/<f_id>/r/<r_id>/cs:<0|1>;u/<u_id>:<0..255>;h/<hs_id>:<0|1>;...
String buildRoomEspString(uint8_t f_id, uint8_t r_id);
bool   parseRoomEspString(const String& roomData);

// -------- MQTT full-system compact string (single message) --------
// Single prefix once: "ELEC520/security/" then multiple floors & rooms compact.
// Uses only numeric payloads (enums as ints, bools as 0/1).
String buildSystemMqttString();
bool   parseSystemMqttString(const String& systemData);


// --- RSSI extraction helpers (non-breaking additions) ---
/**
 * Extract f_id and rssi from a topic + payload pair.
 * Accepts:
 *  - "f/<f_id>/rsi" + payload "0..255"
 *  - "ELEC520/security/f/<f_id>/rsi" + payload "0..255"
 * Returns true on success.
 */
bool extractFloorRssiFromTopicPayload(const char* topic,
                                      const char* payload,
                                      uint8_t& out_f_id,
                                      uint8_t& out_rssi);

/**
 * Extract f_id and rssi from a single line.
 * Accepts common ESP-NOW styles:
 *  - "f/<f_id>/rsi:<val>"
 *  - "ELEC520/security/f/<f_id>/rsi:<val>"
 *  - "f/<f_id>/rsi <val>"
 *  - "f/<f_id>/rsi=<val>"
 * Returns true on success.
 */
bool extractFloorRssiFromSingle(const String& line,
                                uint8_t& out_f_id,
                                uint8_t& out_rssi);

#endif // ELEC520_PROTOCOL_H
