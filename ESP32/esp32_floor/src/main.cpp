#include "classFloorController.h"

FloorController controller;

void setup() {
  Serial.begin(115200);
  delay(50);
  controller.begin();
}

void loop() {
  controller.loop();
}
