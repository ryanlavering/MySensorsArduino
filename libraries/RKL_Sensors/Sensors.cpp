#include <Arduino.h>
#include <SensorNode.h>
#include <Sensors.h>

//
// Interval sensors
//
IntervalSensor::IntervalSensor(Node *gw, unsigned long interval, uint8_t type, 
            uint8_t dtype, uint8_t id, uint8_t num_sub_devices)
        : Sensor(gw, type, dtype, id, num_sub_devices), m_interval(interval)
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
        : IntervalSensor(gw, interval, S_CUSTOM, V_VAR1, AUTO, 0)
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
        : IntervalSensor(gw, interval, S_LIGHT, V_LIGHT, device_id),
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
        : Sensor(gw, S_DIMMER, V_DIMMER, device_id, 2),
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
PresenceSensor::PresenceSensor(Node *gw, uint8_t device_id, int sense_pin)
        : Sensor(gw, S_LIGHT, V_LIGHT, device_id, 1),
            m_sense_pin(sense_pin),
            m_tripped(false)
{
    pinMode(sense_pin, INPUT);
    m_warm_up_done = millis() + PRESENCE_SENSOR_WARM_UP;
}
    
bool PresenceSensor::ready(unsigned long *next_check_ms)
{
    if (millis() > m_warm_up_done) {
        return true;
    } else {
        if (next_check_ms != NULL) {
            *next_check_ms = m_warm_up_done;
        }
        return false;
    }
}
    
bool PresenceSensor::sense()
{
    bool current = (digitalRead(m_sense_pin) == HIGH); 
    if (current != m_tripped) {
        // Value changed since last reading
        m_tripped = current;
        return true;
    } else {
        // Nothing to report
        return false;
    }
}

bool PresenceSensor::report()
{
    return m_gw->send(getMessage().set(m_tripped));
}

int PresenceSensor::getInterrupt()
{ 
    if (m_sense_pin == 2 || m_sense_pin == 3) {
        // Assume we're on nano/micro/uno where pin<->interrupt mapping is 
        // trivial 
        return m_sense_pin - 2; 
    } else {
        return -1;
    }
}
