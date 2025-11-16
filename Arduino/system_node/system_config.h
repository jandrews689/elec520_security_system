//
// Created by jandr on 16/11/2025.
//

#ifndef SECURITY_SYSTEM_CLION_SYSTEM_CONFIG_H
#define SECURITY_SYSTEM_CLION_SYSTEM_CONFIG_H

#pragma once
#include <Arduino.h>
#include <vector>

//pio run --target uploadfs

// System Data Structures
struct SensorConfig {
    uint8_t id;
    int threshold;
};

struct RoomConfig {
    uint8_t id;
    std::vector<SensorConfig> hallSensors;
    std::vector<SensorConfig> ultraSensors;
};

struct FloorConfig {
    uint8_t id;
    std::vector<RoomConfig> rooms;
};

struct SystemConfig {
    uint8_t numberOfFloors;
    std::vector<FloorConfig> floors;
};


//Loads the json file and stores data into the above structs.
bool loadSystemConfig(SystemConfig &config);


//Config the system based on the data read from the json file.
bool configureSystemConfig(const SystemConfig &config);


void listSPIFFS();

#endif //SECURITY_SYSTEM_CLION_SYSTEM_CONFIG_H