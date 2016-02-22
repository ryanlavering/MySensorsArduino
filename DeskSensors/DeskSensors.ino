#include <MySensor.h>
#include <SensorNode.h>
#include <Sensors.h>

// Need to pull in these headers to force linking against respective libs
#include <SPI.h>
#include <DHT.h>

// Define the Node instance
Node node;
#define NODE_NAME "DeskNode"
#define NODE_VERSION "1.0"

// Define sensor instances
PresentationMetaSensor pres(&node);
HeartBeatSensor hb(&node, AUTO, 5*60*1000L);
DHT22Sensor dht22(&node);

// Add sensors to this table to have them automatically registered 
Sensor *sensors[] = {
  &pres,
  &hb,
  &dht22,
};

//
// Most of the time the following won't need any adjustment.
//

#define NUM_SENSORS (sizeof(sensors) / sizeof(sensors[0]))

// Must provide this callback function to process messages. 
void incomingMessage(const MyMessage &msg) {
  node.processIncoming(msg);
}

void setup() {
  node.begin(incomingMessage, 101);
  node.sendSketchInfo(NODE_NAME, NODE_VERSION);
  for (int i = 0; i < NUM_SENSORS; i++) {
    node.addSensor(sensors[i]);
  }
  node.presentAll();
}

void loop() {
  node.update();
}

