#include <Arduino.h>
//add lib here
// ---------- Tiny assert helpers ----------
static int __tests = 0, __fails = 0;
#define ASSERT_TRUE(expr)  do{__tests++; if(!(expr)){__fails++; Serial.printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);} }while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ_U8(exp, act) do{ __tests++; uint8_t _e=(uint8_t)(exp), _a=(uint8_t)(act); if(_e!=_a){__fails++; Serial.printf("FAIL %s:%d: expected %u, got %u (%s)\n", __FILE__, __LINE__, _e, _a, #act);} }while(0)
#define ASSERT_EQ_BOOL(exp, act) do{ __tests++; bool _e=(bool)(exp), _a=(bool)(act); if(_e!=_a){__fails++; Serial.printf("FAIL %s:%d: expected %s, got %s (%s)\n", __FILE__, __LINE__, _e?"true":"false", _a?"true":"false", #act);} }while(0)
#define ASSERT_STREQ(exp, act) do{ __tests++; String _e=(exp), _a=(act); if(_e!=_a){__fails++; Serial.printf("FAIL %s:%d: expected \"%s\", got \"%s\" (%s)\n", __FILE__, __LINE__, _e.c_str(), _a.c_str(), #act);} }while(0)

// ---------- Local peek helpers ----------
static FloorNode* T_findFloor(uint8_t id){
  for(int i=0;i<SMP_MAX_FLOORS;i++) if(MODEL.floors[i].used && MODEL.floors[i].floorId==id) return &MODEL.floors[i];
  return nullptr;
}
static RoomNode* T_findRoom(uint8_t fId,uint8_t rId){
  FloorNode* f=T_findFloor(fId); if(!f) return nullptr;
  for(int i=0;i<SMP_MAX_ROOMS;i++) if(f->rooms[i].used && f->rooms[i].roomId==rId) return &f->rooms[i];
  return nullptr;
}
static UltraSensor* T_findUltra(uint8_t fId,uint8_t rId,uint8_t uId){
  RoomNode* r=T_findRoom(fId,rId); if(!r) return nullptr;
  for(int i=0;i<SMP_MAX_SENSORS;i++) if(r->ultra[i].used && r->ultra[i].id==uId) return &r->ultra[i];
  return nullptr;
}
static HallSensor* T_findHall(uint8_t fId,uint8_t rId,uint8_t hId){
  RoomNode* r=T_findRoom(fId,rId); if(!r) return nullptr;
  for(int i=0;i<SMP_MAX_SENSORS;i++) if(r->hall[i].used && r->hall[i].id==hId) return &r->hall[i];
  return nullptr;
}

// ---------- Tests ----------
static void test_topics_cloud_and_node() {
  // Cloud
  ASSERT_STREQ("ELEC520/security/s/st",  topicSystemState());
  ASSERT_STREQ("ELEC520/security/s/ke",  topicKeypad());
  ASSERT_STREQ("ELEC520/security/n/st",  topicNetwork());
  ASSERT_STREQ("ELEC520/security/n/mc",  topicMac());
  ASSERT_STREQ("ELEC520/security/f/2/id",  topicFloorIdByte(2));
  ASSERT_STREQ("ELEC520/security/f/2/cs",  topicFloorConnection(2));
  ASSERT_STREQ("ELEC520/security/f/2/r/9/id", topicRoomIdByte(2,9));
  ASSERT_STREQ("ELEC520/security/f/2/r/9/cs", topicRoomConnection(2,9));
  ASSERT_STREQ("ELEC520/security/f/2/r/9/u/1", topicUltra(2,9,1));
  ASSERT_STREQ("ELEC520/security/f/2/r/9/h/1", topicHall(2,9,1));

  // Node
  ASSERT_STREQ("s/st",  nodeTopicSystemState());
  ASSERT_STREQ("s/ke",  nodeTopicKeypad());
  ASSERT_STREQ("n/st",  nodeTopicNetwork());
  ASSERT_STREQ("n/mc",  nodeTopicMac());
  ASSERT_STREQ("f/3/id",  nodeTopicFloorIdByte(3));
  ASSERT_STREQ("f/3/cs",  nodeTopicFloorConnection(3));
  ASSERT_STREQ("f/3/r/7/id", nodeTopicRoomIdByte(3,7));
  ASSERT_STREQ("f/3/r/7/cs", nodeTopicRoomConnection(3,7));
  ASSERT_STREQ("f/3/r/7/u/0", nodeTopicUltra(3,7,0));
  ASSERT_STREQ("f/3/r/7/h/0", nodeTopicHall(3,7,0));
}

static void test_parse_cloud_minimal() {
  resetModel();

  // System + keypad + network (BYTEENUM payloads)
  ASSERT_TRUE(parseCloud(topicSystemState().c_str(), "1"));   // ARMED
  ASSERT_TRUE(parseCloud(topicKeypad().c_str(),      "2"));   // DECLINED
  ASSERT_TRUE(parseCloud(topicNetwork().c_str(),     "1"));   // e.g., CONNECTED=1
  ASSERT_TRUE(parseCloud(topicMac().c_str(), "AA:BB:CC:DD:EE:FF"));

  ASSERT_EQ_U8(1, MODEL.systemState);
  ASSERT_EQ_U8(2, MODEL.keypad);
  ASSERT_EQ_U8(1, MODEL.network);
  ASSERT_STREQ("AA:BB:CC:DD:EE:FF", MODEL.mac);

  // Floor & Room id bytes + connections
  ASSERT_TRUE(parseCloud(topicFloorIdByte(1).c_str(), "5"));
  ASSERT_TRUE(parseCloud(topicFloorConnection(1).c_str(), "true"));
  ASSERT_TRUE(parseCloud(topicRoomIdByte(1,101).c_str(), "9"));
  ASSERT_TRUE(parseCloud(topicRoomConnection(1,101).c_str(), "false"));

  FloorNode* f = T_findFloor(1); ASSERT_TRUE(f);
  ASSERT_EQ_U8(5, f->idByte);    ASSERT_EQ_BOOL(true, f->connected);
  RoomNode* r = T_findRoom(1,101); ASSERT_TRUE(r);
  ASSERT_EQ_U8(9, r->idByte);    ASSERT_EQ_BOOL(false, r->connected);

  // Ultra (BYTE) & Hall (BOOL)
  ASSERT_TRUE(parseCloud(topicUltra(1,101,2).c_str(), "128"));
  ASSERT_TRUE(parseCloud(topicHall(1,101,3).c_str(),  "true"));

  UltraSensor* u = T_findUltra(1,101,2); ASSERT_TRUE(u); ASSERT_EQ_U8(128, u->value);
  HallSensor*  h = T_findHall(1,101,3);  ASSERT_TRUE(h); ASSERT_EQ_BOOL(true, h->open);
}

static void test_parse_node_minimal() {
  resetModel();

  ASSERT_TRUE(parseNode("s/st", "2"));       // ALARM
  ASSERT_TRUE(parseNode("s/ke", "1"));       // ACCEPTED
  ASSERT_TRUE(parseNode("n/st", "0"));       // e.g., DISCONNECT=0
  ASSERT_TRUE(parseNode("n/mc", "11:22:33:44:55:66"));

  ASSERT_EQ_U8(2, MODEL.systemState);
  ASSERT_EQ_U8(1, MODEL.keypad);
  ASSERT_EQ_U8(0, MODEL.network);
  ASSERT_STREQ("11:22:33:44:55:66", MODEL.mac);

  ASSERT_TRUE(parseNode("f/4/id",  "7"));
  ASSERT_TRUE(parseNode("f/4/cs",  "false"));
  ASSERT_TRUE(parseNode("f/4/r/3/id", "12"));
  ASSERT_TRUE(parseNode("f/4/r/3/cs", "true"));
  ASSERT_TRUE(parseNode("f/4/r/3/u/1", "200"));
  ASSERT_TRUE(parseNode("f/4/r/3/h/2", "false"));

  FloorNode* f = T_findFloor(4); ASSERT_TRUE(f);
  ASSERT_EQ_U8(7, f->idByte);    ASSERT_EQ_BOOL(false, f->connected);
  RoomNode*  r = T_findRoom(4,3); ASSERT_TRUE(r);
  ASSERT_EQ_U8(12, r->idByte);   ASSERT_EQ_BOOL(true, r->connected);
  UltraSensor* u = T_findUltra(4,3,1); ASSERT_TRUE(u); ASSERT_EQ_U8(200, u->value);
  HallSensor*  h = T_findHall(4,3,2);  ASSERT_TRUE(h); ASSERT_EQ_BOOL(false, h->open);
}

static void test_failures_are_strict() {
  resetModel();

  // Wrong cloud header -> reject
  ASSERT_FALSE(parseCloud("FOO/security/s/st", "1"));

  // Out of range byte -> reject & no change
  ASSERT_TRUE(parseNode("s/st", "1"));
  ASSERT_FALSE(parseNode("s/st", "300"));   // invalid
  ASSERT_EQ_U8(1, MODEL.systemState);       // unchanged

  // Bool must be "true"/"false" only
  ASSERT_FALSE(parseNode("f/1/cs", "1"));
  ASSERT_FALSE(parseNode("f/1/cs", "0"));
  ASSERT_TRUE(parseNode("f/1/cs", "true"));
  ASSERT_EQ_BOOL(true, T_findFloor(1)->connected);

  // Empty payload -> reject
  ASSERT_FALSE(parseNode("n/st", ""));
}

static void test_idempotent_adders() {
  resetModel();

  // Re-setting same items shouldn't break or duplicate
  ASSERT_TRUE(parseNode("f/2/id", "5"));
  ASSERT_TRUE(parseNode("f/2/id", "5"));
  ASSERT_TRUE(parseNode("f/2/r/9/id", "7"));
  ASSERT_TRUE(parseNode("f/2/r/9/id", "7"));
  ASSERT_TRUE(parseNode("f/2/r/9/u/0", "10"));
  ASSERT_TRUE(parseNode("f/2/r/9/u/0", "10"));
  ASSERT_TRUE(parseNode("f/2/r/9/h/0", "true"));
  ASSERT_TRUE(parseNode("f/2/r/9/h/0", "true"));

  FloorNode* f = T_findFloor(2); ASSERT_TRUE(f); ASSERT_EQ_U8(5, f->idByte);
  RoomNode*  r = T_findRoom(2,9); ASSERT_TRUE(r); ASSERT_EQ_U8(7, r->idByte);
  UltraSensor* u = T_findUltra(2,9,0); ASSERT_TRUE(u); ASSERT_EQ_U8(10, u->value);
  HallSensor*  h = T_findHall(2,9,0);  ASSERT_TRUE(h); ASSERT_EQ_BOOL(true, h->open);
}

static void run_all() {
  test_topics_cloud_and_node();
  test_parse_cloud_minimal();
  test_parse_node_minimal();
  test_failures_are_strict();
  test_idempotent_adders();

  Serial.println();
  Serial.printf("======== TEST SUMMARY ========\n");
  Serial.printf("Total: %d | Fails: %d | Pass: %d\n", __tests, __fails, __tests-__fails);
  Serial.println("==============================");
}

void setup() {
  Serial.begin(115200);
  while(!Serial) {}
  delay(300);
  Serial.println("\njson_formatting (lean) – test runner\n");
  run_all();
}

void loop() {
  // idle
  delay(1000);
}
