// -------- includes --------
#include <ArduinoOTA.h>
#include "Main.h"
#include "OTAHandler.h"
#include "Secrets.h"
// -------- Global variables --------

// -------- Function declarations --------
void setup_ota() {
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  // ota password
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {  // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    DEBUG_PRINT_LN("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    DEBUG_PRINT_LN("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    DEBUG_PRINT("Progress:");
    DEBUG_PRINT_LN(progress / (total / 100));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINT("Error: ");
    DEBUG_PRINT_LN(error);
    if (error == OTA_AUTH_ERROR) {
      DEBUG_PRINT_LN("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      DEBUG_PRINT_LN("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      DEBUG_PRINT_LN("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      DEBUG_PRINT_LN("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      DEBUG_PRINT_LN("End Failed");
    }
  });
  ArduinoOTA.begin();
}

void loop_ota() {
  ArduinoOTA.handle();
}