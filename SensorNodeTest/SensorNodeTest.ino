#include <MySensor.h>
#include <SensorNode.h>
#include <Sensors.h>

// Need to pull in these headers to force linking against respective libs
#include <SPI.h>
#include <LedControl.h>
#include <RKLIcons.h>

// Define the Node instance
Node node;
#define NODE_NAME "DeskShelf"
#define NODE_VERSION "1.2"
#define NODE_IS_REPEATER true
//#define NODE_ID AUTO
#define NODE_ID 100

// Define sensor instances
PresentationMetaSensor presentation(&node);
//HeartBeatSensor hb(&node, AUTO, 30000);
PresenceSensor presence(&node, AUTO, 3, 15000UL);
LEDLight led(&node, 5);
// The LED matrix code adds a lot of RAM program space, and as such needs to be compiled with 
// DEBUG turned OFF in MyConfig.h (less that 20% RAM free is bad news) 
SimpleIconLedMatrix matrix(&node);

// Add sensors to this table to have them automatically registered 
Sensor *sensors[] = {
  &presentation,
  //&hb,
  &presence,
  &led,
  &matrix,
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
  node.begin(incomingMessage, NODE_ID, NODE_IS_REPEATER);
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

