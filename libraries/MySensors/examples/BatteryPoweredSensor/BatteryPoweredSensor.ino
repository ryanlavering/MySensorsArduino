/**
 * The MySensors Arduino library handles the wireless radio link and protocol
 * between your home built sensors/actuators and HA controller of choice.
 * The sensors forms a self healing radio network with optional repeaters. Each
 * repeater and gateway builds a routing tables in EEPROM which keeps track of the
 * network topology allowing messages to be routed to nodes.
 *
 * Created by Henrik Ekblad <henrik.ekblad@mysensors.org>
 * Copyright (C) 2013-2015 Sensnology AB
 * Full contributor list: https://github.com/mysensors/Arduino/graphs/contributors
 *
 * Documentation: http://www.mysensors.org
 * Support Forum: http://forum.mysensors.org
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 *******************************
 *
 * DESCRIPTION
 *
 * This is an example that demonstrates how to report the battery level for a sensor
 * Instructions for measuring battery capacity on A0 are available here:
 * http://www.mysensors.org/build/battery
 * 
 */


#include <SPI.h>
#include <MySensor.h>

int BATTERY_SENSE_PIN = A2;  // select the input pin for the battery sense point

MySensor gw;
//unsigned long SLEEP_TIME = 900000;  // sleep time between reads (seconds * 1000 milliseconds)
unsigned long SLEEP_TIME = 10000;  // sleep time between reads (seconds * 1000 milliseconds)
int oldBatteryPcnt = 0;

void setup()  
{
<<<<<<< HEAD
   // use the 3.3 V Vcc reference
   analogReference(DEFAULT);
=======
   // use the 1.1 V internal reference
#if defined(__AVR_ATmega2560__)
   analogReference(INTERNAL1V1);
#else
   analogReference(INTERNAL);
#endif
>>>>>>> 59550410a3f55f11b39aacfcaaa2dd8926682673
   gw.begin();

   // Send the sketch version information to the gateway and Controller
   gw.sendSketchInfo("Battery Sensors", "1.0");
}

void loop()
{
   // get the battery Voltage
   int sensorValue = analogRead(BATTERY_SENSE_PIN);
   #ifdef DEBUG
   Serial.println(sensorValue);
   #endif
   
   // Direct connection to batteries (Nominal 1.6 V * 2 = 3.2V Max) 
   // measured across 3.3 V (nom.) reference from buck converter
   // 3.3/1023 = Volts per bit = 0.003225806
   float batteryV  = sensorValue * 0.003225806;
   int batteryPcnt = sensorValue / 10;

   #ifdef DEBUG
   Serial.print("Battery Voltage: ");
   Serial.print(batteryV);
   Serial.println(" V");

   Serial.print("Battery percent: ");
   Serial.print(batteryPcnt);
   Serial.println(" %");
   #endif

   if (oldBatteryPcnt != batteryPcnt) {
     // Power up radio after sleep
     gw.sendBatteryLevel(batteryPcnt);
     oldBatteryPcnt = batteryPcnt;
   }
   gw.sleep(SLEEP_TIME);
}
