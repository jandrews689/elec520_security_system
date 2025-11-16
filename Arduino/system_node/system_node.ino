 /*******************************************************************************************
 * Project:      ELEC520 - Distributed and Interactive Systems Coursework - Security System
 * File:         system_node
 * Description:  FloorNode_ESP32 main file.
 *
 * Authors:      Joseph Andrews, Brendan Taylor, Charlie W
 * Created:      November 2025
 *
 * Notes:
 *  - This file is part of the ELEC520 coursework project.
 *  - Code created in collaboration with AI (ChatGPT 5).
 *******************************************************************************************/

#include <elec520_nano.h>
#include <elec520_protocol.h>
#include "securitySystemNetworkMQTT.h"
#include "baseStation.h"
#include "i2c.h"
#include <Arduino.h>
#include <SPIFFS.h>
#include <WString.h>

#include "system_config.h"


const char* ssid = "Joe's S23 Ultra";
const char* password = "joea12345"; 
const char* mqtt_server = "broker.hivemq.com"; 
int mqtt_port = 1883; 
const char* mqtt_client_id = "BaseStation";

securitySystemNetworkMQTT objFloor(ssid, password, mqtt_server, mqtt_port, mqtt_client_id);

int count = 0;
bool mqttSystemDebug = true;


//SETUP////////////////////////////////////////////////////////////////////////////////
void setup() {
  pinMode(23, OUTPUT); //Configures the Alarm LED and BUZZER as output
  Serial.begin(115200);
  delay(100);

  //System Config, Reads Json file to build system
    SystemConfig config;
    if (!loadSystemConfig(config)) {
        Serial.println("Error: Failed to load config");
    } else {
        Serial.println("Config loaded OK, applying to MODEL....");
        if (configureSystemConfig(config)) {
            Serial.println("System Configured!\n");
        }
    }



  //I2C SETUP//////////////////////////////////////////////////////////
  i2cSetup();

  //ESP setup///////////////////////////////////////////////////////////
  objFloor.setup_wifi();

}



//LOOP/////////////////////////////////////////////////////////////////////////////////////////
void loop() {

    //i2C////////////////////////////////////////////////////////////////////////////////
    i2cOperation();
    //ESP32/////////////////////////////////////////////////////////////////////////////


    //MQTT//////////////////////////////////////////////////////////////////////////////
    //if elected leader node then mqtt.
    objFloor.mqttOperate();


    //MQTT System Serial Print Debugging
    if (mqttSystemDebug) {
        count++;
        if (count > 1000) {
        count = 0;
        //Spew everything onto the serial.
        debugPrintModel(Serial);
        }
    }

    //SYSTEM STATE////////////////////////////////////////////////////////////////////////
    objFloor.alarmSystemStateMachine();

    //Trigger alarm
    objFloor.triggerAlarm();


}
