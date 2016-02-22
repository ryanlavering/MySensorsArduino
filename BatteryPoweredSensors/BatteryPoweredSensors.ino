// This is an example that demonstrates how to report the battery level for a sensor
// Instructions for measuring battery capacity on A0 are available in the follwoing forum
// thread: http://forum.micasaverde.com/index.php/topic,20078.0.html

#include <SPI.h>
#include <MySensor.h>

#define PRESENSE_SENSOR_PIN 3 // The digital input you attached your motion sensor.  (Only 2 and 3 generates interrupt!)
#define MOTION_INTERRUPT PRESENSE_SENSOR_PIN-2 // Usually the interrupt = pin -2 (on uno/nano anyway)
#define PRESENSE_SENSOR_WARM_UP (15*1000) // warm up time
bool warm_up_done = false;

// input pin for the battery sense point
#define BATTERY_SENSE_PIN A2
//unsigned long SLEEP_TIME = 900000;  // sleep time between reads (seconds * 1000 milliseconds)
unsigned long SLEEP_TIME = 120000;  // sleep time between reads (seconds * 1000 milliseconds)
int oldBatteryPcnt = 0;

#define CHILD_ID_PRESENSE 0

MySensor gw;
MyMessage msg(CHILD_ID_PRESENSE, V_TRIPPED); // motion message

void setup()
{
  // use the 3.3 V Vcc reference
  analogReference(DEFAULT);
  gw.begin();

  // Send the sketch version information to the gateway and Controller
  gw.sendSketchInfo("Battery Sensors", "1.3");

  // Register all sensors to gw (they will be created as child devices)
  gw.present(CHILD_ID_PRESENSE, S_MOTION);
}

void loop()
{
  // get the battery Voltage
  int sensorValue = analogRead(BATTERY_SENSE_PIN);

  // Direct connection to batteries (Nominal 1.6 V * 2 = 3.2V Max)
  // measured across 3.3 V (nom.) reference from buck converter
  // 3.3/1023 = Volts per bit = 0.003225806
  float batteryV  = sensorValue * 0.003225806;
  int batteryPcnt = sensorValue / 10;

  Serial.print("Battery Voltage: ");
  Serial.print(batteryV);
  Serial.println(" V");

  Serial.print("Battery percent: ");
  Serial.print(batteryPcnt);
  Serial.println(" %");

  if (oldBatteryPcnt != batteryPcnt) {
    // Power up radio after sleep
    gw.sendBatteryLevel(batteryPcnt);
    oldBatteryPcnt = batteryPcnt;
  }

  if (warm_up_done) {
    // Sleep until interrupt comes in on motion sensor. Send update every two minute. 
    gw.sleep(MOTION_INTERRUPT, CHANGE, SLEEP_TIME);
    
    // Read digital motion value as soon as we wake up
    boolean tripped = (digitalRead(PRESENSE_SENSOR_PIN) == HIGH); 
    
    Serial.print("Presense sensor tripped: ");
    Serial.println(tripped);
    gw.send(msg.set(tripped?"1":"0"));  // Send tripped value to gw     
  } else {
    // just sleep, ignoring interrupts
    unsigned long t = min(PRESENSE_SENSOR_WARM_UP, SLEEP_TIME);
    Serial.print("Waiting for ");
    Serial.print(t/1000);
    Serial.println(" seconds to allow presense sensor warm-up.");
    gw.sleep(t);
    warm_up_done = true;
  }
}
