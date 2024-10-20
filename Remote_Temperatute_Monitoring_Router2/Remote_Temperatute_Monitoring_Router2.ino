#include <Notecard.h>
#include "painlessMesh.h"

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
#define PRODUCT_UID "com.gmail.onyemandukwu:temperature_monitor" // "com.my-company.my-name:my-project"
#pragma message "PRODUCT_UID is not defined in this example. Please ensure your Notecard has a product identifier set before running this example or define it in code here. More details at https://dev.blues.io/tools-and-sdks/samples/product-uid"
#endif

#define myProductID PRODUCT_UID

#define   MESH_PREFIX     "whateverYouLike"
#define   MESH_PASSWORD   "somethingSneaky"
#define   MESH_PORT       5555

Scheduler userScheduler; // to control your personal task
painlessMesh  mesh;
Notecard notecard;

// User stub
void sendMessage() ; // Prototype so PlatformIO doesn't complain

Task taskSendMessage( TASK_SECOND * 1 , TASK_FOREVER, &sendMessage );

void sendMessage() {
  String msg = String(mesh.getNodeId());
  mesh.sendBroadcast( msg );
  taskSendMessage.setInterval( random( TASK_SECOND * 1, TASK_SECOND * 5 ));
}

// Needed for painless library
void receivedCallback( uint32_t from, String &msg ) {
  Serial.printf("startHere: Received from %u msg=%s\n", from, msg.c_str());
      
    J *req = notecard.newRequest("note.add");
    if (req != NULL)
    {
        JAddBoolToObject(req, "sync", true);
        J *body = JAddObjectToObject(req, "body");
        if (body != NULL)
        {
            JAddStringToObject(body, String(from).c_str(), msg.c_str());

        }
        notecard.sendRequest(req);
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
void setup()
{
    // Set up for debug output (if available).
    // If you open Arduino's serial terminal window, you'll be able to watch
    // JSON objects being transferred to and from the Notecard for each request.
    Wire.begin(16, 17);  // SDA and SCL pins
      Serial.begin(115200);
    usbSerial.begin(115200);
mesh.setDebugMsgTypes( ERROR | MESH_STATUS | CONNECTION | SYNC | COMMUNICATION | GENERAL | MSG_TYPES | REMOTE | STARTUP); // all types on
  //mesh.setDebugMsgTypes( ERROR | STARTUP );  // set before init() so that you can see startup messages

  mesh.init( MESH_PREFIX, MESH_PASSWORD, &userScheduler, MESH_PORT );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();

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
    if (myProductID[0])
    {
        JAddStringToObject(req, "product", myProductID);
    }

    // This command determines how often the Notecard connects to the service.
    // If "continuous", the Notecard immediately establishes a session with the
    // service at notehub.io, and keeps it active continuously. Due to the power
    // requirements of a continuous connection, a battery powered device would
    // instead only sample its sensors occasionally, and would only upload to
    // the service on a "periodic" basis.
    JAddStringToObject(req, "mode", "continuous");

    notecard.sendRequestWithRetry(req, 5); // 5 seconds
}


void loop()
{
    mesh.update();


 
}
