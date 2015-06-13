#include <MySensor.h>
#include <SensorNode.h>
#include <Sensors.h>

// Need to pull in these headers to force linking against respective libs
#include <SPI.h>
#include <DHT.h>

// Create the Node instance
Node node;

// Create sensor instances
HeartBeatSensor hb;
DHT22Sensor dht;

// Must provide this callback function to process messages. 
void incomingMessage(const MyMessage &msg) {
  node.processIncoming(msg);
}

void setup() {
  node.begin(incomingMessage, 100);
  node.sendSketchInfo("SensorNode", "1.0");
  node.addSensor(&hb);
  node.addSensor(&dht);
  node.presentAll();
}

void loop() {
  node.update();
}
