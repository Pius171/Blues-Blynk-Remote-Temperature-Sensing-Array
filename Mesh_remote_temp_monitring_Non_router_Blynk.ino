#include "painlessMesh.h"
#include "FS.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <map>

// Data wire is plugged into port 2 on the Arduino
#define ONE_WIRE_BUS 4

//LED for debugging
#define LED 16

#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555

#define DEBUG 1

#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugf(...) Serial.printf(__VA_ARGS__)  // Use variadic macros to allow multiple arguments

#else
#define debug(x)
#define debugln(x)
#define debugf(...)
#endif

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

Scheduler userScheduler;  // to control your personal task
painlessMesh mesh;

bool RouterExists = false;
uint32_t router = 0;
const String DEVICE_NAME = "Room 200";  // should be different for each device and should match your blynk datastream.
const int INTERVAL = 5;                 // in seconds

// User stub
void sendMessage();  // Prototype so PlatformIO doesn't complain
String readFile(String path);
void writeFile(const char *path, String message);

Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage);

void sendMessage() {
  debug("getting temperature...");
  sensors.requestTemperatures();  // Send the command to get temperatures
  debugln("DONE");
  // After we got the temperatures, we can print them here.
  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  int tempC = (int)sensors.getTempCByIndex(0);
  char msg[50];
  sprintf(msg, "%02d%s", tempC, DEVICE_NAME.c_str());
  debugln(msg);


  if (RouterExists) {
digitalWrite(LED,1);// turn on LED
    if (!mesh.sendSingle(router, String(msg))) {
      debugf("failed to send %s", msg);
    }
    debug("router: ");
    debugln(router);
    taskSendMessage.setInterval(TASK_SECOND * INTERVAL);
  }
}

// Needed for painless library
void receivedCallback(uint32_t from, String &msg) {
  debugf("startHere: Received from %u msg=%s\n", from, msg.c_str());
  if (!RouterExists) {
    digitalWrite(LED, 0);  // turn off LED
    debugln("no router, finding router");
    router = from;
    debug("router: ");
    debugln(router);
    RouterExists = true;
  }
}

void newConnectionCallback(uint32_t nodeId) {
  debugf("--> startHere: New Connection, nodeId = %u\n", nodeId);
}

void changedConnectionCallback() {
  debugf("Changed connections\n");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  debugf("Adjusted time %u. Offset = %d\n", mesh.getNodeTime(), offset);
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
#if DEBUG == 1
  mesh.setDebugMsgTypes(ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE);  // all types on
#else
  mesh.setDebugMsgTypes(ERROR | STARTUP);  // set before init() so that you can see startup messages
#endif

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
  sensors.begin();
}

void loop() {
  // it will run the user scheduler as well
  mesh.update();
}
