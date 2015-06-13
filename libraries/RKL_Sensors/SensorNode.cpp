/* RKL Sensor Nodes */

#include <Arduino.h>
#include <SensorNode.h>

// 
// Node Class
//
// Implements logic for scheduling and updates to the devices associated with 
// this node.
//

Node::Node(uint8_t _cepin, uint8_t _cspin)
        : MySensor(_cepin,_cspin)
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

// Present a device
// Will set a device id automatically if necessary.
void Node::present(Device *d, uint8_t sub_device, bool ack) 
{
    // make sure device has a proper ID
    if (d->getId(sub_device) == AUTO) {
        d->setId(next_device_id++, sub_device);
    }
    MySensor::present(d->getId(sub_device), d->getType(sub_device), ack);
}

void Node::presentAll(bool ack)
{
    for (int i = 0; i < m_num_sensors; i++) {
        Sensor *s = m_sensors[i];
        assignDeviceIds(s); // make sure all devices have a proper ID
        for (int j = 0; j < s->getNumSubDevices(); j++) {
            MySensor::present(s->getId(j), s->getType(j), ack);
        }
    }
}

bool Node::addSensor(Sensor *sensor, bool do_present) 
{
    if (sensor == NULL || m_num_sensors >= MAX_SENSORS)
        return false;
    m_sensors[m_num_sensors++] = sensor;
    assignDeviceIds(sensor); // fill in any device IDs that request AUTO
    if (do_present) {
        present(sensor);
    }
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
            printf("Node: Sensor %d reports ready.\n", i);
            // Sensors might take some time to respond, only report if 
            // sense() reports data ready
            bool done = m_sensors[i]->sense();
            if (done) {
                printf("\tReading complete. Reporting to base...");
                m_sensors[i]->report(this);
            }
        }
    }
}

// Handle incoming packets from the network
void Node::processIncoming(const MyMessage &msg) 
{
    int i;
    printf("Node: Incoming packet\n");
    
    for (i = 0; i < m_num_sensors; i++) {
        // Give each sensor a chance to handle the packet. 
        // If a sensor returns true, it has completely handled the 
        // packet.
        if (m_sensors[i]->react(msg)) {
            printf("\tHandled by sensor %d\n", i);
            break;
        }
    }
    if (i == m_num_sensors) printf("\nNot handled or global.\n");
}


// 
// Device
//
Device::Device(uint8_t type, uint8_t dtype, uint8_t id, uint8_t num_sub_devices) 
        : m_num_sub_devices(num_sub_devices)
{
    if (num_sub_devices > MAX_SUB_DEVICES) {
        // error
        printf("Too many devices specified (%d)\n", num_sub_devices);
        return;
    }
    
    for (int i = 0; i < num_sub_devices; i++) {
        setId(id, i);
        if (id != AUTO) {
            id++;
        }
        setType(type, i);
        setDataType(dtype, i);
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

MyMessage &Device::getMessage(uint8_t sub_device)
{
    return m_sub[sub_device].msg;
}

uint8_t Device::getNumSubDevices()
{
    return m_num_sub_devices;
}


//
// Interval sensors
//
IntervalSensor::IntervalSensor(unsigned long interval, uint8_t type, 
            uint8_t dtype, uint8_t id, uint8_t num_sub_devices)
        : Sensor(type, dtype, id, num_sub_devices), m_interval(interval)
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


