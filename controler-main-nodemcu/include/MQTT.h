#ifndef MQTT_H
#define MQTT_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "Secrets.h"
// ************************************************************************
// DEFINES           
// ************************************************************************
// MQTT topics
// controls
#define MQTT_TOPIC_CONTROL_SUBS     "home/heater/control/#"
#define MQTT_TOPIC_CONTROL_STATE    "home/heater/control/state"
#define MQTT_TOPIC_CONTROL_TEMP     "home/heater/control/temperature" // default topic for room temperature
#define MQTT_TOPIC_CONTROL_SETPOINT "home/heater/control/setpoint"   // topic for setting target temperature
// States
#define MQTT_TOPIC_PUBLISH          "home/heater/status"
#define MQTT_TOPIC_PUBLISH_FEEDBACK "home/heater/status/feedback"
#define MQTT_TOPIC_PUBLISH_STATE    "home/heater/status/state"
#define MQTT_TOPIC_PUBLISH_PUFFER   "home/heater/status/puffer"
#define MQTT_TOPIC_PUBLISH_BOILER   "home/heater/status/boiler"
#define MQTT_TOPIC_PUBLISH_REMAIN   "home/heater/status/remaining"
#define MQTT_TOPIC_PUBLISH_ROOM     "home/heater/status/room"    // Topic for reporting current room temperature
#define MQTT_TOPIC_PUBLISH_SETPOINT "home/heater/status/setpoint" // Topic for reporting target temperature
#define MQTT_TOPIC_PUBLISH_MODE     "home/heater/status/mode"     // Topic for reporting current operation mode

//***********************************************************************
// DECLARATIONS
//***********************************************************************
void publishMqtt();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void setup_mqtt();
void loop_mqtt();
//**********************************************************************
// MQTTClient class definition  
//**********************************************************************

class MQTTClient {
public:
    MQTTClient(const char* server, int port, const char* user, const char* pass);
    void begin();
    void loop();
    void publish(const char* topic, const char* payload);
    bool isConnected();
    void setCallback(void (*callback)(char*, byte*, unsigned int));
    String getBrokerIP() { return mqtt_server; }
    void setBrokerIP(const char* server);
    void setPort(uint16_t port);
    void setUser(const char* user);
    void setPassword(const char* pass);
    uint16_t getPort() { return mqtt_port; }
    String getUsername() { return mqtt_user; }
    String getPassword() { return mqtt_password; }
    bool isEnabled() const { return mqtt_enabled; }
    void setEnabled(bool enabled);
    String getMqttTempTopic();
    void setMqttTempTopic(const char* topic);
    
private:
    String mqtt_server;
    uint16_t mqtt_port;
    String mqtt_user;
    String mqtt_password;
    String mqtt_temp_topic;
    WiFiClient espClient;
    PubSubClient client;
    unsigned long lastReconnectAttempt = 0;
    const unsigned long RECONNECT_DELAY = 5000; // 5 seconds
    bool mqtt_enabled = true;  // Default to enabled
    
    void connect();
};

// ************************************************************************
// EXTERNAL VARIABLES
// ************************************************************************
// MQTT client instance
extern MQTTClient mqtt;

#endif