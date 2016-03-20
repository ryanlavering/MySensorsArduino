#include <Arduino.h>
#include <SensorNode.h>
#include <Sensors.h>
#include <math.h>

//
// Interval sensors
//
IntervalSensor::IntervalSensor(Node *gw, unsigned long interval, uint8_t type, 
            uint8_t dtype, uint8_t id, uint8_t num_sub_devices,
            const char *description)
        : Sensor(gw, type, dtype, id, num_sub_devices, description), 
            m_interval(interval)
{
    m_next_timeout = millis(); // now
}

bool IntervalSensor::ready(unsigned long *next_check_ms) 
{
    unsigned long now = millis();
    if (now > m_next_timeout) {
        m_next_timeout += m_interval;
        return true;
    } else {
        if (next_check_ms != NULL) {
            *next_check_ms = m_next_timeout;
        }
        return false;
    }
}


// 
// Presentation meta-sensor
// Periodically triggers presentation of all node devices to controller
//
PresentationMetaSensor::PresentationMetaSensor(Node *gw, unsigned long interval)
        : IntervalSensor(gw, interval, S_CUSTOM, V_VAR1, AUTO, 0, "presentation")
{
    //Serial.println("Start: PresMeta");
}

bool PresentationMetaSensor::report()
{
    // Skip very first reading to prevent double-presentation (Node does 
    // the first present)
    static bool skip = true; 
    if (!skip) {
        m_gw->presentAll();
    }
    skip = false;
}


//
// Heartbeat Sensor
//
HeartBeatSensor::HeartBeatSensor(Node *gw, uint8_t device_id, 
            unsigned long interval)
        : IntervalSensor(gw, interval, S_LIGHT, V_LIGHT, device_id, 1, "heartbeat"),
            m_last_state(false)
{
    //Serial.println("Start: HeartBeat");
}

bool HeartBeatSensor::sense() 
{
    // Toggle state
    m_last_state = !m_last_state;
    return true; 
}

bool HeartBeatSensor::report() 
{
    return m_gw->send(getMessage().set(m_last_state));
}


//
// DHT22 Sensor
//
DHT22Sensor::DHT22Sensor(Node *gw, uint8_t device_id, 
            int sensor_pin, unsigned long interval)
        : IntervalSensor(gw, interval, S_TEMP, V_TEMP, device_id, 2), 
            m_sensor_pin(sensor_pin)
{
    // Set up second sub_device (index 1)
    setType(S_HUM, 1);
    setDataType(V_HUM, 1);
    
    // Set up DHT sensor
    m_dht.setup(sensor_pin);
    
    //Serial.println("Start: DHT22");
}

// Read sensor values
// TODO: This may take a while, which will block other node processing. 
// Consider looking into whether this could be made asynchronous.
bool DHT22Sensor::sense()
{
    // Make sure we've waited at least the minimum sampling period
    if (millis() <= m_nextsample) {
        delay(m_nextsample - millis());
    }
    m_nextsample = millis() + m_dht.getMinimumSamplingPeriod() + 100/*fudge*/;
    
    // Read values
    float temp = m_dht.toFahrenheit(m_dht.getTemperature()); // TODO: allow metric
    float hum = m_dht.getHumidity();
    
    // Is there anything new to report?
    bool new_data = ((abs(temp - m_lastTemp) > 0.1) || (abs(hum - m_lastHum) > 0.1));
    
    if (new_data) {
        m_lastTemp = temp;
        m_lastHum = hum;
    }
    
    return new_data;
}

bool DHT22Sensor::report()
{
    bool all_ok = true;
    
    Serial.print("DHT22: Sending temp ");
    Serial.println((int)m_lastTemp);
    all_ok &= m_gw->send(getMessage(0).set(m_lastTemp, 1));

    Serial.print("DHT22: Sending hum ");
    Serial.println((int)m_lastHum);
    all_ok &= m_gw->send(getMessage(1).set(m_lastHum, 1));
    
    return all_ok;
}


//
// LED 8x8 Panel 
//
SimpleIconLedMatrix::SimpleIconLedMatrix(Node *gw, uint8_t device_id,
            int data_in_pin, int clk_pin, int load_pin)
        : Sensor(gw, S_DIMMER, V_DIMMER, device_id, 2, "LED_Matrix"),
            m_lc(data_in_pin, clk_pin, load_pin, 1)
{
    // No need to adjust subdevices; both are (S_DIMMER, V_DIMMER)
    
    // Make sure that subdevices have real IDs assigned before we call getId()
    // TODO: Maybe make getId() autoamtically ensure AUTO gets assigned?
    //      - would require Device to know about Node
    m_gw->assignDeviceIds(this);
    
    // Set the brightness to a low value initially
    m_lc.setIntensity(0, 1);
    // and clear the display
    m_lc.clearDisplay(0);
    
    // Get the initial value from EEPROM
    m_brightness = constrain(m_gw->loadState(EEPROM_ADDR(getId(LED_PANEL))), LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
    //Serial.print("Current brightness: ");
    //Serial.println(m_brightness);
    if (m_brightness != 0) {
        /*
        The MAX72XX is in power-saving mode on startup,
        we have to do a wakeup call
        */
        m_lc.shutdown(0, false);
        m_lc.setIntensity(0, m_brightness);
    } else {
        m_lc.shutdown(0, true);
    }
    // And draw the initial icon (making sure that EEPROM value is sane)
    m_icon = constrain(m_gw->loadState(EEPROM_ADDR(getId(LED_ICON))), 0, NUM_ICONS-1);
    //Serial.print("Current icon: ");
    //Serial.println(m_icon);
    draw(icons[m_icon]);
    
    //Serial.println("Start: LED Panel");
}

bool SimpleIconLedMatrix::present()
{
    m_gw->present(getId(LED_PANEL), getType(LED_PANEL));
    m_gw->present(getId(LED_ICON), getType(LED_ICON));
    // Domoticz MySensors integration doesn't actually register a new device 
    // when a S_DIMMER presentation is sent. Sending a value will, however.
    m_gw->send(getMessage(LED_PANEL).set(m_brightness));
    m_gw->send(getMessage(LED_ICON).set(m_icon));
}
            
bool SimpleIconLedMatrix::react(const MyMessage &msg) 
{
    // Controller might send V_DIMMER (for dimmer setting change) 
    // or V_LIGHT (on/off switch) 
    if ((msg.type == V_DIMMER || msg.type == V_LIGHT) && msg.sensor == getId(LED_PANEL)) {
        //  Retrieve the power or dim level from the incoming request message
        int requestedLevel = atoi( msg.data );

        // If the light is currently on, and we're turning off, 
        // then save the current brightness as the last seen (EEPROM+1)
        if (requestedLevel == 0 && m_brightness != 0) {
            m_gw->saveState(EEPROM_ADDR(msg.sensor)+1, m_brightness);
        }

        // Adjust incoming level if this is a V_LIGHT variable update [0 == off, 1 == on]
        if (msg.type == V_LIGHT && requestedLevel == 1) {
            // Get the last known brightness, instead of 100%
            m_brightness = m_gw->loadState(EEPROM_ADDR(msg.sensor)+1);
        } else {
            // Clip incoming level to valid range of 0 to 100
            requestedLevel = constrain(requestedLevel, 0, 100);
            // And map to actual LED array levels
            m_brightness = map(requestedLevel, 0, 100, LED_PANEL_MIN_LEVEL, LED_PANEL_MAX_LEVEL);
        }

        // Store state in eeprom
        m_gw->saveState(EEPROM_ADDR(msg.sensor), m_brightness);

        // Write some debug info
        Serial.print("LED Panel new status: ");
        Serial.println(m_brightness);

        // And set the actual brightness
        if (m_brightness != 0) {
            // turn in on
            m_lc.shutdown(0, false);
            m_lc.setIntensity(0, m_brightness);
        } else {
            // turn if off
            m_lc.shutdown(0, true);
        }
        
        // Message handled
        return true;
    } else if ((msg.type == V_DIMMER || msg.type == V_LIGHT) && msg.sensor == getId(LED_ICON)) {
        //  Retrieve the power or dim level from the incoming request message
        int newIcon = atoi( msg.data );

        // TODO: Figure out a better way to select icons -- slider is inefficient
        // Clip incoming level to valid range of 0 to 100
        newIcon = constrain(newIcon, 0, 100);
        // And map to actual LED array levels
        newIcon = map(newIcon, 0, 100, 0, NUM_ICONS-1);

        // Only set if the value is reasonable
        if (newIcon >= 0 && newIcon < NUM_ICONS) {
            // Store state in eeprom
            m_gw->saveState(EEPROM_ADDR(msg.sensor), newIcon);

            // Write some debug info
            Serial.print("LED Panel icon index: ");
            Serial.println(newIcon);

            // Draw the icon
            draw(icons[newIcon]);
            m_icon = newIcon;
        } else {
            // Write some debug info
            Serial.print("Invalid icon requested: ");
            Serial.println(newIcon);
        }
        
        // Message handled
        return true;
    }
    
    // Message not handled
    return false;
}

void SimpleIconLedMatrix::draw(const byte *ico) {
    if (ico == NULL) return;

    for (int i = 0; i < 8; i++) {
        // display is upside down, so 7-i
        m_lc.setColumn(0, FLIP(i), ico[i]);
    }
}


//
// Presence Sensor
//
PresenceSensor::PresenceSensor(Node *gw, uint8_t device_id, int sense_pin, 
            unsigned long off_delay)
        : Sensor(gw, S_LIGHT, V_LIGHT, device_id, 1, "presence"),
            m_sense_pin(sense_pin),
            m_tripped(false),
            m_off_after(PRESENCE_TIMER_OFF),
            m_off_delay(off_delay)
{
    pinMode(sense_pin, INPUT);
    m_warm_up_done = millis() + PRESENCE_SENSOR_WARM_UP;
}
    
bool PresenceSensor::ready(unsigned long *next_check_ms)
{
    if (millis() >= m_warm_up_done) {
        if (motionDetected()) {
            // If motion has been detected, then report ready so that 
            // the node won't go to sleep. We also need the node to keep 
            // calling sense() to eventually move to the "off" state. 
            return true;
        } else if (digitalRead(m_sense_pin) == HIGH) {
            // Any time the sense pin reads high we are ready.
            // (This should be hit when coming off of an interrupt.) 
            return true;
        } else {
            // If no motion is detected, then we can report "not ready".
            // The interrupt handler will kick the node active once a
            // motion event occurs. 
            if (next_check_ms != NULL) {
                *next_check_ms = SLEEP_UNTIL_INTERRUPT;
            }
            return false;
        }
    } else {
        // If we aren't warmed up, then report "not ready". 
        if (next_check_ms != NULL) {
            *next_check_ms = m_warm_up_done;
        }
        return false;
    }
}
    
bool PresenceSensor::sense()
{
    bool offTimerWasActive = (m_off_after != PRESENCE_TIMER_OFF);
    bool current = (digitalRead(m_sense_pin) == HIGH); 
    if (current != m_tripped) {
        // Value changed since last reading
#if PRESENCE_DEBUG
        Serial.println(current ? "Motion detected" : "All quiet");
#endif
        m_tripped = current;
        if (m_tripped) {
            // (re)set the timer
            m_off_after = millis() + m_off_delay;
#if PRESENCE_DEBUG
            Serial.print("Time: ");
            Serial.print(millis());
            Serial.print(", turning off at: ");
            Serial.println(m_off_after);
#endif
        }
#if 0 // Bad idea because packets could be lost. Better to transmit more often.
        if (m_tripped && offTimerWasActive) {
            // we changed to "on", but hadn't yet reported the "off", so 
            // no state change needed
            return false;
        } else 
#endif
        {
            // changed to "on" from reported "off" state, 
            // or changed to "off". 
            return m_tripped; // only send an "on" signal right away
        }
    } else {
        if (millis() > m_off_after) {
            m_tripped = false; // set to off
            m_off_after = PRESENCE_TIMER_OFF; // disable timer
#if PRESENCE_DEBUG
            Serial.print("No motion detected. Turning off at: ");
            Serial.println(millis());
#endif
            return true; // send the off signal now
        } else {
            // nothing to report
            return false; 
        }
    }
}

bool PresenceSensor::report()
{
    bool ok = m_gw->send(getMessage().set(m_tripped));
    if (!ok && !m_tripped) {
        // "Off" signal lost. Make sure to resend next round.
        m_off_after = 0;
    }
    
    return ok;
}

int PresenceSensor::getInterrupt()
{ 
    // Don't respond to interrupts until warmup is complete
    if (millis() < m_warm_up_done) 
        return -1;
    
    if (m_sense_pin == 2 || m_sense_pin == 3) {
        // Assume we're on nano/micro/uno where pin<->interrupt mapping is 
        // trivial 
        return m_sense_pin - 2; 
    } else {
        return -1;
    }
}

bool PresenceSensor::motionDetected()
{
    // If the presence timer is active, then we detected motion within
    // the current timeout time.
    // See ::sense() for timer logic. 
    return (m_off_after != PRESENCE_TIMER_OFF);
}

#if 1

//
// LED Light
//
LEDLight::LEDLight(Node *gw, int pwm_pin, uint8_t device_id)
        : Sensor(gw, S_LIGHT, V_DIMMER/*not used*/, device_id, 1, "LED"),
            m_pin(pwm_pin),
            m_brightness(0),
            m_on(false)
{
    // Make sure that subdevices have real IDs assigned before we call getId()
    m_gw->assignDeviceIds(this);
    
    // Get the initial value from EEPROM
    m_brightness = constrain(m_gw->loadState(EEPROM_ADDR(getId())), LED_MIN_LEVEL, LED_MAX_LEVEL);

    // Set up output pin and initial state (off)
    pinMode(m_pin, OUTPUT);
    analogWrite(m_pin, 0); // light off by default
}

// given to = 0..100%, fade the LED panel to the matching level
void LEDLight::fade(uint8_t to, float delta, bool raw_value)
{
    uint8_t curr = (m_on ? m_brightness : 0);
    int direction = (to < curr) ? -1 : 1;
    
    // calculate automatic delta value for smooth transitions that take a 
    // bounded amount of time
    if (delta == LED_FADE_AUTO) {
        // Linear
        //delta = max(abs(to-curr) / LED_FADE_NUMSTEPS, 1);
        
        // Logarithmic
        // This is based solving for R in the P*e^(R*t) formula, which actually 
        // gives a rate (delta) based on continuous growth, whereas we're 
        // actually growing only at discrete intervals, so our delta will 
        // result in slightly too many steps, but that's not super critical.
        delta = 1.0 + (log(max(1,abs(to-curr))) / LED_FADE_NUMSTEPS);
    }
    
    if (raw_value) {
        to = constrain(to, LED_MIN_LEVEL, LED_MAX_LEVEL);
    } else {
        to = constrain(to, 0, 100);
        to = map(to, 0, 100, LED_MIN_LEVEL, LED_MAX_LEVEL);
    }

    //if ((to < curr && delta > 0) || (to > curr && delta < 0)) { // linear
    if ((direction < 0 && delta > 1) || (direction > 0 && delta < 1)) { // log
        // reverse delta
        // Linear
        //delta = -delta;
        
        // Logarithmic
        delta = 1/delta;
    }

    while (curr != to) {
        // Linear
        //curr = constrain(curr + delta, min(curr, to), max(curr, to));
        // Logarithmic
        uint8_t prev = curr;
        curr = constrain(curr * delta, min(curr, to), max(curr, to));
        if (curr == prev && curr != to) {
            // must always move at least one to avoid infinite loop
            curr += direction;
        }
        analogWrite(m_pin, curr);
        delay(LED_FADE_DELAY);
    }
}

bool LEDLight::react(const MyMessage &msg)
{
    // Controller might send V_DIMMER (for dimmer setting change) 
    // or V_LIGHT (on/off switch) 
    if ((msg.type == V_DIMMER || msg.type == V_LIGHT) && msg.sensor == getId()) {    
        //  Retrieve the power or dim level from the incoming request message
        int requestedLevel = atoi( msg.data );
        
        Serial.print("Requested level: ");
        Serial.println(requestedLevel);
        
        // Adjust incoming level if this is a V_LIGHT variable update [0 == off, 1 == on]
        if (msg.type == V_LIGHT && requestedLevel == 1) {
            requestedLevel = m_brightness;
        } else {
            // Clip incoming level to valid range of 0 to 100
            requestedLevel = constrain(requestedLevel, 0, 100);
            // And map to actual LED array levels
            requestedLevel = map(requestedLevel, 0, 100, LED_MIN_LEVEL, LED_MAX_LEVEL);
        }
        
        int setting_on = (requestedLevel != 0);
        if (setting_on) {
            // Only save brightness to last "on" value
            m_gw->saveState(EEPROM_ADDR(msg.sensor), (uint8_t)requestedLevel);
        }

        // Write some debug info
        Serial.print("LED Panel new status: ");
        Serial.println(setting_on ? requestedLevel : 0);

        // Set the actual brightness (will update m_brightness)
        fade(setting_on ? requestedLevel : 0, LED_FADE_AUTO, true);
        
        // And update the state after fading (fade uses m_on for its current
        // brightness level, so we don't want to update until after the fade)
        m_on = setting_on;
        if (m_on) {
            m_brightness = (uint8_t)requestedLevel;
        }
        
        // Message handled
        return true;
    } else {
        return false;
    }
}

void LEDLight::setState(bool on)
{
    fade(on ? m_brightness : 0, LED_FADE_AUTO, true);
    m_on = on;
}

#endif
