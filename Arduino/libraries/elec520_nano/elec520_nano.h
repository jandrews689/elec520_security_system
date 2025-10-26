#ifndef ELEC520_NANO_H
#define ELEC520_NANO_H

#include <Arduino.h>

// ---------- Topic builders (Nano) ----------
String nodeTopicRoomConnection(uint8_t f_id, uint8_t r_id);               // f/{f}/r/{r}/cs
String nodeTopicUltra        (uint8_t f_id, uint8_t r_id, uint8_t u_id);  // f/{f}/r/{r}/u/{u}
String nodeTopicHall         (uint8_t f_id, uint8_t r_id, uint8_t hs_id); // f/{f}/r/{r}/h/{h}

// ---------- Combined builder+setter (returns "topic:value") ----------
String nanoTokenRoomConnection(uint8_t f_id, uint8_t r_id, bool connected01);         // "f/..../cs:0|1"
String nanoTokenUltra        (uint8_t f_id, uint8_t r_id, uint8_t u_id, uint8_t val); // "f/..../u/x:0..255"
String nanoTokenHall         (uint8_t f_id, uint8_t r_id, uint8_t hs_id, bool open01);// "f/..../h/x:0|1"


#endif // ELEC520_NANO_H