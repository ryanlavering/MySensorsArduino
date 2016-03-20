#ifndef SENSOR_NODE_H
#define SENSOR_NODE_H

// Includes
#include "../MySensors/MySensor.h"
#include "../MySensors/MyMessage.h"

// Definitions
// Bump these values if you need more stuff attached to a given node
#define MAX_SENSORS 10
#define MAX_SUB_DEVICES 2

// Give each sensor 4 bytes in EEPROM
#define EEPROM_ADDR(sensor_id) (sensor_id * 4)

#define NOT_IMPLEMENTED -1

#define SLEEP_UNTIL_INTERRUPT (-1UL)

// Forward declarations
struct SubDevice;
class Device;
class Sensor;

// Base class implementing basic node logic for sensor nodes
class Node : public MySensor {
    public: 
        typedef bool (*SleepCallback)(void);
        typedef void (*WakeCallback)(bool interrupt_wake);
    
    public:
        // Create a new node instance
        Node(MyTransport &radio =*new MyTransportNRF24(), MyHw &hw=*new MyHwDriver()
#ifdef MY_SIGNING_FEATURE
            , MySigning &signer=*new MySigningNone()
#endif
#ifdef WITH_LEDS_BLINKING
            , uint8_t _rx=DEFAULT_RX_LED_PIN,
            uint8_t _tx=DEFAULT_TX_LED_PIN,
            uint8_t _er=DEFAULT_ERR_LED_PIN,
            unsigned long _blink_period=DEFAULT_LED_BLINK_PERIOD
#endif
            );
        
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
        
        // When the node goes to sleep, enable a callback before turning off 
        // the radio and powering down the node. 
        //
        // This can be useful, for example, with a battery powered node that 
        // uses a motion detector to wake up. By providing a delay during
        // shutdown we can allow the voltage regularor time to settle before
        // fully sleeping, which prevents invalid motion detection.
        // 
        // The callback must return a boolean value indicating whether or not
        // the node should continue going to sleep. If the function returns 
        // false the node will NOT continue going to sleep, and the wake 
        // callback (if defined) will NOT be called. 
        //
        // Pass in NULL to disable callback. 
        void setSleepCallback(SleepCallback func_on_sleep) { m_sleep_callback = func_on_sleep; }
        
        // Set wake up callback. This function will be called just after the 
        // node wakes from a sleep. 
        //
        // The function will be passed one boolean parameter indicating 
        // whether the wake was due to an interrupt (true) or the sleep 
        // timer expiring (false). 
        //
        // Pass in NULL to disable callback. 
        void setWakeCallback(WakeCallback func_on_wake) { m_wake_callback = func_on_wake; }
        
        // Expose the raw radio (USE CAUTION WHEN MESSING WITH THIS OR THE 
        // NODE COULD BE LEFT IN AN INVALID STATE!) 
        MyTransport& getRadio() { return radio; }
        
    protected:
        // Set Arduino millis() value
        void setMillis(unsigned long new_millis);
        
    private:
        Sensor *m_sensors[MAX_SENSORS];
        int m_num_sensors;
        int next_device_id;
        SleepCallback m_sleep_callback;
        WakeCallback m_wake_callback;
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
    const char *description; 
    MyMessage msg;
};

class Device {
    public:
        Device(uint8_t type, uint8_t dtype, uint8_t id=AUTO, uint8_t num_sub_devices=1, const char *description="");
        
        uint8_t getId(uint8_t sub_device=0);
        void setId(uint8_t new_id, uint8_t sub_device=0);
        
        uint8_t getType(uint8_t sub_device=0);
        void setType(uint8_t new_type, uint8_t sub_device=0);
        
        uint8_t getDataType(uint8_t sub_device=0);
        void setDataType(uint8_t new_type, uint8_t sub_device=0);
        
        const char *getDescription(uint8_t sub_device=0);
        void setDescription(const char *new_description, uint8_t sub_device=0);
        
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
                    uint8_t num_sub_devices=1, const char *description="") 
                : Device(type, dtype, id, num_sub_devices, description),
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
            if (next_check_ms != NULL) { *next_check_ms = SLEEP_UNTIL_INTERRUPT; }
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
        
        // Interrupt number.
        // @return the interrupt number (as used by sleep() calls, NOT the pin 
        //      number) if one is used by this sensor, otherwise return -1 to 
        //      indicate that no interupt is used. 
        virtual int getInterrupt() { return -1; }
        
    protected:
        Node *m_gw;
};

#endif /* SENSOR_NODE_H */
