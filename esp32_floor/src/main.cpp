// -------- esp32_floor/src/main.cpp --------
#include "classFloorController.h"

// Entry point kept intentionally tiny: it just spins the FloorController runtime.
FloorController controller;

void setup()
{
    Serial.begin(115200);
    delay(50);
    controller.begin();
}

void loop()
{
    controller.loop();
}