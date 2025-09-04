#ifndef BYLNK_CENTRALA
#define BYLNK_CENTRALA


// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent();
void initialize_blynk();
void setup_blynk();
void loop_blynk();
bool getBynkStatus();
void setBlynkStatus(bool enabled);  

#endif //BYLNK_CENTRALA