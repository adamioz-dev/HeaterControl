#ifndef WEBSERVER_CENTRALA
#define WEBSERVER_CENTRALA

// -------- includes --------
#include "common.h"

// -------- function declarations --------
void connectionCheck();
void secondsToHMS( const uint32_t seconds, uint16_t &hours_out, uint8_t &min_out, uint8_t &sec_out );
void setup_webserver();
void loop_webserver();

#endif  //WEBSERVER_CENTRALA