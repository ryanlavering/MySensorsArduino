#include <MySensor.h>
#include <SensorNode.h>
#include <Sensors.h>

// Need to pull in these headers to force linking against respective libs
#include <SPI.h>
#include <DHT.h>
#include <LedControl.h>
#include <RKLIcons.h>

// Create the Node instance
Node node;

// Create sensor instances
PresetationMetaSensor pres(&node);
HeartBeatSensor hb(&node, AUTO, 30000);
SimpleIconLedMatrix led(&node);

// Must provide this callback function to process messages. 
void incomingMessage(const MyMessage &msg) {
  node.processIncoming(msg);
}

void setup() {
  node.begin(incomingMessage, 100);
  node.sendSketchInfo("SensorNode", "1.0");
  node.addSensor(&pres);
  node.addSensor(&hb);
  node.addSensor(&led);
  node.presentAll();
}

void loop() {
  node.update();
}
