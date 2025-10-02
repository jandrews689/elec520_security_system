#ifndef JSON_FORMATTING_H
#define JSON_FORMATTING_H

#include <Arduino.h>
#include <ArduinoJson.h>

// -------- Limits --------
#define SMP_MAX_FLOORS   8
#define SMP_MAX_ROOMS    8
#define SMP_MAX_SENSORS  8

// -------- Enums --------
enum SystemState { DISARMED=0, ARMED=1, ALARM=2, OTHER=3 };
enum KeypadState { NO_INPUT=0, ACCEPTED=1, DECLINED=2 };

// -------- Data Structures --------
struct UltraSensor {
  bool used = false;
  int  id   = -1;
  bool state= false;
};

struct HallSensor {
  bool used = false;
  int  id   = -1;
  bool state= false;
};

struct RoomNode {
  bool used        = false;
  int  roomId      = -1;
  bool connection  = false;
  UltraSensor ultra[SMP_MAX_SENSORS];
  HallSensor  hall [SMP_MAX_SENSORS];
};

struct FloorNode {
  bool used        = false;
  int  floorId     = -1;
  bool connection  = false;
  RoomNode rooms[SMP_MAX_ROOMS];
};

struct ProtocolModel {
  SystemState systemState = DISARMED;
  KeypadState keypad = NO_INPUT;
  bool        networkStatus = false;   // STATUS field
  String      macAddress = "";         // ADRRESS field
  FloorNode   floors[SMP_MAX_FLOORS];
};

// -------- Global Model --------
extern ProtocolModel MODEL;

// -------- Add functions --------
bool addFloor(int floorId);
bool addRoom(int floorId, int roomId);
bool addUltrasonic(int floorId, int roomId, int uId);
bool addHall(int floorId, int roomId, int hsId);

// -------- Setters --------
bool setSystemState(SystemState state);
bool setSystemKeypad(KeypadState key);
bool setNetworkStatus(bool connected);
bool setNetworkMac(const String& mac);
bool setFloorConnection(int floorId, bool connected);
bool setRoomConnection(int floorId, int roomId, bool connected);
bool setUltrasonicState(int floorId, int roomId, int uId, bool state);
bool setHallState(int floorId, int roomId, int hsId, bool state);

// -------- Parse incoming messages --------
bool parseMqtt(const char* topicC, const char* payloadC);

// -------- Build JSON snapshot --------
String buildSnapshot();

// -------- Reset model --------
void resetModel();

// -------- Topic Builders --------
String topicSystemState();
String topicSystemKeypad();
String topicNetworkStatus();
String topicNetworkMac();
String topicFloorId(int flr_id);
String topicFloorConnection(int flr_id);
String topicRoomId(int flr_id,int rm_id);
String topicRoomConnection(int flr_id,int rm_id);
String topicUltrasonic(int flr_id,int rm_id,int u_id);
String topicHall(int flr_id,int rm_id,int hs_id);

#endif
