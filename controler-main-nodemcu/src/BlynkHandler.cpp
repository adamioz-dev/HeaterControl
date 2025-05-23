#include "ProjVersion.h"
#include "Secrets.h"

#ifdef BLYNK_ENABLE

#define BLYNK_FIRMWARE_VERSION      PROJECT_STR_VERSION

#define TIMER_TASK_BLYNK_INTERVAL_MS   1000L
#include <Arduino.h>
#include "Common.h"
#include <BlynkSimpleEsp8266.h>
#include "WebServerHandler.h"
#include "Main.h"
#include "BlynkHandler.h"
#include <ESP8266httpUpdate.h>

// Your WiFi credentials for Blynk.
const char* ssid;
const char* pass;

bool blynk_initialized = false;

// wifi client instance
WiFiClient client;

//Blynk task timer
BlynkTimer taskBlynk_timer;

// ******* Externals *********//
extern String processFeedbackCode();
// the variable used
extern ComTransmitData comTransmitData;
/* flag for com error */
extern bool communication_error;


BLYNK_WRITE(InternalPinOTA) {
  String overTheAirURL = "";
  overTheAirURL = param.asString();
  HTTPClient http;
  http.begin(client, overTheAirURL);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK)
  {return;}
  int contentLength = http.getSize();
  if (contentLength <= 0) 
  {return; }
  bool canBegin = Update.begin(contentLength);
  if (!canBegin) 
  { return;}
  Client& client = http.getStream();
  int written = Update.writeStream(client);
  if (written != contentLength) 
  {return;}
  if (!Update.end()) 
  {return;}
  if (!Update.isFinished()) 
  {return;}
  ESP.restart();
}

// heating timer state request
BLYNK_WRITE(V0)
{
  // Set incoming value from pin V0 to a variable
  int newRequest = param.asInt();
  (void)comTransmitData.setHeating_TriggerRequest((uint8_t)newRequest, true);
  DEBUG_PRINT_LN("Blynk: Pressed V0");
}

// set heater value
BLYNK_WRITE(V1)
{
  /* slide offset control mode settings*/
  const int local_middle_value =    100;
  const int local_step_value =      30;
  /* Init variables */
  int local_heating_mode =          comTransmitData.getHeating_mode();
  int local_current_setpoint_scaled = 0;
  // Set incoming value from pin V1 to a variable
  int newRequest = param.asInt();

  /* Write the values */
  if ((local_heating_mode == TIMER_MODE) || (local_heating_mode == THERMOSTAT_MODE))
  {
    /** get the current setting for timer */
    //local_current_setpoint_scaled = (int)comTransmitData.getHeatingTimer_offtime();

    DEBUG_PRINT("Blynk: Target off time for timer from blynk: ");
    DEBUG_PRINT_LN(newRequest);

    // steps 10 by 10
    int remainder_modulo = (newRequest % 10);
    if (remainder_modulo < 5){
      // round down by 10
      local_current_setpoint_scaled = newRequest - remainder_modulo;
    }
    else
    {
      // round up by 10
      local_current_setpoint_scaled = newRequest + (10 - remainder_modulo);
    }

    /* limit values */
    if (local_current_setpoint_scaled > 200)
    {
      local_current_setpoint_scaled = 200;
    }
    else if (local_current_setpoint_scaled <= 0)
    {
      local_current_setpoint_scaled = 0;
    }

    (void)comTransmitData.setHeatingTimer_offtime((uint16_t)local_current_setpoint_scaled, true);
    /* position the slide to the selected value */
    Blynk.virtualWrite(V1, local_current_setpoint_scaled);
  }
  else if (local_heating_mode == TEMPERATURE_MODE)
  {
    /** get the current setting for temperature */
    local_current_setpoint_scaled = (int)((float)comTransmitData.getHeatingTermostat_target_ambient() * FLOAT_SCALING);

    DEBUG_PRINT("Blynk: Target room temp update from blynk: ");
    DEBUG_PRINT_LN(newRequest);

    // positive number
    if (newRequest > local_middle_value)
    {
      // identify scale
      if (newRequest >= (local_middle_value + (local_step_value * 3)))
      {
        /* step increase level 4 */
        local_current_setpoint_scaled = local_current_setpoint_scaled + 50;
      }
      else if (newRequest >= (local_middle_value + (local_step_value * 2)))
      {
        /* step increase level 3 */
        local_current_setpoint_scaled = local_current_setpoint_scaled + 10;
      }
      else if (newRequest >= (local_middle_value + local_step_value))
      {
        /* step increase level 2 */
        local_current_setpoint_scaled = local_current_setpoint_scaled + 5;
      }
      else if (newRequest >= local_middle_value)
      {
        /* step increase level 1 */
        local_current_setpoint_scaled = local_current_setpoint_scaled + 1;
      }
    }
    /* negative request */
    else if (newRequest < local_middle_value)
    {
      // identify scale
      if (newRequest <= (local_middle_value - (local_step_value * 3)))
      {
        /* step increase level 4 */
        local_current_setpoint_scaled = local_current_setpoint_scaled - 50;
      }
      else if (newRequest <= (local_middle_value - (local_step_value * 2)))
      {
        /* step increase level 3 */
        local_current_setpoint_scaled = local_current_setpoint_scaled - 10;
      }
      else if (newRequest <= (local_middle_value - local_step_value))
      {
        /* step increase level 2 */
        local_current_setpoint_scaled = local_current_setpoint_scaled - 5;
      }
      else if (newRequest <= local_middle_value)
      {
        /* step increase level 1 */
        local_current_setpoint_scaled = local_current_setpoint_scaled - 1;
      }
    }
    else
    {
      /* Do nothing */
    }

    /* limit values */
    if (local_current_setpoint_scaled > 300)
    {
      local_current_setpoint_scaled = 300;
    }
    else if (local_current_setpoint_scaled <= 100)
    {
      local_current_setpoint_scaled = 100;
    }
    /* write value for temperature */
    (void)comTransmitData.setHeatingTermostat_target_ambient((uint16_t)local_current_setpoint_scaled, true);

    /* set back to middle */
    Blynk.virtualWrite(V1, local_middle_value);
  }
  else
  {
    DEBUG_PRINT("Blynk: Unknown mode! ");
    DEBUG_PRINT_LN(local_heating_mode);
  }

  DEBUG_PRINT_LN("Blynk: Changed V1");
}

// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
  DEBUG_PRINT_LN("Blynk: connected!");
}

// This function sends Arduino's uptime every second to Virtual Pin 2.
void myTimerEvent()
{
  float send_deviation_th = 0.1f;

  // previous values
  static float local_puffer_temp_prev = 0.0f;
  static float local_boiler_temp_prev = 0.0f;
  static int local_heating_on_time_prev = 0;
  static int local_heating_off_time_prev = 0;
  static float local_room_temp_prev = 0.0f;
  static float local_target_room_temp_prev = 0.0f;
  static float local_remainingTimeInMode_sec_prev = 0.0f;
  static int local_heater_trigger_request_prev = 0;
  static bool communication_error_prev = false;
  static int local_feedbackCode_prev = 0;
  static uint8_t local_room_temp_valid_prev = false;
  static uint8_t local_min_prev = 0;
  static uint8_t local_ext_thermostat_prev = 255; // invalid value

  String temperatures_blynk = "";
  String feedback_blynk = "";
  String heater_information_blynk ="";
  // mode
  int local_heating_mode =      comTransmitData.getHeating_mode();
  //timer
  uint16_t local_remainingTimeInMode_sec = comTransmitData.getHeatingTimer_TimeRemaining_sec();

  int local_heating_on_time =  comTransmitData.getHeatingTimer_ontime();
  int local_heating_off_time =  comTransmitData.getHeatingTimer_offtime();
  // termometer
  float local_room_temp = comTransmitData.getHeatingTermostat_AmbientTemp();
  float local_room_temp_bat = comTransmitData.getHeatingTermostat_AmbientTempBattery();
  float local_target_room_temp = comTransmitData.getHeatingTermostat_target_ambient();
  bool local_room_temp_valid = comTransmitData.getHeatingTermostat_AmbientTemp_Valid();
  bool local_ext_thermostat_state = comTransmitData.getHeatingExternalTermostat_State();

  // measured temps
  float local_puffer_temp =       comTransmitData.getPuffer_Temp();
  float local_boiler_temp =       comTransmitData.getBoiler_Temp();
  // trigger state
  int local_heater_trigger_request = comTransmitData.getHeating_TriggerRequest();
  // feedback
  int local_feedbackCode = comTransmitData.get_Feedback();

  temperatures_blynk += ("P:" + String(local_puffer_temp, 1) + "°   "); // puffer temp
  temperatures_blynk += ("C:" + String(local_boiler_temp, 1) + "°"); // boiler temp

  uint16_t local_hours = 0u;
  uint8_t  local_min = 0u;
  uint8_t  local_sec = 0u;
  secondsToHMS(local_remainingTimeInMode_sec, local_hours, local_min, local_sec);

  if (local_heating_mode == TIMER_MODE)
  {

    if (local_heating_off_time == 0){
      // constant mode
      heater_information_blynk += "Constant mode!";
    }
    else
    {
      // display remaining time
      if (local_heater_trigger_request == HEATER_ON)
      {
        if (local_hours > 0){
          heater_information_blynk += String(local_hours) + "h";
        }
        if (local_min > 0){
          if ((local_min < 10) && (local_hours > 0)){ // when hours are available and 1 digit only
            heater_information_blynk += "0"; //add leading 0
          }
          heater_information_blynk += String(local_min) + "' ";
        }
        // last 2 min display seconds
        if (local_remainingTimeInMode_sec <= 120){
          if (local_sec < 10){  // 1 digit only
            heater_information_blynk += "0"; //add leading 0
          }
          heater_information_blynk += String(local_sec) + "\" ";
        }
      }
      else
      {
        heater_information_blynk += "OFF";
      }
      // display settings of timer
      heater_information_blynk += (" (" + String(local_heating_on_time) + "|" +String(local_heating_off_time) + "')");
    }// else for constant mode
    // send when changed 
    if (((local_remainingTimeInMode_sec <= 120) && (local_remainingTimeInMode_sec != local_remainingTimeInMode_sec_prev)) || 
      (local_heating_off_time != local_heating_off_time_prev) ||
      (local_heating_on_time != local_heating_on_time_prev) ||
      (local_heater_trigger_request_prev != local_heater_trigger_request) ||
      (local_min_prev != local_min) || 
      (local_room_temp_valid != local_room_temp_valid_prev) || 
      ((local_room_temp < (local_room_temp_prev - send_deviation_th)) || (local_room_temp > (local_room_temp_prev + send_deviation_th))))
    {
      // display temperature in room, if known
      if (local_room_temp_valid == true && (local_room_temp != 0)){
        heater_information_blynk += " " + (String(local_room_temp,1) + "° ");
        local_room_temp_valid_prev = local_room_temp_valid;

        if (local_room_temp_bat <= LOW_BAT_WARN)
        {
          heater_information_blynk += "[LOW]";
        }
      }else{
        // temp not known, don't display
        local_room_temp_valid_prev = 255; // invalid value, to trigger resend in case of mode change
      }

      local_min_prev = local_min;
      local_heating_on_time_prev = local_heating_on_time;
      local_heating_off_time_prev = local_heating_off_time;
      local_remainingTimeInMode_sec_prev = local_remainingTimeInMode_sec;
      // inforamtion field
      Blynk.virtualWrite(V4, heater_information_blynk);
    }
    local_ext_thermostat_prev = 255; // invalid value, to trigger resend in case of mode change
  }
  else if (local_heating_mode == TEMPERATURE_MODE)
  {
    // in termostat mode
    if (local_room_temp_valid == true && (local_room_temp != 0)){
      heater_information_blynk = (String(local_room_temp,1) + "° (" + String(local_target_room_temp,1) + "°) ");
    }else{
      heater_information_blynk = ("--.-°C (" + String(local_target_room_temp,1) + "°) ");
    }
    // display remaining time as well
    if (local_heater_trigger_request == HEATER_ON)
    {
      if (local_hours > 0){
        heater_information_blynk += String(local_hours) + "h";
      }
      if (local_min > 0){
        if ((local_min < 10) && (local_hours > 0)){ // when hours are available and 1 digit only
          heater_information_blynk += "0"; //add leading 0
        }
        heater_information_blynk += String(local_min) + "' ";
      }
      // last 2 min display seconds
      if (local_remainingTimeInMode_sec <= 120){
        if (local_sec < 10){  // 1 digit only
          heater_information_blynk += "0"; //add leading 0
        }
        heater_information_blynk += String(local_sec) + "\" ";
      }
    }
    else
    {
      heater_information_blynk += "OFF";
    }
    if (local_room_temp_bat <= LOW_BAT_WARN)
    {
      heater_information_blynk += " [LOW]";
    }
    // send when changed 
    if (((local_room_temp < (local_room_temp_prev - send_deviation_th)) || (local_room_temp > (local_room_temp_prev + send_deviation_th))) || 
      ((local_target_room_temp != local_target_room_temp_prev )) ||
      ((local_room_temp_valid != local_room_temp_valid_prev)) ||
      ((local_remainingTimeInMode_sec <= 120) && (local_remainingTimeInMode_sec != local_remainingTimeInMode_sec_prev)) || 
      (local_heater_trigger_request_prev != local_heater_trigger_request) ||
      (local_min_prev != local_min))
    {
      // store last sent data info 
      local_min_prev = local_min;
      local_remainingTimeInMode_sec_prev = local_remainingTimeInMode_sec;
      local_room_temp_prev = local_room_temp;
      local_target_room_temp_prev = local_target_room_temp;
      local_room_temp_valid_prev = local_room_temp_valid;
      // inforamtion field
      Blynk.virtualWrite(V4, heater_information_blynk);
    }
    local_ext_thermostat_prev = 255; // invalid value, to trigger resend in case of mode change
    local_remainingTimeInMode_sec_prev = 255; // invalid value, to trigger resend in case of mode change
  }
  else if (local_heating_mode == THERMOSTAT_MODE)
  {
    // send when changed 
    if (local_ext_thermostat_prev != local_ext_thermostat_state)
    {
      if (local_ext_thermostat_state == true)
      {
        heater_information_blynk ="Thermostat ON";
        // inforamtion field
        Blynk.virtualWrite(V4, heater_information_blynk);
      }
      else
      {
        heater_information_blynk ="Thermostat OFF";
        // inforamtion field
        Blynk.virtualWrite(V4, heater_information_blynk);
      }
      // update prev value
      local_ext_thermostat_prev = local_ext_thermostat_state;
    }
    local_room_temp_valid_prev = 255; // invalid value, to trigger resend in case of mode change
    local_remainingTimeInMode_sec_prev = 255; // invalid value, to trigger resend in case of mode change
  }

  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  // Update on blynk
  if (local_heater_trigger_request != local_heater_trigger_request_prev)
  {
    // update previous 
    local_heater_trigger_request_prev = local_heater_trigger_request;
    //request status
    Blynk.virtualWrite(V0, local_heater_trigger_request);
  }

  // update on blynk if value is different
  if (communication_error != communication_error_prev)
  {
    // update the send flag
    communication_error_prev = communication_error;
    // send new values 
    Blynk.virtualWrite(V4, heater_information_blynk);
  }

  static int refresh_counter = 0;
  if (((local_puffer_temp < (local_puffer_temp_prev - send_deviation_th)) || (local_puffer_temp > (local_puffer_temp_prev + send_deviation_th))) || 
      ((local_boiler_temp < (local_boiler_temp_prev - send_deviation_th)) || (local_boiler_temp > (local_boiler_temp_prev + send_deviation_th))) || 
      (refresh_counter >= 3600)) // send at least once per hour
  {
    // clear counter at sending
    refresh_counter = 0;

    // update perviously send values
    local_puffer_temp_prev = local_puffer_temp;
    local_boiler_temp_prev = local_boiler_temp;
    // temperature
    Blynk.virtualWrite(V2, temperatures_blynk);
  }
  else
  {
    // increase refresh counter
    refresh_counter += 1;
  }

  // get current feedback
  if (local_feedbackCode != local_feedbackCode_prev)
  {
    local_feedbackCode_prev = local_feedbackCode;
    // send new value 
    feedback_blynk = processFeedbackCode();
    // feedback info
    Blynk.virtualWrite(V3, feedback_blynk); 
  }
}

void initialize_blynk(){
  if (WiFi.status() == WL_CONNECTED){
    ssid = WiFi.SSID().c_str();
    pass = WiFi.psk().c_str();

    if(client.connect(BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT) == true)
    {
      client.stop();
      Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass); /// stuck in loop when no internet is connected

      // Setup a function to be called every second
      taskBlynk_timer.setInterval(TIMER_TASK_BLYNK_INTERVAL_MS, myTimerEvent);
      blynk_initialized = true;
    }
    else{
      client.stop();
    }
  }
}


void setup_blynk(){
  initialize_blynk();
}

void loop_blynk(){
  if ((WiFi.status() == WL_CONNECTED)){/// && ){
    if (blynk_initialized){
      if(client.connect(BLYNK_DEFAULT_DOMAIN, BLYNK_DEFAULT_PORT) == true)
      {
        client.stop();

        Blynk.run();  /// stuck in loop when no internet is connected
        taskBlynk_timer.run();
      }
      else
      {
        client.stop();
      }
    }
    else{
      initialize_blynk();
    }
  }
}
#else // BLYNK_ENABLE
// dummy functions for blynk
// to be used when blynk is not enabled
void setup_blynk(){}
void loop_blynk(){}
void initialize_blynk(){}
void myTimerEvent(){}
#endif // BLYNK_ENABLE