// This is an example that demonstrates how to report the battery level for a sensor
// Instructions for measuring battery capacity on A0 are available in the follwoing forum
// thread: http://forum.micasaverde.com/index.php/topic,20078.0.html

#include <SPI.h>
#include <MySensor.h>

int BATTERY_SENSE_PIN = A2;  // select the input pin for the battery sense point

MySensor gw;
//unsigned long SLEEP_TIME = 900000;  // sleep time between reads (seconds * 1000 milliseconds)
unsigned long SLEEP_TIME = 10000;  // sleep time between reads (seconds * 1000 milliseconds)
int oldBatteryPcnt = 0;

void setup()  
{
   // use the 3.3 V Vcc reference
   analogReference(DEFAULT);
   gw.begin();

   // Send the sketch version information to the gateway and Controller
   gw.sendSketchInfo("Battery Sensors", "1.0");
}

void loop()
{
   // get the battery Voltage
   int sensorValue = analogRead(BATTERY_SENSE_PIN);
   Serial.println(sensorValue);
   
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
   gw.sleep(SLEEP_TIME);
}
