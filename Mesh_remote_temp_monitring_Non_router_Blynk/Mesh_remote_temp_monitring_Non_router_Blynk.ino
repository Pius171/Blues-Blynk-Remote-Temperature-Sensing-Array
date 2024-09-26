//************************************************************
// this is a simple example that uses the painlessMesh library
//
// 1. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 2. prints anything it receives to debug
//
//
//************************************************************
#include "painlessMesh.h"
#include "FS.h"

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

#define DEBUG 0

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugf(...) Serial.printf(__VA_ARGS__) // Use variadic macros to allow multiple arguments

#else
#define debug(x)
#define debugln(x)
#define debugf(...)
#endif


Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

bool RouterExists = false;
uint32_t router=0;
const String DEVICE_NAME = "Room 200"; // should be different for each device and should match your blynk datastream.
const int INTERVAL = 5; // in seconds

// User stub
void sendMessage() ; // Prototype so PlatformIO doesn't complain
String readFile(String path);
void writeFile(const char * path, String message);

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void sendMessage() {
  int temperature= random(32);
  char msg[50];
  sprintf(msg, "%02d%s",temperature,DEVICE_NAME.c_str());

  //mesh.sendBroadcast( temperature );
  if(RouterExists){
  if(!mesh.sendSingle(router,String(msg))){
debugf("failed to send %s",msg);
  }
  debug("router: ");
  debugln(router);
  taskSendMessage.setInterval( TASK_SECOND * INTERVAL);

  }
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
  debugf("startHere: Received from %u msg=%s\n", from, msg.c_str());
  if(!RouterExists){
    debugln("no router, finding router");
   router=from;
     debug("router: ");
  debugln(router);
   RouterExists=true;
  }

}

void newConnectionCallback(uint32_t nodeId) {
    debugf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  debugf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    debugf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void setup() {
  Serial.begin(115200);
#if DEBUG == 1
  mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
#else
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages
#endif

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();
}

void loop() {
  // it will run the user scheduler as well
  mesh.update();
}
