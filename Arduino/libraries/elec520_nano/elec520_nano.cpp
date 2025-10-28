#include "elec520_nano.h"

// ----- Topic builders -----
String nodeTopicRoomConnection(uint8_t f_id, uint8_t r_id) {
  String t = "f/"; t += f_id; t += "/r/"; t += r_id; t += "/cs";
  return t;
}
String nodeTopicUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id) {
  String t = "f/"; t += f_id; t += "/r/"; t += r_id; t += "/u/"; t += u_id;
  return t;
}
String nodeTopicHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id) {
  String t = "f/"; t += f_id; t += "/r/"; t += r_id; t += "/h/"; t += hs_id;
  return t;
}

// ----- Combined: "topic:value" -----
static String _pair(const String& topic, const String& value) {
  String out; out.reserve(topic.length()+1+value.length());
  out += topic; out += ':'; out += value; 
  return out;
}

String nanoTokenRoomConnection(uint8_t f_id, uint8_t r_id, bool connected01) {
  return _pair(nodeTopicRoomConnection(f_id, r_id), connected01 ? "1" : "0");
}
String nanoTokenUltra(uint8_t f_id, uint8_t r_id, uint8_t u_id, uint8_t val) {
  return _pair(nodeTopicUltra(f_id, r_id, u_id), String(val));
}
String nanoTokenHall(uint8_t f_id, uint8_t r_id, uint8_t hs_id, bool open01) {
  return _pair(nodeTopicHall(f_id, r_id, hs_id), open01 ? "1" : "0");
}