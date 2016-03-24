/* RKL Sensor Nodes */

#include <Arduino.h>
#include <SensorNode.h>

// 
// Node Class
//
// Implements logic for scheduling and updates to the devices associated with 
// this node.
//

Node::Node(MyTransport &_radio, MyHw &_hw
#ifdef MY_SIGNING_FEATURE
    , MySigning &_signer
#endif
#ifdef WITH_LEDS_BLINKING
    , uint8_t _rx, uint8_t _tx, uint8_t _er, unsigned long _blink_period
#endif
    )
        : MySensor(_radio, _hw
#ifdef MY_SIGNING_FEATURE
            , _signer
#endif
#ifdef WITH_LEDS_BLINKING
            , _rx, _tx, _er, _blink_period
#endif
            )
{
    next_device_id = 0;
    memset(m_sensors, 0, sizeof(m_sensors));
    m_num_sensors = 0;
    m_sleep_callback = NULL;
    m_wake_callback = NULL;
}

/*
void Node::begin(void (* msgCallback)(const MyMessage &), uint8_t nodeId, 
            boolean repeaterMode, uint8_t parentNodeId, 
            rf24_pa_dbm_e paLevel, uint8_t channel, rf24_datarate_e dataRate) 
{
    // Call parent class begin function, but always use processIncoming() as 
    // the callback.
    MySensor::begin(msgCallback, nodeId, repeaterMode, parentNodeId, 
        paLevel, channel, dataRate);
}
*/

// Present a sub-device
// Will set a device id automatically if necessary.
void Node::presentDevice(Sensor *s, uint8_t sub_device, bool ack) 
{
    // make sure device has a proper ID
    if (s->getId(sub_device) == AUTO) {
        s->setId(next_device_id++, sub_device);
    }
    if (!s->present()) {
        present(s->getId(sub_device), s->getType(sub_device), s->getDescription(sub_device), ack);
    }
}

void Node::presentAll(bool ack)
{
    for (int i = 0; i < m_num_sensors; i++) {
        Sensor *s = m_sensors[i];
        assignDeviceIds(s); // make sure all devices have a proper ID
        if (!s->present()) {
            for (int j = 0; j < s->getNumSubDevices(); j++) {
                present(s->getId(j), s->getType(j), s->getDescription(j), ack);
            }
        }
    }
}

bool Node::addSensor(Sensor *sensor) 
{
    if (sensor == NULL || m_num_sensors >= MAX_SENSORS)
        return false;
    m_sensors[m_num_sensors++] = sensor;
    assignDeviceIds(sensor); // fill in any device IDs that request AUTO
    return true;
}

void Node::assignDeviceIds(Device *d)
{
    for (int i = 0; i < d->getNumSubDevices(); i++) {
        if (d->getId(i) == AUTO) {
            d->setId(next_device_id++, i);
        }
    }
}

// Update the node state machine. 
//
// Searches through all sensors to see 
// if any have pending actions, and processes them.
//
// This is the main interface that clients will call during the processing
// loop, so it should not block for long. 
void Node::update()
{
    unsigned long next_check_ms = SLEEP_UNTIL_INTERRUPT;
    unsigned long sleep_until = SLEEP_UNTIL_INTERRUPT; // sleep forever
    bool interrupts[2] = {false, false}; // hard coded for 2-interrupt arduinos
    bool do_sleep = true;
    
    while (process()) {
        // move data through the network
        // See processIncoming()
    }
    
    // Now process any sensors that report ready 
    for (int i = 0; i < m_num_sensors; i++) {
        if (m_sensors[i]->ready(&next_check_ms)) {
            // Sensors might take some time to respond, only report if 
            // sense() reports data ready
            bool done = m_sensors[i]->sense();
            if (done) {
                Serial.print("Sensor ");
                Serial.print(i);
                Serial.print(" (");
                Serial.print(m_sensors[i]->getDescription());
                Serial.println(") reporting to base");
                m_sensors[i]->report();
            }
            
            // Don't sleep if any sensor reported ready. 
            do_sleep = false;
        } else {
            // Sensor is not ready. Update sleep scheduling.
            sleep_until = min(sleep_until, next_check_ms); 
            
            // If this sensor supports an interrupt, then record that
            // NOTE: This is hard coded to support Arduinos that have 2 
            // interrupts numbered 0 and 1. Any other system will need 
            // different logic. 
            int int_num = m_sensors[i]->getInterrupt();
            if (int_num == 0 || int_num == 1) {
                interrupts[int_num] = true;
            }
        }
    }
    
    // If we are not a repeater node and no sensor reported ready, then 
    // sleep until the earliest reported next_check_ms value or an interrupt
    if (do_sleep && !repeaterMode) {
        unsigned long sleep_length;
        unsigned long before = millis();
        if (sleep_until == SLEEP_UNTIL_INTERRUPT 
                && (interrupts[0] || interrupts[1])) {
            sleep_length = 0; // sleep() interprets 0 as "forever"
        } else {
            // convert to relative time
            sleep_length = sleep_until - before; 
        }
        
        // Call sleep callback (if defined) and as long as it doesn't cancel
        // go to sleep. 
        if (m_sleep_callback == NULL || m_sleep_callback()) {
            Serial.print("Going to sleep until: ");
            Serial.println(sleep_until);
            
            // Deal with various interrupt scenarios (hard coded for 2-interrupt 
            // Arduinos)
            bool interrupt_wake = false;
            if (interrupts[0] && interrupts[1]) {
                interrupt_wake = (sleep(0, CHANGE, 1, CHANGE, sleep_length) >= 0);
            } else if (interrupts[0]) {
                interrupt_wake = sleep(0, CHANGE, sleep_length);
            } else if (interrupts[1]) {
                interrupt_wake = sleep(1, CHANGE, sleep_length);
            } else {
                sleep(sleep_length);
            }
            
            // sleeping... zzzz...
            // 
            // ... awake!
            
            // Fix up millis() time now that we're back (sleep() resets the 
            // clock to 0, which causes all sorts of issues with sensors)
            if (!interrupt_wake) {
                // we slept for the complete time
                setMillis(sleep_until);
            } else {
                // woken up by interrupt. the only thing we know for certain 
                // is that it is "later", but we don't know by how much.
                setMillis(before + 1);
            }
            
            // Call wake callback if defined
            if (m_wake_callback != NULL) {
                m_wake_callback(interrupt_wake);
            }
            
            Serial.print("...and we're back. Current time is: ");
            Serial.println(millis());
        }
    }
}

// Set millis() value; necessary because sleeping resets the timer value, but
// many sensors need a monotonically increasing value from millis() (possibly 
// rolling over very occasionally)
// From https://tomblanch.wordpress.com/2013/07/27/resetting_millis/
extern volatile unsigned long timer0_millis;
void Node::setMillis(unsigned long new_millis) {
    uint8_t oldSREG = SREG;
    cli();
    timer0_millis = new_millis;
    SREG = oldSREG;
}

// Handle incoming packets from the network
void Node::processIncoming(const MyMessage &msg) 
{
    int i;
    Serial.print("Incoming packet for device ");
    Serial.println(msg.sensor);
    
    for (i = 0; i < m_num_sensors; i++) {
        // Give each sensor a chance to handle the packet. 
        // If a sensor returns true, it has completely handled the 
        // packet.
        if (m_sensors[i]->react(msg)) {
            Serial.print("\tHandled by sensor ");
            Serial.print(i);
            Serial.print(" (");
            Serial.print(m_sensors[i]->getDescription());
            Serial.println(")");
            break;
        }
    }
    if (i == m_num_sensors) Serial.println("\tNot handled or global.");
}


// 
// Device
//
Device::Device(uint8_t type, uint8_t dtype, uint8_t id, uint8_t num_sub_devices, const char *description) 
        : m_num_sub_devices(num_sub_devices)
{
    if (num_sub_devices > MAX_SUB_DEVICES) {
        // error
        Serial.println("Too many devices specified");
        return;
    }
    
    for (int i = 0; i < num_sub_devices; i++) {
        setId(id, i);
        if (id != AUTO) {
            id++;
        }
        setType(type, i);
        setDataType(dtype, i);
        setDescription(description, i);
    }
}
        
uint8_t Device::getId(uint8_t sub_device) 
{
    return m_sub[sub_device].device_id;
}

void Device::setId(uint8_t new_id, uint8_t sub_device)
{
    m_sub[sub_device].device_id = new_id;
    m_sub[sub_device].msg.setSensor(new_id);
}

uint8_t Device::getType(uint8_t sub_device)
{
    return m_sub[sub_device].sensor_type;
}

void Device::setType(uint8_t new_type, uint8_t sub_device)
{
    m_sub[sub_device].sensor_type = new_type;
}

uint8_t Device::getDataType(uint8_t sub_device)
{
    return m_sub[sub_device].data_type;
}

void Device::setDataType(uint8_t new_type, uint8_t sub_device)
{
    m_sub[sub_device].data_type = new_type;
    m_sub[sub_device].msg.setType(new_type);
}

const char *Device::getDescription(uint8_t sub_device)
{
    return m_sub[sub_device].description;
}

void Device::setDescription(const char *new_description, uint8_t sub_device)
{
    m_sub[sub_device].description = new_description;
}

MyMessage &Device::getMessage(uint8_t sub_device)
{
    return m_sub[sub_device].msg;
}

uint8_t Device::getNumSubDevices()
{
    return m_num_sub_devices;
}

