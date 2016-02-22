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
    while (process()) {
        // move data through the network
        // See processIncoming()
    }
    
    // Now process any sensors that report ready 
    for (int i = 0; i < m_num_sensors; i++) {
        if (m_sensors[i]->ready()) {
            // Sensors might take some time to respond, only report if 
            // sense() reports data ready
            bool done = m_sensors[i]->sense();
            if (done) {
                Serial.print("Sensor ");
                Serial.print(i);
                Serial.println(" reporting to base");
                m_sensors[i]->report();
            }
        }
    }
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
            Serial.print("\tHandled by sensor");
            Serial.println(i);
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

