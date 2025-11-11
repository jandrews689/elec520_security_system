#ifndef BASE_STATION_SYSTEM
#define BASE_STATION_SYSTEM

    /* 
            1) Check sensor data against limits and system state. If ARMED and exceed limit then trigger event. 
            2) Check for trigger events 
                2) Location ID should be published to cloud during trigger event. 
                3) Trigger event causes alarm buzzer and LED to be set off. Maybe configure certain pins on esp32 for buzzer and led. 
            4) configure certain pins for keypad connection as wel. 
        5) MMQT messaging with the cloud (not part of this class)
        6) System setup and configuration. Place into configuration file which all nodes read from to build system.
        7) keypad data entry checking of user passwords and access to system state. 
        8) Store password and user into local database. 
    */





#endif