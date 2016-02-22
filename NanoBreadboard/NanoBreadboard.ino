#include <SPI.h>
#include <MySensor.h>
#include <LedControl.h>
#include <RKLIcons.h>

// Give each sensor 4 bytes in EEPROM
#define EEPROM_ADDR(sensor_id) (sensor_id * 4)

// MySensors
MySensor gw;
boolean metric = true;
#define CHILD_ID_LED_PANEL 5
#define CHILD_ID_LED_ICON 6
MyMessage msgPanel(CHILD_ID_LED_PANEL, V_DIMMER);
MyMessage msgIcon(CHILD_ID_LED_ICON, V_DIMMER);

// Sleep time
#define CYCLE_PERIOD 5000

// Sensor presentation
#define SENSOR_PRESENTATION_PERIOD (5*60*1000)

// LED Panel
// pin 8 is connected to the DataIn
// pin 6 is connected to the CLK
// pin 7 is connected to LOAD
// We have only a single MAX72XX.
LedControl lc = LedControl(8, 6, 7, 1);
// wait a bit between updates of the display
unsigned long delaytime = 50;
#define LED_PANEL_MAX_LEVEL 15
#define LED_PANEL_MIN_LEVEL 0
int led_panel_val = 1;
int current_icon = 0;

#define FLIP(i) (7-i)

void allOn() {
  for (int row = 0; row < 8; row++) {
    lc.setRow(0, row, 0xff);
  }
}

void allOff() {
  for (int row = 0; row < 8; row++) {
    lc.setRow(0, row, (byte)0);
  }
}

void draw(const byte *ico) {
  if (ico == NULL) return;
  
  for (int i = 0; i < 8; i++) {
    // display is upside down, so 7-i
    lc.setColumn(0, FLIP(i), ico[i]);
  }
}


void presentSensors()
{
  Serial.println("Presenting sensors to base station...");
  // Register all sensors to gw (they will be created as child devices)
  gw.present(CHILD_ID_LED_PANEL, S_DIMMER);
  gw.present(CHILD_ID_LED_ICON, S_DIMMER);
  // Domoticz MySensors integration doesn't actually register a new device 
  // when a S_DIMMER presentation is sent. Sending a value will, however.
  gw.send(msgPanel.set(led_panel_val));
  gw.send(msgIcon.set(current_icon));
  Serial.println("\t...presentation complete.");
}

void incomingMessage(const MyMessage &message) {
  Serial.print("Incoming change for sensor:");
  Serial.println(message.sensor);
  // We only expect one type of message from controller. But we better check anyway.
  if ((message.type == V_DIMMER || message.type == V_LIGHT) && message.sensor == CHILD_ID_LED_PANEL) {
    //  Retrieve the power or dim level from the incoming request message
    int requestedLevel = atoi( message.data );

    // If the light is currently on, and we're turning off, 
    // then save the current brightness as the last seen (EEPROM+1)
    if (requestedLevel == 0 && led_panel_val != 0) {
      gw.saveState(EEPROM_ADDR(message.sensor)+1, led_panel_val);
    }

    // Adjust incoming level if this is a V_LIGHT variable update [0 == off, 1 == on]
    if (message.type == V_LIGHT && requestedLevel == 1) {
      // Get the last known brightness, instead of 100%
      led_panel_val = gw.loadState(EEPROM_ADDR(message.sensor)+1);
    } else {
      // Clip incoming level to valid range of 0 to 100
      requestedLevel = constrain(requestedLevel, 0, 100);
      // And map to actual LED array levels
      led_panel_val = map(requestedLevel, 0, 100, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
    }

    // Store state in eeprom
    gw.saveState(EEPROM_ADDR(message.sensor), led_panel_val);

    // Write some debug info
    Serial.print("LED Panel new status: ");
    Serial.println(led_panel_val);

    // And set the actual brightness
    if (led_panel_val != 0) {
      // turn in on
      lc.shutdown(0, false);
      lc.setIntensity(0, led_panel_val);
    } else {
      // turn if off
      lc.shutdown(0, true);
    }
  } else if ((message.type == V_DIMMER || message.type == V_LIGHT) && message.sensor == CHILD_ID_LED_ICON) {
    //  Retrieve the power or dim level from the incoming request message
    int newIcon = atoi( message.data );

    // TODO: Figure out a better way to select icons -- slider is inefficient
    // Clip incoming level to valid range of 0 to 100
    newIcon = constrain(newIcon, 0, 100);
    // And map to actual LED array levels
    newIcon = map(newIcon, 0, 100, 0, NUM_ICONS-1);

    // Only set if the value is reasonable
    if (newIcon >= 0 && newIcon < NUM_ICONS) {
      // Store state in eeprom
      gw.saveState(EEPROM_ADDR(message.sensor), newIcon);
  
      // Write some debug info
      Serial.print("LED Panel icon index: ");
      Serial.println(newIcon);
  
      // Draw the icon
      draw(icons[newIcon]);
      current_icon = newIcon;
    } else {
      // Write some debug info
      Serial.print("Invalid icon requested: ");
      Serial.println(newIcon);
    }
  }
}

void setup() {
  gw.begin(incomingMessage, AUTO, true);
  // Send the Sketch Version Information to the Gateway
  gw.sendSketchInfo("NanoBreadboard", "1.2");

  // Set the brightness to a low value initially
  lc.setIntensity(0, 1);
  // and clear the display
  lc.clearDisplay(0);

  // Get the initial value from EEPROM
  led_panel_val = constrain(gw.loadState(EEPROM_ADDR(CHILD_ID_LED_PANEL)), LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
  Serial.print("Current brightness: ");
  Serial.println(led_panel_val);
  if (led_panel_val != 0) {
    /*
    The MAX72XX is in power-saving mode on startup,
    we have to do a wakeup call
    */
    lc.shutdown(0, false);
    lc.setIntensity(0, led_panel_val);
  } else {
    lc.shutdown(0, true);
  }
  // And draw the initial icon (making sure that EEPROM value is sane)
  current_icon = constrain(gw.loadState(EEPROM_ADDR(CHILD_ID_LED_ICON)), 0, NUM_ICONS-1);
  Serial.print("Current icon: ");
  Serial.println(current_icon);
  draw(icons[current_icon]);
}

void beat(byte *ico, int bright = 2) {
  bright = constrain(bright, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
  int step_time = (delaytime * 4) / (bright + 1);

  draw(ico);
  for (int b = LED_PANEL_MIN_LEVEL; b < bright; b++) {
    lc.setIntensity(0, b);
    delay(step_time);
  }
  for (int b = bright; b >= LED_PANEL_MIN_LEVEL; b--) {
    lc.setIntensity(0, b);
    delay(step_time);
  }
}

void loop() {
  static unsigned long nextPresent = 0;
  static unsigned long nextCycle = 0;

  // Process incoming messages
  gw.process();

  // Every once in a while make sure the gateway knows about our sensors.
  if (millis() > nextPresent) {
    nextPresent += SENSOR_PRESENTATION_PERIOD;
    presentSensors();
  }

  if (millis() > nextCycle) {
    nextCycle += CYCLE_PERIOD;
    //beat(small_heart_icon);
  }
}
