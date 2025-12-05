#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

class RoomBusI2C {
public:
  void begin() {
#if !SIMULATE_I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_CLOCK_HZ);
  Wire.setTimeOut(50);   // ms: fail fast if a Nano stalls the bus
#endif
}


  // Returns byte count; 0 = no frame this tick
  int readLine(uint8_t addr, uint8_t* out, size_t outMax,
               size_t requestLen, uint16_t settleMs);
};
