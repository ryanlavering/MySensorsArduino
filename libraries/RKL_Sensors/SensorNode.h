#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

// Includes
#include "MySensor.h"
#include "MyMessage.h"

// Definitions
// Bump these values if you need more stuff attached to a given node
#define MAX_SENSORS 10
#define MAX_SUB_DEVICES 4

// Give each sensor 4 bytes in EEPROM
#define EEPROM_ADDR(sensor_id) (sensor_id * 4)

#define NOT_IMPLEMENTED -1

// Forward declarations
struct SubDevice;
class Device;
class Sensor;

// Base class implementing basic node logic for sensor nodes
class Node : public MySensor {
    public:
        // Create a new node instance
        Node(uint8_t _cepin=DEFAULT_CE_PIN, uint8_t _cspin=DEFAULT_CS_PIN);
        
        /*
        // Start the node running
        // Takes the same parameters as MySensor::begin() except for the 
        // msgCallback param, which will always be this class's 
        // processIncoming() function.
        void begin(void (* msgCallback)(const MyMessage &)=NULL, uint8_t nodeId=AUTO, boolean repeaterMode=false, uint8_t parentNodeId=AUTO, rf24_pa_dbm_e paLevel=RF24_PA_LEVEL, uint8_t channel=RF24_CHANNEL, rf24_datarate_e dataRate=RF24_DATARATE);
        */
        
        // Add a new device
        bool addSensor(Sensor *sensor, bool do_present=false);
        
        // Present devices
        void present(Device *d, uint8_t sub_device=0, bool ack=false);
        void presentAll(bool ack=false);
        
        // Update the node scheduler. 
        // Looks for any work to be done by querying the network for 
        // incoming packets, asking each sensor if it has pending data,
        // and processing any interruptors. 
        void update();
        
        // Process incoming messages by passing them on to listening reactors.
        //
        // Due to the way that MySensor callbacks are handled, it is not 
        // possible to directly pass this function as a callback (there's no 
        // way to pass the class instance pointer). Instead, you should 
        // add a function like the following to your sketch, and then 
        // pass "incomingMessage" to Node::begin().
        //
        // void incomingMessage(const MyMessage &msg) {
        //     node.processIncoming(msg); // where 'node' is your Node instance
        // }
        void processIncoming(const MyMessage &msg);
        
    protected:
        // Set automatic device id(s)
        void assignDeviceIds(Device *d);
        
    private:
        Sensor *m_sensors[MAX_SENSORS];
        int m_num_sensors;
        int next_device_id;
};


//
// Device
//
// Handles logic related to a logical MySensors device, which may include 
// multiple sub-sensors (for example, a DHT22 sensor, which encapsulates a 
// temperature and humidity sensor) 
// This is the building block for the higher-level "Sensor" and "Reactor" 
// classes.
//

struct SubDevice {
    uint8_t device_id;
    uint8_t sensor_type;
    uint8_t data_type;
    MyMessage msg;
};

class Device {
    public:
        Device(uint8_t type, uint8_t dtype, uint8_t id=AUTO, uint8_t num_sub_devices=1);
        
        uint8_t getId(uint8_t sub_device=0);
        void setId(uint8_t new_id, uint8_t sub_device=0);
        
        uint8_t getType(uint8_t sub_device=0);
        void setType(uint8_t new_type, uint8_t sub_device=0);
        
        uint8_t getDataType(uint8_t sub_device=0);
        void setDataType(uint8_t new_type, uint8_t sub_device=0);
        
        MyMessage &getMessage(uint8_t sub_device=0);
        
        uint8_t getNumSubDevices();
        
    protected:
        uint8_t m_num_sub_devices;
        SubDevice m_sub[MAX_SUB_DEVICES];
};

// Abstract base class for generic sensors
class Sensor : public Device {
    public:
        Sensor(uint8_t type, uint8_t dtype, uint8_t id=AUTO, uint8_t num_sub_devices=1) : Device(type, dtype, id, num_sub_devices) {};

        // Is data ready to sense?
        // This function should return true if conditions are right to 
        // take a new reading. Used by the scheduler to determine if it should 
        // take a sensor reading. 
        // @param next_check_ms -- [Output] Optionally return number of 
        // ms for next ready time. (I.e. don't check again for x ms)
        virtual bool ready(unsigned long *next_check_ms=NULL) { return false; } 
        
        // Take a reading. 
        // Return true if there is new data to report, or false 
        // if the reading is still in progress or there is no new data to 
        // report.
        virtual bool sense() { return false; } 
        
        // report data upstream 
        // Return true if the upstream data report was completed successfully
        virtual bool report(MySensor *gw) { return false; } 
        
        // Handle the incoming message. 
        // Return true iff the packet was handled and no one else needs to 
        // process it.
        // Return false if the packet is not destined for this reactor, OR if 
        // the packet should be handled by other reactors.
        virtual bool react(const MyMessage &msg) { return false; }
        
    protected:
        // none
};

// Base class for interval-based sensors
class IntervalSensor : public Sensor {
    public:
        IntervalSensor(unsigned long interval, uint8_t type, uint8_t dtype, uint8_t id=AUTO, uint8_t num_sub_devices=1);
        virtual bool ready(unsigned long *next_check_ms=NULL);
        unsigned long nextTimeout() { return m_next_timeout; }
    
    private:
        unsigned long m_interval; // interval, in ms
        unsigned long m_next_timeout; // when does the next timeout occur?
        //unsigned long m_avg_runtime;
};

#endif /* SENSOR_NODE_H */
