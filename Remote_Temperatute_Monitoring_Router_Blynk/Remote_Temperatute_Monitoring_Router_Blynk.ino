#include <Notecard.h>
#include "painlessMesh.h"
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into IO4 on the ESP32
#define ONE_WIRE_BUS 4

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

#define usbSerial Serial

// This is the unique Product Identifier for your device.  This Product ID tells
// the Notecard what type of device has embedded the Notecard, and by extension
// which vendor or customer is in charge of "managing" it.  In order to set this
// value, you must first register with notehub.io and "claim" a unique product
// ID for your device.  It could be something as simple as as your email address
// in reverse, such as "com.gmail.smith.lisa:test-device" or
// "com.outlook.gates.bill.demo"

// This is the unique Product Identifier for your device
#ifndef PRODUCT_UID
#define PRODUCT_UID "com.gmail.onyemandukwu:temperature_monitor"  // change to your own PRODUCT UID
#pragma message "PRODUCT_UID is not defined in this example. Please ensure your Notecard has a product identifier set before running this example or define it in code here. More details at https://dev.blues.io/tools-and-sdks/samples/product-uid"
#endif

#define myProductID PRODUCT_UID

/* The cellular notecard by default uses its embedded
*  SIM which comes with 500MB and 10 years worth of 
*  cellular service, but is not available in all regions
*  If it is not available in your region you can use an 
* external SIM card.
*/
#define USING_EXTERNAL_SIM true  // chnange to false if using the embedded SIM card

#define ATTN_PIN 18
#define LED 16
#define SDA 19
#define SDL 21
#define MESH_PREFIX "whateverYouLike"
#define MESH_PASSWORD "somethingSneaky"
#define MESH_PORT 5555
#define HIDDEN true
#define CHANNEL 1
#define MAX_DEVICES 26  // max number of devices

Scheduler userScheduler;  // to control your personal task
painlessMesh mesh;
Notecard notecard;

std::map<String, int> tempHashMap;
std::map<uint32_t, bool> IDmap;
// User stub
void sendMessage();
void sendRouterTemp();

Task taskSendMessage(TASK_SECOND * 1, TASK_FOREVER, &sendMessage);

void sendMessage() {
  String msg = String(mesh.getNodeId());
  mesh.sendBroadcast(msg);  // sends its id to non router devices, so they can know where to send thier temp reading to
  taskSendMessage.setInterval(TASK_SECOND * 1);
}


// Needed for painless library
void receivedCallback(uint32_t from, String &msg) {

  debugf("startHere: Received from %u msg=%s\n", from, msg.c_str());
  char *deviceName = nullptr;  // pointer to hold device name

  try {
    bool deviceNamed = IDmap.at(from);
    if (!deviceNamed) {
      J *req = notecard.newRequest("env.get");
      JAddStringToObject(req, "name", String(from).c_str());
      J *rsp = notecard.requestAndResponse(req);
      debugln(JConvertToJSONString(rsp));

      deviceName = JGetString(rsp, "text");

      if (deviceName == "") {
        // ID does not exist in environmental variable
        debugf("\n%u does not exist in env\n", from);
        deviceName = strdup(String(from).c_str());

      } else {
        // device has gotten its name from environmental variable
        IDmap[from] = true;
      }
      debugln(deviceName);
    } else {
      // if the device is named remove its duplicate from the
      // tempHashMap
      tempHashMap.erase(String(from));
    }
  } catch (const std::out_of_range &e) {
    // id does not exist, adding to map
    debugf("%u does not exist in IDmap, adding to map\n", from);
    IDmap[from] = false;
  }


  int temperature = msg.toInt();
  tempHashMap[deviceName] = temperature;

  debug("getting router temperature...");
  sensors.requestTemperatures();  // Send the command to get temperatures
  debugln("DONE");

  int routerTempC = (int)sensors.getTempCByIndex(0);
  tempHashMap["Router"] = routerTempC;  // add router temperature to hashmap

  if (digitalRead(ATTN_PIN)) {
    debugln("blynk.qi has arrived. ATTN pin fired");


    J *req = notecard.newRequest("note.add");
    if (req != NULL) {
      JAddStringToObject(req, "file", "temperature.qo");
      JAddBoolToObject(req, "sync", true);
      J *body = JAddObjectToObject(req, "body");
      if (body != NULL) {
        //iterate over key values pair in map
        for (const auto &entry : tempHashMap) {
          JAddNumberToObject(body, entry.first.c_str(), entry.second);
        }
      }
      notecard.sendRequest(req);
    }

    //rearm
    debugln("rearming ATTN PIN.");
    req = NoteNewRequest("card.attn");
    JAddStringToObject(req, "mode", "arm");  // make the attn pin always HIGH at setup.
    notecard.sendRequestWithRetry(req, 5);   // 5 secondsd
    debugln("ATTN PIN is armed(LOW)");
  }
  free(deviceName);
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
  // Set up for debug output (if available).
  // If you open Arduino's serial terminal window, you'll be able to watch
  // JSON objects being transferred to and from the Notecard for each request.
  Wire.begin(SDA, SDL);  // SDA and SCL pins
  Serial.begin(115200);
  usbSerial.begin(115200);

  pinMode(ATTN_PIN, INPUT_PULLDOWN);
  pinMode(LED, OUTPUT);
  //digitalWrite(LED, 1);
#if DEBUG == 1
  mesh.setDebugMsgTypes(ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | STARTUP);  // all types on
#else
  mesh.setDebugMsgTypes(ERROR | STARTUP);  // set before init() so that you can see startup messages
#endif

  mesh.init(MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT, WIFI_AP_STA, CHANNEL, HIDDEN, MAX_DEVICES);
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  // Bridge node, should (in most cases) be a root node. See [the wiki](https://gitlab.com/painlessMesh/painlessMesh/wikis/Possible-challenges-in-mesh-formation) for some background
  mesh.setRoot(true);


  const size_t usb_timeout_ms = 3000;
  for (const size_t start_ms = millis(); !usbSerial && (millis() - start_ms) < usb_timeout_ms;)
    ;
  notecard.setDebugOutputStream(usbSerial);

  notecard.begin();


  // "newRequest()" uses the bundled "J" json package to allocate a "req",
  // which is a JSON object for the request to which we will then add Request
  // arguments.  The function allocates a "req" request structure using
  // malloc() and initializes its "req" field with the type of request.
  J *req = notecard.newRequest("hub.set");

  // This command (required) causes the data to be delivered to the Project
  // on notehub.io that has claimed this Product ID (see above).
  if (myProductID[0]) {
    JAddStringToObject(req, "product", myProductID);
  }

  // This command determines how often the Notecard connects to the service.
  // If "continuous", the Notecard immediately establishes a session with the
  // service at notehub.io, and keeps it active continuously. Due to the power
  // requirements of a continuous connection, a battery powered device would
  // instead only sample its sensors occasionally, and would only upload to
  // the service on a "periodic" basis.
  JAddStringToObject(req, "mode", "continuous");
  JAddNumberToObject(req, "inbound", 1);  // check notehub for any inbound notes every minute
  notecard.sendRequestWithRetry(req, 5);  // 5 seconds

  req = notecard.newRequest("card.attn");
  JAddStringToObject(req, "mode", "disarm");  // make the attn pin always HIGH at setup.
  notecard.sendRequestWithRetry(req, 5);      // 5 seconds

  //now arm it,
  req = notecard.newRequest("card.attn");
  JAddStringToObject(req, "mode", "arm, files");  // attn pin will now be low because I have armed it

  // when the blynk.qi file arrives it will make the attn pin HIGH
  J *files = JAddArrayToObject(req, "files");
  // for downstream from blynk, they are written to an inbound note blynk.qi
  JAddItemToArray(files, JCreateString("blynk.qi"));


  notecard.sendRequestWithRetry(req, 5);




#if USING_EXTERNAL_SIM
  req = notecard.newRequest("card.wireless");
  JAddStringToObject(
    req,
    "apn",
    "web.gprs.mtnnigeria.net"  // change this to your SIM access point name
  );


  /* set the mode you want your SIM to use
  *  for connectivity:
  *   "-" to reset to the default mode.
  *  "auto" to perform automatic band scan mode (this is the default mode).
  *  "m" to restrict the modem to Cat-M1.
  *  "nb" to restrict the modem to Cat-NB1.
  *  "gprs" to restrict the modem to EGPRS
  */
  JAddStringToObject(req, "mode", "gprs");  // only gprs worked in my region
#else
  req = NoteNewRequest("card.wireless");
  JAddStringToObject(
    req,
    "apn",
    "-"  // reverts to embedded SIM
  );


  /* set the mode you want your SIM to use
  *  for connectivity:
  *   "-" to reset to the default mode.
  *  "auto" to perform automatic band scan mode (this is the default mode).
  *  "m" to restrict the modem to Cat-M1.
  *  "nb" to restrict the modem to Cat-NB1.
  *  "gprs" to restrict the modem to EGPRS
  */
  JAddStringToObject(req, "mode", "-");  // reverts to embedded SIM
  notecard.sendRequestWithRetry(req, 5);
#endif
  userScheduler.addTask(taskSendMessage);
  taskSendMessage.enable();
}


void loop() {
  int mesh_size = mesh.getNodeList().size();
  if (mesh_size > 0) {
    // turn on LED to indicate there is a non router connected
    digitalWrite(LED, 0);  // led is active low
    //debugln(x)
  } else {
    digitalWrite(LED, 1);
  }
  mesh.update();
}
