#include <MySensor.h>
#include <SensorNode.h>
#include <Sensors.h>

// Need to pull in these headers to force linking against respective libs
#include <SPI.h>

// Define the Node instance
Node node;
#define NODE_NAME "PathLightSlave"
#define NODE_VERSION "1.0"
#define NODE_IS_REPEATER false
//#define NODE_ID AUTO
#define NODE_ID 115
#define NODE_PARENT AUTO
//#define NODE_PARENT 100

// Forward reference
bool requestNeighborState();

// Define sensor instances
PresentationMetaSensor presentation(&node);
HeartBeatSensor hb(&node, AUTO, 30000UL);
LEDLight led(&node, 5);
PeriodicCallback stateQuery(&node, 1000, requestNeighborState);

// Add sensors to this table to have them automatically registered 
Sensor *sensors[] = {
  &presentation,
  &hb,
  &led,
  &stateQuery,
};

#define REQUEST_TIMEOUT_MS 50
#define OFF_DELAY 15000UL
#define STAGGER_DELAY 2000UL
#define LIGHT_OFF -1UL
unsigned long request_sent_at = 0UL;
unsigned long neighbor_on_time = LIGHT_OFF;
unsigned long my_on_time = LIGHT_OFF;

bool requestNeighborState() {
  // TODO: Clean up child id and other node id
  Serial.println("Requesting state from neighbor node.");
  node.request(1 /* presence child id */, V_VAR1, 110 /* pathlight id */);
  request_sent_at = millis();

  return false; // no need to call report() callback
}

bool sleepSetup() {
  if (millis() - request_sent_at < REQUEST_TIMEOUT_MS) {
    // still waiting for the response. don't go to sleep.
    return false;
  } else if (neighbor_on_time != LIGHT_OFF) {
    // neighbor is still on. we should be on, too. 
    // (note: We might not be on yet, but will be soon.) 
    return false;
  } else if (led.getState() == true) {
    // don't sleep while the light is on
    return false;
  }
  
  // go to sleep
  return true;
}

void processRules() {
  if (led.getState() == false && millis() >= my_on_time) {
    // turn on
    Serial.println("Lights on!");
    led.setState(true);
  } else if (led.getState() == true && millis() >= my_on_time + OFF_DELAY) {
    // turn off
    Serial.println("Lights off!");
    led.setState(false);
    my_on_time = LIGHT_OFF;
  }
}

//
// Most of the time the following won't need any adjustment.
//

#define NUM_SENSORS (sizeof(sensors) / sizeof(sensors[0]))

// Must provide this callback function to process messages. 
void incomingMessage(const MyMessage &msg) {
  if (mGetCommand(msg) == C_SET && msg.type == V_VAR1 && msg.sender == 110) {
    // we got a response
    neighbor_on_time = msg.getULong();
    Serial.print("Got response from neighbor: ");
    if (neighbor_on_time == LIGHT_OFF) {
      Serial.println("lights are off.");
    } else {
      Serial.print("lights went on ");
      Serial.print(neighbor_on_time);
      Serial.println(" ms ago.");
    }

    // If we're currently off, then figure out when we should go on. 
    if (neighbor_on_time != LIGHT_OFF) {
      // Set or update the "on" time to stay in the sync with the neighbor.
      // This calculation may result in a time in the past, but that just 
      // means we'll turn on right away.      
      my_on_time = millis() + STAGGER_DELAY - neighbor_on_time;
    }
  } else {
    // not ours -- pass to the Node
    node.processIncoming(msg);
  }
}

void setup() {
  // For this node, we need special logic before it goes to sleep
  node.setSleepCallback(sleepSetup);
  
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

