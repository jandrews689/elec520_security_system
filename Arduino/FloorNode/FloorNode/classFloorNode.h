#ifndef CLASS_FLOOR_NODE
#define CLASS_FLOOR_NODE

#include <cstdint>

class classFloorNode {
    private:
        int _iFloorNodeID;
        uint64_t _uiMacAddress;

    public:
        classFloorNode(){};

        void setup(int iID){
            _iFloorNodeID = iID;
        };
        void begin(){};
        void update(){};

        void setMacAddress(uint64_t MacAddress){
            _uiMacAddress = MacAddress;
        }

};

#endif