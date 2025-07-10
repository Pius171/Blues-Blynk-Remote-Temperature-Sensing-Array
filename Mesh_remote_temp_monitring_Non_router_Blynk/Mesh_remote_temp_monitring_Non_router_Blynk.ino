//************************************************************
// this is a simple example that uses the painlessMesh library
//
// 1. sends a silly message to every node on the mesh at a random time between 1 and 5 seconds
// 2. prints anything it receives to Serial.print
//
//
//************************************************************
#include "painlessMesh.h"
#include "FS.h"
#include "LittleFS.h"

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;

bool RouterExists = false;
uint32_t router=0;

// User stub
void sendMessage() ; // Prototype so PlatformIO doesn't complain
String readFile(String path);
void writeFile(const char * path, String message);

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void sendMessage() {
  String temperature = String(random(32));
  //mesh.sendBroadcast( temperature );
  if(RouterExists){
  mesh.sendSingle(router,temperature);
  Serial.print("router: ");
  Serial.println(router);
  taskSendMessage.setInterval( random( TASK_SECOND * 1, TASK_SECOND * 5 ));

  }
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
  if(!RouterExists){
    Serial.println("no router finding router");
   router=from;
     Serial.print("router: ");
  Serial.println(router);
   writeFile("/masterID.txt",String(from));
   RouterExists=true;
  }

}

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  Serial.printf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(),offset);
}

void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount Failed");
    return;
  }

    if (LittleFS.exists("/masterID.txt")) {
    RouterExists= true;
    router= readFile("/masterID.txt").toInt();
    Serial.print("router: ");
    Serial.println(router);
 
  }
//mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE ); // all types on
  mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

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

void writeFile(const char * path, String message){
    Serial.printf("Writing file: %s\n", path);

    File file = LittleFS.open(path, "w");
    if(!file){
        Serial.println("Failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.close();
}

String readFile(String path) {
  String DoC = "";
  Serial.print("Reading file: %s\n");
  Serial.println(path);

  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return DoC;
  }

  Serial.print("Read from file: ");
  while (file.available()) {

    DoC += file.readString();
    Serial.print(DoC);
    //delay(500);
  }
  return DoC;
}
