// -------- esp32_floor/include/i2c_roombus.h --------
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"


// Minimal I²C helper that treats each Nano as a text endpoint.
// The Nano should respond with a single ASCII line (<= I2C_RX_MAX bytes)
// representing one room snapshot in the compact protocol format, e.g.:
// "f/0/r/1/cs:1;u/0:87;h/1:0

class RoomBusI2C{
public:
void begin() {}
// Request a line from a given I²C address. Returns number of bytes read (0 if none).
int readLine(uint8_t addr, uint8_t* out, size_t outMax,
size_t requestLen, uint16_t /*settleMs*/){
if (!addr) return 0;
Wire.requestFrom((int)addr, (int)requestLen);
int n=0; while (Wire.available() && n<(int)outMax){ out[n++] = (uint8_t)Wire.read(); }
return n; // may be 0 if that Nano is offline
}
};