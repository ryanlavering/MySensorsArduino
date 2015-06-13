#include <Arduino.h>
#include <SensorNode.h>
#include <Sensors.h>

//
// Heartbeat Sensor
//
HeartBeatSensor::HeartBeatSensor(uint8_t device_id, unsigned long interval)
        : IntervalSensor(interval, S_LIGHT, V_LIGHT, device_id),
            m_last_state(false)
{
    // pass
}

bool HeartBeatSensor::sense() 
{
    // Toggle state
    m_last_state = !m_last_state;
    return true; 
}

bool HeartBeatSensor::report(MySensor *gw) 
{
    return gw->send(getMessage().set(m_last_state));
}

#if 1

//
// DHT22 Sensor
//
DHT22Sensor::DHT22Sensor(uint8_t device_id, int sensor_pin, unsigned long interval)
        : IntervalSensor(interval, S_TEMP, V_TEMP, device_id, 2), 
            m_sensor_pin(sensor_pin)
{
    // Set up second sub_device (index 1)
    setType(S_HUM, 1);
    setDataType(V_HUM, 1);
    
    // Set up DHT sensor
    m_dht.setup(sensor_pin);
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
    
    m_lastTemp = temp;
    m_lastHum = hum;
    
    return new_data;
}

bool DHT22Sensor::report(MySensor *gw)
{
    bool all_ok = true;
    
    printf("Sending temp (DHT22) %d...", (int)m_lastTemp);
    all_ok &= gw->send(getMessage(0).set(m_lastTemp, 1));

    printf("Sending humidity (DHT22) %d...", (int)m_lastHum);
    all_ok &= gw->send(getMessage(1).set(m_lastHum, 1));
    
    return all_ok;
}

#endif