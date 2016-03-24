#ifndef RKL_SENSORS_H
#define RKL_SENSORS_H

#include <SensorNode.h>
#include <math.h>

// Base class for interval-based sensors
class IntervalSensor : public Sensor {
    public:
        IntervalSensor(Node *gw, unsigned long interval, uint8_t type, uint8_t dtype, uint8_t id=AUTO, uint8_t num_sub_devices=1, const char *description="");
        virtual bool ready(unsigned long *next_check_ms=NULL);
        unsigned long nextTimeout() { return m_next_timeout; }
    
    private:
        unsigned long m_interval; // interval, in ms
        unsigned long m_next_timeout; // when does the next timeout occur?
        //unsigned long m_avg_runtime;
};


// Periodic Presentation meta-sensor
#define PRESENTATION_PERIOD (5*60*1000UL)
class PresentationMetaSensor : public IntervalSensor {
    public:
        PresentationMetaSensor(Node *gw, unsigned long interval = PRESENTATION_PERIOD);
        // Always report that sensor reading available
        virtual bool sense() { return true; }
        virtual bool report();
};


// Heart Beat sensor -- sends a fake LED code to the base station at a given 
// interval (default 5s)
class HeartBeatSensor : public IntervalSensor {
    public:
        HeartBeatSensor(Node *gw, uint8_t device_id = AUTO, unsigned long interval = 5000);
                       
        virtual bool sense();
        virtual bool report();
        
    private:
        bool m_last_state; // last state (on/off)
};

#if 1
#include "../DHT/DHT.h"

// DHT22 Sensor
// Dual-device sensor
// Report temp and humidity at specific interval (default 5s)
// Default pin is 5. 'Cause that's what the example for the DHT lib uses.
#define DHT_SENSOR_DIGITAL_PIN 5

class DHT22Sensor : public IntervalSensor {
    public:
        DHT22Sensor(Node *gw, uint8_t device_id=AUTO, 
                    int sensor_pin=DHT_SENSOR_DIGITAL_PIN,
                    unsigned long interval=5000);
        
        virtual bool sense();
        virtual bool report();
    
    private:
        DHT m_dht;
        int m_sensor_pin;
        float m_lastTemp;
        float m_lastHum;
        unsigned long m_nextsample;
};
#endif


//
// 8x8 LED Panel 
//
#include "../LedControl/src/LedControl.h"
#include "../RKL_Icons/RKLIcons.h"

// Default pin definitions
#define LED_DATA_IN_PIN 8
#define LED_CLK_PIN 6
#define LED_LOAD_PIN 7

// Sub-device indices
#define LED_PANEL 0
#define LED_ICON 1

// Brightness constraints
#define LED_PANEL_MIN_LEVEL 0
#define LED_PANEL_MAX_LEVEL 15

#define FLIP(i) (7-i)

// Supports one device for panel on/off brightness and one for icon
// Icons supported by the RKL_Icons library
class SimpleIconLedMatrix : public Sensor {
    public:
        SimpleIconLedMatrix(Node *gw, uint8_t device_id=AUTO,
                    int data_in_pin=LED_DATA_IN_PIN, 
                    int clk_pin=LED_CLK_PIN, 
                    int load_pin=LED_LOAD_PIN);
                    
        // Need a custom presentation to work around issue with 
        // Domoticz/MySensors support w/ S_DIMMER devices
        virtual bool present();
        virtual bool react(const MyMessage &msg);
        
        void draw(const byte *ico);
        
    private:
        LedControl m_lc;
        uint8_t m_brightness;
        uint8_t m_icon;
};


//
// PIR Presence Sensor
//
#define PRESENCE_SENSOR_PIN 3 // The digital input you attached your motion sensor.  (Only 2 and 3 generates interrupt!)
#define PRESENCE_SENSOR_WARM_UP (15*1000UL) // warm up time
#define PRESENCE_ONLY_SEND_ON 0 // if 1, only send "on" messages; controller will turn off
#define PRESENCE_OFF_DELAY (60*1000UL) // only send off if no "on" signal within 60 seconds
#define PRESENCE_TIMER_OFF (-1UL)
#define PRESENCE_MAX_RETRIES 5
#define PRESENCE_DEBUG 1

class PresenceSensor : public Sensor {
    public:
        PresenceSensor(Node *gw, uint8_t device_id=AUTO,
                    int sense_pin=PRESENCE_SENSOR_PIN,
                    unsigned long off_delay=PRESENCE_OFF_DELAY);
                    
        virtual bool ready(unsigned long *next_check_ms=NULL);
        virtual bool sense();
        virtual bool report();
        
        virtual bool react(const MyMessage& msg);
        
        virtual int getInterrupt();
        
        // Was motion detected recently? Relies on reading from last sense() call.
        bool motionDetected();
        
    private:
        bool m_tripped;
        unsigned long m_warm_up_done;
        int m_sense_pin;
        unsigned long m_off_after;
        unsigned long m_off_delay;
        unsigned long m_last_tripped_at;
};

#if 1
//
// LED Light
//
#define LED_MIN_LEVEL 0
#define LED_MAX_LEVEL 255
//#define LED_MAX_LEVEL 127
#define LED_FADE_AUTO 0.0
#define LED_FADE_DELTA LED_FADE_AUTO // auto-delta
#define LED_FADE_NUMSTEPS 32
#define LED_FADE_DELAY 32
#define LED_ON true
#define LED_OFF false

class LEDLight : public Sensor {
    public:
        LEDLight(Node *gw, int pwm_pin, uint8_t device_id=AUTO);
        virtual bool react(const MyMessage &msg);
        void fade(uint8_t to, float delta=LED_FADE_DELTA, bool raw_value=false);
        void setState(bool on);
        bool getState();
        
    private:
        int m_pin;
        uint8_t m_brightness;
        bool m_on;
};
#endif

#if 0
//
// Analog Photoresistor Light Level Sensor
//
class PhotoResistorSensor : public Sensor {
    public:
        PhotoResistorSensor(Node *gw, int analog_pin, uint8_t device_id=AUTO);
        
        virtual bool ready(unsigned long *next_check_ms=NULL) { return true; }
        virtual bool sense();
        virtual bool report();
        
    private:
        int m_analog_pin;
};
#endif

#if 0
//
// HC-SR04 Ultrasonic Distance Sensor
//
class UltrasonicSensor : public Sensor {
    public:
        UltrasonicSensor(Node *gw, int trigger_pin, int echo_pin, uint8_t device_id=AUTO);
        
        //virtual bool ready(unsigned long *next_check_ms=NULL) { return true; }
        virtual bool sense();
        virtual bool report();
        
    private:
        int m_trigger_pin;
        int m_echo_pin;
};
#endif

#if 1
// Periodic callback sensor 
// Generic interval sensor that will call the given
// sense() and report() callbacks when the interval expires. 
//
// Useful for one-off sketches that need interrupt-friendly periodic logic. 
#define PERIODIC_CALLBACK_INTERVAL 5000
class PeriodicCallback : public IntervalSensor {
    public:
        typedef bool (*SenseCallback)(void);
        typedef bool (*ReportCallback)(void);
        
    public:
        PeriodicCallback(Node *gw, unsigned long interval, 
            SenseCallback sense_func=NULL, ReportCallback report_func=NULL,
            uint8_t device_id=AUTO, const char *description="periodic");
                       
        virtual bool sense();
        virtual bool report();
        
        SenseCallback setSenseCallback(SenseCallback on_sense);
        ReportCallback setReportCallback(ReportCallback on_report);
        
    private:
        SenseCallback m_sense_callback;
        ReportCallback m_report_callback;
};

class IncomingCallback : public Sensor {
    public:
        typedef bool (*ReactCallback)(const MyMessage &msg);
        
    public:
        IncomingCallback(Node *gw, ReactCallback react_func, 
            uint8_t device_id=AUTO, const char *description="react");
        
        virtual bool react(const MyMessage &msg);
        
        ReactCallback setReactCallback(ReactCallback on_react);
        
    private: 
        ReactCallback m_react_callback;
};
#endif

#endif