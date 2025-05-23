#ifndef SECRETS
#define SECRETS

#ifdef BLYNK_ENABLE
/* Fill-in information from Blynk Device Info here
    OR fill in in the platformio.ini file */
#ifndef BLYNK_TEMPLATE_ID   
#define BLYNK_TEMPLATE_ID          //ADD_YOUR_BLYNK_TEMPLATE_ID_HERE
#endif
#ifndef BLYNK_TEMPLATE_NAME
#define BLYNK_TEMPLATE_NAME        //ADD_YOUR_BLYNK_TEMPLATE_NAME_HERE 
#endif
#ifndef BLYNK_AUTH_TOKEN
#define BLYNK_AUTH_TOKEN           //ADD_YOUR_BLYNK_AUTH_TOKEN_HERE
#endif
#endif //BLYNK_ENABLE

// Web server settings
#define HTTP_PAGE_USERNAME         "admin"              // username for web page
#define HTTP_PAGE_PASSWORD         "esp8266_http_admin" // password for web page

// access point settings
#define AP_NAME                     "Configurator"
#define AP_PASS                     "23456789"

// OTA settings
#define OTA_HOSTNAME                "centrala.local" // hostname for OTA updates
#define OTA_PASSWORD                "esp8266_admin"  // password for OTA updates

// MQTT default settings
#define MQTT_SERVER "192.168.1.101"
#define MQTT_PORT 1883
#define MQTT_USER "user"
#define MQTT_PASS "passwd"

#endif //SECRETS