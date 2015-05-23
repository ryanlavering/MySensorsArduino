#include <SPI.h>
#include <MySensor.h>
#include <DHT.h>
#include <NewPing.h>

// Give each sensor 4 bytes in EEPROM
#define EEPROM_ADDR(sensor_id) (sensor_id * 4)

#define CHILD_ID_HUM 0
#define CHILD_ID_TEMP 1
#define CHILD_ID_LIGHT 2
#define CHILD_ID_LED_PANEL 3
#define CHILD_ID_DISTANCE 4

// DHT Humid/temp sensor
#define DHT_SENSOR_DIGITAL_PIN 5
DHT dht;
float lastTemp;
float lastHum;

// Simple analog light sensor
#define LIGHT_SENSOR_ANALOG_PIN 0
int lastLightLevel = 0;
int light_min = 100;
int light_max = 0;

// LED Panel
#define LED_PANEL_PIN 6
#define LED_PANEL_MAX_LEVEL 255
#define LED_PANEL_MIN_LEVEL 5
int brightness = 0;
bool led_panel_on = true;

// Distance Sensor
#define TRIGGER_PIN  8  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     7  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
int lastDist;

unsigned long SLEEP_TIME = 5000; // Sleep time between reads (in milliseconds)

// MySensors 
MySensor gw;
boolean metric = true; 
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);
MyMessage msgLight(CHILD_ID_LIGHT, V_LIGHT_LEVEL);
MyMessage msgDist(CHILD_ID_DISTANCE, V_DISTANCE);

// Sensor presentation
#define SENSOR_PRESENTATION_PERIOD (5*60*1000)

void presentSensors()
{
  Serial.println("Presenting sensors to base station...");
  // Register all sensors to gw (they will be created as child devices)
  gw.present(CHILD_ID_HUM, S_HUM);
  gw.present(CHILD_ID_TEMP, S_TEMP);
  gw.present(CHILD_ID_LIGHT, S_LIGHT_LEVEL);
  gw.present(CHILD_ID_LED_PANEL, S_LIGHT);
  gw.present(CHILD_ID_DISTANCE, S_DISTANCE);
  Serial.println("\t...presentation complete.");
}

// given to = 0..100%, fade the LED panel to the matching level
void fade(int to, bool automap=true)
{
  int step = 5;
  
  // Automatically map from highest / lowest values seen on light sensor 
  if (automap) {
    to = map(to, 0, 100, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
  } else {
    to = constrain(to, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
  }
  
  if (to < brightness)
    step = -step;
    
  while (abs(to-brightness) > abs(step)) {
    brightness = constrain(brightness + step, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
    analogWrite(LED_PANEL_PIN, brightness);
    delay(30);
  }
}

void incomingMessage(const MyMessage &message) {
  Serial.print("Incoming change for sensor:");
  Serial.println(message.sensor);
  // We only expect one type of message from controller. But we better check anyway.
  if (message.type==V_LIGHT) {
     // Change relay state
     led_panel_on = message.getBool();
     // Store state in eeprom
     gw.saveState(EEPROM_ADDR(message.sensor), message.getBool());
     // Write some debug info
     Serial.print("LED Panel new status: ");
     Serial.println(message.getBool());
     if (led_panel_on) {
       // set to min level
       fade(0, false);
     } else {
       // go to min
       fade(0, false);
       // then turn off completely
       analogWrite(LED_PANEL_PIN, 0);
     }
   } 
}

void setup()  
{ 
  gw.begin(incomingMessage, AUTO, true);
  // Send the Sketch Version Information to the Gateway
  gw.sendSketchInfo("Breadboard", "2.3");
  // Will be done in Loop
  //presentSensors();
  metric = gw.getConfig().isMetric;
  
  // DHT setup
  dht.setup(DHT_SENSOR_DIGITAL_PIN); 

  // LED Panel setup
  pinMode(LED_PANEL_PIN, OUTPUT);
  led_panel_on = gw.loadState(EEPROM_ADDR(CHILD_ID_LED_PANEL));
  light_min = gw.loadState(EEPROM_ADDR(CHILD_ID_LIGHT));
  light_max = gw.loadState(EEPROM_ADDR(CHILD_ID_LIGHT)+1);
}

void loop()      
{  
  static unsigned long nextPresent = 0;
  static unsigned long nextDHTSample = 0;
  static unsigned long nextSample = 0;

  // Process incoming messages
  gw.process();
  
  // Every once in a while make sure the gateway knows about our sensors.
  // TODO: Is retransmit necessary?
  if (millis() > nextPresent) {
    presentSensors();
    nextPresent += SENSOR_PRESENTATION_PERIOD;
  }
  
  if (millis() > nextSample) {
    nextSample += SLEEP_TIME;

    if (millis() <= nextDHTSample) {
      delay(nextDHTSample - millis());
    }
    nextDHTSample = millis() + dht.getMinimumSamplingPeriod() + 100/*fudge*/;
  
    float temperature = dht.getTemperature();
    if (isnan(temperature)) {
        Serial.println("Failed reading temperature from DHT");
    } else if (temperature != lastTemp) {
      lastTemp = temperature;
      if (!metric) {
        temperature = dht.toFahrenheit(temperature);
      }
      if (!gw.send(msgTemp.set(temperature, 1))) {
        // If packet transmission failed then force a retransmit on next reading
        lastTemp = NAN;
      }
      Serial.print("T: ");
      Serial.println(temperature);
    }
    
    float humidity = dht.getHumidity();
    if (isnan(humidity)) {
        Serial.println("Failed reading humidity from DHT");
    } else if (humidity != lastHum) {
        lastHum = humidity;
        if (!gw.send(msgHum.set(humidity, 1))) {
          // If packet transmission failed then force a retransmit on next reading
          lastHum = NAN;
        }
        Serial.print("H: ");
        Serial.println(humidity);
    }

    // Get raw light level
    int lightLevel = (1023-analogRead(LIGHT_SENSOR_ANALOG_PIN))/10.23; 
    // and update extremes if necessary
    if (lightLevel < light_min) {
      light_min = lightLevel;
      gw.saveState(EEPROM_ADDR(CHILD_ID_LIGHT), light_min);
    }
    if (lightLevel > light_max) {
      light_max = lightLevel;
      gw.saveState(EEPROM_ADDR(CHILD_ID_LIGHT)+1, light_max);
    }
    
    // map light level against historical min/max
    lightLevel = map(lightLevel, light_min, light_max, 0, 100);
#if 0
    // LED panel gets brighter with ambient
    if (led_panel_on) {
      fade(lightLevel);
    }
#endif
    Serial.print("L: ");
    Serial.println(lightLevel);
    if (lightLevel != lastLightLevel) {
        if (!gw.send(msgLight.set(lightLevel))) {
          lastLightLevel = -1; // force retransmit next round
        } else {
          lastLightLevel = lightLevel;
        }
    }

    int distus = sonar.ping_median();
    int dist = metric ? sonar.convert_cm(distus) : sonar.convert_in(distus);
    int distcm = sonar.convert_cm(distus);
    if (led_panel_on) {
      if (distcm != 0) {
        int distcm2 = MAX_DISTANCE;
        if (lastDist == 0) {
          // Filter for spurious nonzero readings.
          delay(100);
          int distus2 = sonar.ping_median();
          distcm2 = sonar.convert_cm(distus2);
        }
        if (distcm2 != 0) {
          // Light gets brighter as you approach
          int to = map(distcm, MAX_DISTANCE, 1, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
          fade(to, false);
        }
      } else {
        // Turn off
        fade(LED_PANEL_MIN_LEVEL, false);
        brightness = 0;
        analogWrite(LED_PANEL_PIN, brightness);
        delay(30);
      }
    }
    Serial.print("D: ");
    Serial.print(dist); // Convert ping time to distance in cm and print result (0 = outside set distance range)
    Serial.println(metric ? " cm" : " in");
    if (dist != lastDist) {
        gw.send(msgDist.set(dist));
        lastDist = dist;
    }
  }
  
#if 0
  int distus = sonar.ping_median();
  int distcm = sonar.convert_cm(distus);
  Serial.print("\tD: ");
  Serial.print(distcm); // Convert ping time to distance in cm and print result (0 = outside set distance range)
  Serial.println(" cm");
  static int zerocount = 0;
  if (led_panel_on) {
    if (distcm != 0) {
      // Light gets brighter as you approach
      brightness = map(distcm, MAX_DISTANCE, 1, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
      analogWrite(LED_PANEL_PIN, brightness);
      delay(30);
      zerocount = 0;
    } else {
      zerocount++;
      if (zerocount > 30/*adjustable*/) {
        // Turn off
        brightness = 0;
        analogWrite(LED_PANEL_PIN, brightness);
        delay(30);
        zerocount = 0;
      }
    }
  }
#endif

  //gw.sleep(SLEEP_TIME); //sleep a bit
}


