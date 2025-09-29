#ifndef Json_formatting.h
#define Json_formatting.h

#include <Arduino.h>
#include <ArduinoJson.h>

// -------- Limits (override before including if you want) --------
#ifndef SMP_MAX_FLOORS
#define SMP_MAX_FLOORS   8
#endif
#ifndef SMP_MAX_ROOMS
#define SMP_MAX_ROOMS    8
#endif
#ifndef SMP_MAX_SENSORS
#define SMP_MAX_SENSORS  8
#endif

// -------- Enums aligned with your data dictionary --------
enum SystemState { DISARMED=0, ARMED=1, ALARM=2, OTHER=3 };
enum KeypadState { NO_INPUT=0, PASS_ACCEPTED=1, PASS_DECLINED=2 };

// -------- Data model (fixed-size, simple C structs) --------
struct UltraSensor {
  bool used = false;
  int  id   = -1;     // U_ID
  bool state= false;  // U_STATE
};

struct HallSensor {
  bool used = false;
  int  id   = -1;     // HS_ID
  bool state= false;  // HS_STATE
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
  // System / BaseStation
  SystemState systemState = DISARMED;
  bool        baseConnected = false;
  KeypadState keypad = NO_INPUT;

  // Floors
  FloorNode floors[SMP_MAX_FLOORS];
};

// -------- Global model store (defined in .cpp) --------
extern ProtocolModel MODEL;

// -------- API you asked for --------

// 1) add floors
bool addFloor(int floorId);

// 2) add rooms (nested under a floor)
bool addRoom(int floorId, int roomId);

// 3) add ultrasonic sensors (nested under room)
bool addUltrasonic(int floorId, int roomId, int uId);

// 4) add hall sensors (nested under room)
bool addHall(int floorId, int roomId, int hsId);

// 5) parse all data (pass PubSubClient topic/payload here)
bool parseMqtt(const char* topicC, const char* payloadC);

// 6) concatenate all data (snapshot JSON)
String buildSnapshot();

// Optional: reset everything to defaults
void resetModel();

#endif