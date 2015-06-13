#ifndef RKL_SENSORS_H
#define RKL_SENSORS_H

#include <SensorNode.h>
#include <../DHT/DHT.h>

// Heart Beat sensor -- sends a fake LED code to the base station at a given 
// interval (default 5s)
class HeartBeatSensor : public IntervalSensor {
    public:
        HeartBeatSensor(uint8_t device_id = AUTO, unsigned long interval = 5000);
                       
        virtual bool sense();
        virtual bool report(MySensor *gw);
        
    private:
        bool m_last_state; // last state (on/off)
};

#if 1
// DHT22 Sensor
// Dual-device sensor
// Report temp and humidity at specific interval (default 5s)
// Default pin is 5. 'Cause that's what the example for the DHT lib uses.
#define DHT_SENSOR_DIGITAL_PIN 5

class DHT22Sensor : public IntervalSensor {
    public:
        DHT22Sensor(uint8_t device_id=AUTO, 
                    int sensor_pin=DHT_SENSOR_DIGITAL_PIN,
                    unsigned long interval=5000);
        
        virtual bool sense();
        virtual bool report(MySensor *gw);
    
    private:
        DHT m_dht;
        int m_sensor_pin;
        float m_lastTemp;
        float m_lastHum;
        unsigned long m_nextsample;
};
#endif

#endif