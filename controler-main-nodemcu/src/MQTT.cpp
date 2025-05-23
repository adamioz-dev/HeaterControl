#include "MQTT.h"
#include "Main.h"
#include "Storage.h"
// Add to your main sketch
#include <MQTT.h> 
#include "common.h"

// marker for communication error
extern bool communication_error;

// MQTT client
MQTTClient mqtt(MQTT_SERVER, MQTT_PORT, MQTT_USER, MQTT_PASS);

// ***********************************************************************
// MQTTClient class implementation  
// ***********************************************************************
MQTTClient::MQTTClient(const char* server, int port, const char* user, const char* pass)
    : mqtt_server(server)  // String constructor will handle conversion
    , mqtt_port(port)
    , mqtt_user(user)
    , mqtt_password(pass)
    , client(espClient)
{
    // do nothing
}

void MQTTClient::begin() {
    // Load all stored settings
    String storedIP = storage.loadBrokerIP();
    uint16_t storedPort = storage.loadMQTTPort();
    String storedUser = storage.loadMQTTUser();
    String storedPass = storage.loadMQTTPass();
	if (storedIP.length() > 0) {
        DEBUG_PRINT("MQTT: Loaded stored broker IP: ");
        DEBUG_PRINT_LN(storedIP);
        mqtt_server = storedIP;
    } else {
        DEBUG_PRINT_LN("MQTT: No stored broker IP found, using default");
        mqtt_server = MQTT_SERVER; // Default server IP
        storage.storeBrokerIP(mqtt_server.c_str()); // Store default IP in EEPROM
    }

    if (storedPort == 65535) { // 0xFFFF is the default value for uninitialized EEPROM
        DEBUG_PRINT_LN("MQTT: No stored port found, using default");
        storedPort = MQTT_PORT;
        storage.storeMQTTPort(storedPort); // Store default port in EEPROM
    } else {
        DEBUG_PRINT("MQTT: Loaded stored port: ");
        DEBUG_PRINT_LN(storedPort);
        mqtt_port = storedPort;
    }
    if (storedUser.length() > 0) {
        DEBUG_PRINT("MQTT: Loaded stored user: ");
        DEBUG_PRINT_LN(storedUser);
        mqtt_user = storedUser;
    } else {
        DEBUG_PRINT_LN("MQTT: No stored user found, using default");
        mqtt_user = MQTT_USER; // Default user
        storage.storeMQTTUser(mqtt_user.c_str()); // Store default user in EEPROM
    }

    if (storedPass.length() > 0) {
        DEBUG_PRINT("MQTT: Loaded stored password: ");
        DEBUG_PRINT_LN(storedPass);
        mqtt_password = storedPass;
    } else {
        DEBUG_PRINT_LN("MQTT: No stored password found, using default");
        mqtt_password = MQTT_PASS; // Default password
        storage.storeMQTTPass(mqtt_password.c_str()); // Store default password in EEPROM
    }

    client.setServer(mqtt_server.c_str(), mqtt_port);
    // Check if MQTT is enabled in EEPROM and connect if so
    mqtt_enabled = storage.loadMQTTEnabled();
    if (!mqtt_enabled) {
        DEBUG_PRINT_LN("MQTT: Starting disabled");
        return;
    }
    connect();
}

void MQTTClient::loop() {
    if (!mqtt_enabled) return;
    if (!client.connected()) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt > RECONNECT_DELAY) {
            lastReconnectAttempt = now;
            connect();
        }
    }
    client.loop();
}

void MQTTClient::publish(const char* topic, const char* payload) {
    // allow publishing only if connected and no communication error with the uno
    if (client.connected() && communication_error == false) {
        client.publish(topic, payload);
    }
}

bool MQTTClient::isConnected() {
    return client.connected();
}

void MQTTClient::setCallback(void (*callback)(char*, byte*, unsigned int)) {
    client.setCallback(callback);
}

void MQTTClient::connect() {
    if (client.connect("HeaterController", mqtt_user.c_str(), mqtt_password.c_str())) {
        // Note: Use c_str() when passing to PubSubClient methods
        client.setServer(mqtt_server.c_str(), mqtt_port);
        // Subscribe to control topics
        client.subscribe(MQTT_TOPIC_CONTROL_SUBS);
        
        // Publish connection status
        client.publish(MQTT_TOPIC_PUBLISH, "connected");
    }
}

void MQTTClient::setBrokerIP(const char* server) {
    if (client.connected()) {
        client.disconnect();
    }
    mqtt_server = server;  // String assignment will handle conversion
    // Store new IP in EEPROM
    storage.storeBrokerIP(mqtt_server.c_str());
    // Set the new server address
    client.setServer(mqtt_server.c_str(), mqtt_port);
}

void MQTTClient::setPort(uint16_t port) {
    if (client.connected()) {
        client.disconnect();
    }
    mqtt_port = port;
    storage.storeMQTTPort(port);
    client.setServer(mqtt_server.c_str(), mqtt_port);
    connect();
}

void MQTTClient::setUser(const char* user) {
    mqtt_user = user;
    storage.storeMQTTUser(user);
}

void MQTTClient::setPassword(const char* pass) {
    mqtt_password = pass;
    storage.storeMQTTPass(pass);
}

void MQTTClient::setEnabled(bool enabled) {
    mqtt_enabled = enabled;
    if (enabled == false) {
        DEBUG_PRINT_LN("MQTT: Disabling connection");
        if (client.connected()) {
            client.disconnect();
        }
    } else if (enabled) {
        DEBUG_PRINT_LN("MQTT: Enabling connection");
    }
    // Store the enabled state in EEPROM
    storage.storeMQTTEnabled(enabled);
}

// ***********************************************************************
// FUNCTIONS
// ***********************************************************************
void publishMqtt() {
    if (!mqtt.isEnabled()) {
        DEBUG_PRINT_LN("MQTT: Disabled, skipping publish");
        return;
    }
    else if (!mqtt.isConnected()) {
        DEBUG_PRINT_LN("MQTT: Not connected, skipping publish");
        return;
    }

    // Publish temperature
    char temp[50];
    
    String current_feedback = processFeedbackCode();
    mqtt.publish(MQTT_TOPIC_PUBLISH_FEEDBACK, current_feedback.c_str()); 

    // Publish state
    boolean trigger_state = (boolean)comTransmitData.getHeating_TriggerRequest();
    String current_req_state = "OFF";
    if (trigger_state == false) { 
        current_req_state = "OFF";
    } else if (trigger_state == true) {
        current_req_state = "ON";
    } 
    mqtt.publish(MQTT_TOPIC_PUBLISH_STATE, current_req_state.c_str());
    
    // Publish puffer temperature
    float pufferTemp = comTransmitData.getPuffer_Temp();
    dtostrf(pufferTemp, 4, 1, temp);
    mqtt.publish(MQTT_TOPIC_PUBLISH_PUFFER, temp);
    
    // Publish boiler temperature
    float boilerTemp = comTransmitData.getBoiler_Temp();
    dtostrf(boilerTemp, 4, 1, temp);
    mqtt.publish(MQTT_TOPIC_PUBLISH_BOILER, temp);
    
    // Publish remaining time
    uint16_t remaining_sec = (uint16_t)comTransmitData.getHeatingTimer_TimeRemaining_sec();
    String(remaining_sec).toCharArray(temp, sizeof(temp));
    mqtt.publish(MQTT_TOPIC_PUBLISH_REMAIN, temp);

    DEBUG_PRINT_LN("MQTT: Publish complete");
}

// Callback function for incoming MQTT messages
// This function is called when a message arrives on a subscribed topic
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (!mqtt.isEnabled()) {
        DEBUG_PRINT_LN("MQTT: Disabled, skipping callback");
        return;
    }
    else if (!mqtt.isConnected()) {
        DEBUG_PRINT_LN("MQTT: Not connected, skipping callback");
        return;
    }
    DEBUG_PRINT_LN("MQTT: Callback triggered");
    DEBUG_PRINT("MQTT: Topic: ");   
    DEBUG_PRINT_LN(topic);

    // Handle incoming messages
    String message = String((char*)payload).substring(0, length);
    DEBUG_PRINT("MQTT: Message: ");
    DEBUG_PRINT_LN(message); 

    // handle control messages
    if (String(topic) == MQTT_TOPIC_CONTROL_STATE) {
        if (message == "ON") {
            comTransmitData.setHeating_TriggerRequest(HEATER_ON, true);
        } else if (message == "OFF") {
            comTransmitData.setHeating_TriggerRequest(HEATER_OFF, true);
        }
    }
    // handle measured temperatures received
    else if (String(topic) == MQTT_TOPIC_CONTROL_TEMP) {
        if (isValidNumber(message)) {
            float targetTemp = message.toFloat();
            receiveRoomTemp(targetTemp);
        }
    }
}

//************************************************************************
// SETUP AND LOOP FUNCTIONS
//************************************************************************

// Function to setup MQTT client
void setup_mqtt() {
    // Initialize MQTT client
    storage.begin();
    mqtt.begin();
    mqtt.setCallback(mqttCallback);
}

void loop_mqtt() {
    // Call the MQTT loop function to handle incoming messages and maintain connection
    mqtt.loop();
}
//************************************************************************