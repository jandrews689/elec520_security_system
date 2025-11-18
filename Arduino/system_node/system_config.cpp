 /*******************************************************************************************
* Project:      ELEC520 - Distributed and Interactive Systems Coursework - Security System
* File:         system_config
* Description:  FloorNode_ESP32 main file.
*
* Authors:      Joseph Andrews
* Created:      November 2025
*
* Notes:
*  - This file is part of the ELEC520 coursework project.
*******************************************************************************************/

#include <Arduino.h>
#include "system_config.h"
#include <ArduinoJson.h>
#include <FS.h>
#include <SPIFFS.h>
#include <elec520_protocol.h>


bool loadSystemConfig(SystemConfig &config) {

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount FAILED!");
        return false;
    }
    Serial.println("SPIFFS mounted OK");

    //list the file.
    listSPIFFS();


    //Serial Peripheral Interface Flash File System
    //Opens the config file in read mode and stores in flash.

    File file = SPIFFS.open("/system_config.json", "r");
    if (!file) {
        Serial.println("Failed to open system_config.json file!");
        return false;
    }

    //Creates static object, and parses json messages.
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.print("Deserialization FAILED: ");
        Serial.println(error.c_str());
        return false;
    }
    Serial.println("Deserialization successful!");

    //top level system object.
    JsonObject sys = doc["system"];
    config.numberOfFloors = sys["numFloors"];

    //construct the system from the json file.
    for (JsonObject floorObj : sys["floors"].as<JsonArray>()) {
        FloorConfig f;
        f.id = floorObj["id"];

        for (JsonObject roomObj : floorObj["rooms"].as<JsonArray>()) {
            RoomConfig r;
            r.id = roomObj["id"];

            //Storing the sensor data into the struct using brace initialisation.
            for (JsonObject u : roomObj["ultra"].as<JsonArray>()) r.ultraSensors.push_back({u["id"], u["threshold"]});
            for (JsonObject h : roomObj["hall"].as<JsonArray>()) r.hallSensors.push_back({h["id"], h["threshold"]});

            f.rooms.push_back(r);
        }

        config.floors.push_back(f);
    }
    return true;
}


//Config the system based on the data read from the json file.
bool configureSystemConfig(const SystemConfig &config) {
    setNumOfFloors(config.numberOfFloors);

    for (const FloorConfig &floor : config.floors) {
        addFloor(floor.id);

        for (const RoomConfig &room : floor.rooms) {
            addRoom(floor.id, room.id);

            for (const SensorConfig &ultra : room.ultraSensors) {
                addUltra(floor.id, room.id, ultra.id);
            }

            for (const SensorConfig &hall : room.hallSensors) {
                addHall(floor.id, room.id, hall.id);
            }
        }
    }
    return true;
}


void listSPIFFS() {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.print("File: ");
        Serial.println(file.name());
        file = root.openNextFile();
    }
}