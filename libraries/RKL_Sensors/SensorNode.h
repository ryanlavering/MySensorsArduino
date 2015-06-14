#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

// Includes
#include "MySensor.h"
#include "MyMessage.h"

// Definitions
// Bump these values if you need more stuff attached to a given node
#define MAX_SENSORS 10
#define MAX_SUB_DEVICES 2

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
        bool addSensor(Sensor *sensor);
        
        // Set automatic device id(s)
        void assignDeviceIds(Device *d);
        
        // Present devices
        void presentDevice(Sensor *s, uint8_t sub_device=0, bool ack=false);
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
        Sensor(Node *gw, uint8_t type, uint8_t dtype, uint8_t id=AUTO, 
                    uint8_t num_sub_devices=1) 
                : Device(type, dtype, id, num_sub_devices),
                    m_gw(gw)
        {
            // pass
        }

        // Do any special presentation logic necessary for this sensor
        // @return true if presentation is completely handled by this method.
        //      If this returns false, then the Node will do default 
        //      presentation logic (It is permissible to do partial 
        //      presentation in this function and return false to finish 
        //      with the default presentation.)
        virtual bool present() { return false; }
        
        // Is data ready to sense?
        // @return true if conditions are right to take a new reading. 
        //      Used by the scheduler to determine if it should 
        //      request a sensor reading. 
        // @param next_check_ms -- [Output] Optionally return absolute time  
        //      (in ms) for next ready time. (I.e. don't check again until 
        //      millis() >= *next_check_ms.)
        //      Only valid if function returns false.
        virtual bool ready(unsigned long *next_check_ms=NULL) 
        { 
            // Default version of this function will never return true, so
            // if the scheduler is paying attention to next_check_ms then 
            // send it the maximum possible value so it won't waste time
            // polling this device.
            if (next_check_ms != NULL) { *next_check_ms = (unsigned long)-1; }
            return false; 
        } 
        
        // Take a reading. 
        // @return true if there is new data to report, or false 
        //      if the reading is still in progress or there is no new data to 
        //      report.
        virtual bool sense() { return false; } 
        
        // report data upstream 
        // @return true if the upstream data report was completed successfully
        virtual bool report() { return false; } 
        
        // Handle the incoming message. 
        // @return true iff the packet was handled and no one else needs to 
        //      process it. Return false if the packet is not destined for 
        //      this reactor, OR if the packet should be handled by other 
        //      reactors.
        virtual bool react(const MyMessage &msg) { return false; }
        
    protected:
        Node *m_gw;
};

#endif /* SENSOR_NODE_H */
