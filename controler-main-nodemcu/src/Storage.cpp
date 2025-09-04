#include "Storage.h"
#include "MQTT.h"
#include "Main.h"
#include <EEPROM.h>
#include <Arduino.h>

Storage storage;

void Storage::begin() {
    EEPROM.begin(EEPROM_SIZE);
    
    if (!isConfigured()) {
        DEBUG_PRINT_LN("Storage: First time setup, storing default values");
        // Clear EEPROM first
        for (int i = 0; i < EEPROM_SIZE; i++) {
            EEPROM.write(i, 0);
        }
        
        // Store default values
        storeBrokerIP(MQTT_SERVER);
        storeMQTTPort(MQTT_PORT);
        storeMQTTUser(MQTT_USER);
        storeMQTTPass(MQTT_PASS);
        storeMQTTTempTopic(MQTT_TOPIC_CONTROL_TEMP);
        storeMQTTEnabled(false);
        storeBlynkEnabled(true);
        
        // Mark as configured
        EEPROM.write(CONFIG_FLAG_ADDR, CONFIGURED_FLAG);
        EEPROM.commit();
    } else {
        DEBUG_PRINT_LN("Storage: EEPROM already configured");
    }
}

String Storage::readString(int startAddr) {
    char str[50];
    int i;
    
    DEBUG_PRINT("Storage: Reading string from addr ");
    DEBUG_PRINT_LN(startAddr);
    
    // Clear buffer first
    memset(str, 0, sizeof(str));
    
    // Read until null terminator or max length
    for (i = 0; i < (int)sizeof(str) - 1; i++) {
        char c = EEPROM.read(startAddr + i);
        if (c == '\0') break;
        if (!isprint(c)) {
            DEBUG_PRINT("Storage: Found invalid char at pos ");
            DEBUG_PRINT_LN(i);
            break;
        }
        str[i] = c;
    }
    str[i] = '\0';
    
    DEBUG_PRINT("Storage: Read string: ");
    DEBUG_PRINT_LN(str);
    return String(str);
}

void Storage::writeString(int startAddr, const char* str, int maxLen) {
    // First, clear the area
    for (int i = 0; i < maxLen; i++) {
        EEPROM.write(startAddr + i, 0);
    }
    
    int len = strlen(str);
    if (len >= maxLen) len = maxLen - 1;
    
    DEBUG_PRINT("Storage: Writing string to addr ");
    DEBUG_PRINT(startAddr);
    DEBUG_PRINT(" len=");
    DEBUG_PRINT_LN(len);
    
    // Write string including null terminator
    for (int i = 0; i < len; i++) {
        EEPROM.write(startAddr + i, str[i]);
    }
    EEPROM.write(startAddr + len, '\0');  // Explicit null terminator
    EEPROM.commit();  // Commit after each write
    
    DEBUG_PRINT("Storage: Wrote string: ");
    DEBUG_PRINT_LN(str);
}

void Storage::storeBrokerIP(const char* ip) {
    DEBUG_PRINT("Storage: Storing broker IP: ");
    DEBUG_PRINT_LN(ip);
    // Write IP string to EEPROM
    writeString(IP_ADDR_START, ip, 32);
    EEPROM.commit();
}

String Storage::loadBrokerIP() {
    // Read IP string from EEPROM
    String ip = readString(IP_ADDR_START);
    DEBUG_PRINT("Storage: Loaded broker IP: ");
    DEBUG_PRINT_LN(ip);
    
    return String(ip);
}

bool Storage::isConfigured() {
    return EEPROM.read(CONFIG_FLAG_ADDR) == CONFIGURED_FLAG;
}

void Storage::resetToDefaults() {
    DEBUG_PRINT_LN("Storage: Resetting to defaults");
    EEPROM.write(CONFIG_FLAG_ADDR, 0xFF);
    EEPROM.commit();
    begin();
}

void Storage::storeMQTTEnabled(bool enabled) {
    EEPROM.write(MQTT_ENABLED_ADDR, enabled ? 1 : 0);
    EEPROM.commit();
}

bool Storage::loadMQTTEnabled() {
    return EEPROM.read(MQTT_ENABLED_ADDR) == 1;
}

void Storage::storeBlynkEnabled(bool enabled) {
    EEPROM.write(BLYNK_ENABLE_ADDR, enabled ? 1 : 0);
    EEPROM.commit();
}

bool Storage::loadBlynkEnabled() {
    return EEPROM.read(BLYNK_ENABLE_ADDR) == 1;
}

void Storage::storeMQTTPort(uint16_t port) {
    EEPROM.put(MQTT_PORT_ADDR, port);
    EEPROM.commit();
}

uint16_t Storage::loadMQTTPort() {
    uint16_t port;
    EEPROM.get(MQTT_PORT_ADDR, port);
    return port;
}

void Storage::storeMQTTUser(const char* user) {
    writeString(MQTT_USER_ADDR, user, 50);
    EEPROM.commit();
}

String Storage::loadMQTTUser() {
    return readString(MQTT_USER_ADDR);
}

void Storage::storeMQTTPass(const char* pass) {
    writeString(MQTT_PASS_ADDR, pass, 50);
    EEPROM.commit();
}

String Storage::loadMQTTPass() {
    return readString(MQTT_PASS_ADDR);
}

void Storage::storeMQTTTempTopic(const char* topic) {
    writeString(MQTT_TEMP_TOPIC_ADDR, topic, 100);
    EEPROM.commit();
}

String Storage::loadMQTTTempTopic() {
    return readString(MQTT_TEMP_TOPIC_ADDR);
}