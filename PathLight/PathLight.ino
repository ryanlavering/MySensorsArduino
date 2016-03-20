#include <MySensor.h>
#include <SensorNode.h>
#include <Sensors.h>

// Need to pull in these headers to force linking against respective libs
#include <SPI.h>

// Define the Node instance
Node node;
#define NODE_NAME "PathLight"
#define NODE_VERSION "1.4"
#define NODE_IS_REPEATER false
//#define NODE_ID AUTO
#define NODE_ID 110
#define NODE_PARENT AUTO
//#define NODE_PARENT 100

// Define sensor instances
PresentationMetaSensor presentation(&node);
HeartBeatSensor hb(&node, AUTO, 30000UL);
PresenceSensor presence(&node, AUTO, 3, 15000UL);
LEDLight led(&node, 5);

// Add sensors to this table to have them automatically registered 
Sensor *sensors[] = {
  &presentation,
  &hb,
  &presence,
  &led,
};

void processRules() {
  // Make sure that LED light status reflects the presence sensor state
  // Could also be implemented in the controller
  led.setState(presence.motionDetected());
}

//
// Most of the time the following won't need any adjustment.
//

#define NUM_SENSORS (sizeof(sensors) / sizeof(sensors[0]))

// Must provide this callback function to process messages. 
void incomingMessage(const MyMessage &msg) {
  node.processIncoming(msg);
}

void setup() {
  node.begin(incomingMessage, NODE_ID, NODE_IS_REPEATER, NODE_PARENT);
  node.sendSketchInfo(NODE_NAME, NODE_VERSION);
  for (int i = 0; i < NUM_SENSORS; i++) {
    node.addSensor(sensors[i]);
  }
  node.presentAll();
}

void loop() {
  node.update();
  processRules();
}

