#ifndef STORAGE_H
#define STORAGE_H

#include <EEPROM.h>
#include <Arduino.h>

class Storage {
public:
    static void begin();
    static void storeBrokerIP(const char* ip);
    static String loadBrokerIP();
    static void storeMQTTPort(uint16_t port);
    static uint16_t loadMQTTPort();
    static void storeMQTTUser(const char* user);
    static String loadMQTTUser();
    static void storeMQTTPass(const char* pass);
    static String loadMQTTPass();
    static void storeMQTTTempTopic(const char* topic);
    static String loadMQTTTempTopic();
    static bool isConfigured();
    static void resetToDefaults();
    static void storeMQTTEnabled(bool enabled);
    static bool loadMQTTEnabled();
    static void storeBlynkEnabled(bool enabled);
    static bool loadBlynkEnabled();
private:
    static void writeString(int startAddr, const char* str, int maxLen);
    static String readString(int startAddr);
    static const int EEPROM_SIZE = 512;
    static const byte CONFIGURED_FLAG = 0xA5;
    static const int CONFIG_FLAG_ADDR = 0;
    static const int BLYNK_ENABLE_ADDR = 6;
    static const int MQTT_ENABLED_ADDR = 8;
    static const int IP_ADDR_START = 10;
    static const int MQTT_PORT_ADDR = 100;
    static const int MQTT_USER_ADDR = 150;
    static const int MQTT_PASS_ADDR = 200;
    static const int MQTT_TEMP_TOPIC_ADDR = 250;
};

extern Storage storage;

#endif