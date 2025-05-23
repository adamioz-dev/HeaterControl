// *********************************************************************
// INCLUDES
// *********************************************************************
#include "Main.h"
#include <EEPROM.h>
#include "ProjVersion.h"
#include "MQTT.h"
// *********************************************************************
// DEFINES
// *********************************************************************

// *********************************************************************
// DATATYPES
// *********************************************************************
struct {
  uint32_t flash_request;
  uint32_t flash_attempts;
  uint32_t reserved[3];
} rtcData;

typedef struct {
  // heating parameters
  float heating_puffer_coef =       200;
  float heating_result_multiplier = 2;
  // cooling params
  float cooling_room_coef =         75;
  float cooling_result_multiplier = 12;
}tempAproxCalib_t;

// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************
// temp aproximation calib data
tempAproxCalib_t calibData;

//room temperature timeout counter
uint16_t roomTempUpdateTimeout_cnt = 0u;

// intervals for scheduler
unsigned long g_fast_task_interval_cnt =    0; 
unsigned long g_mid_task_interval_cnt =     0; 
unsigned long g_slow_task_interval_cnt =    0; 
unsigned long g_minute_task_interval_cnt =  0; 

// comunicated values 
ComTransmitData comTransmitData;

// uptime counter
unsigned long g_esp_uptime = 0; 

// data received marker
bool data_received = false;

// marker for communication error, starup flag set as error present
bool communication_error = true;

//webserver request flags 
bool uno_flash_req_flag = false;
bool uno_reset_req_flag = false;
bool esp_reset_req_flag = false;

/* Init value */
flash_resp_t flash_status = FLASH_INFO_NOT_AVAIL;

/* extern stuff */
extern bool AP_mode_active; 

// *********************************************************************
// FUNCTION DECLARATIONS 
// *********************************************************************
void versionCheck();
void mqttCallback(char* topic, byte* payload, unsigned int length);

// *********************************************************************
// FUNCTIONS
// *********************************************************************

void reset_uno(){
  // reset the uno controler
  DEBUG_PRINT_LN("Reset the AVR UNO");
  digitalWrite(UNO_RESET_PIN, LOW); // when pulled low, the uno will reset
  delay(100);
  digitalWrite(UNO_RESET_PIN, HIGH); // return to normal
  delay(50);
}

void check_uno(){
  // must be called slower than the uni will report the time
  uint32_t local_uno_uptime = comTransmitData.get_unoUptime();
  static uint32_t local_uno_uptime_prev = 0;
  static uint8_t local_reset_timeou_cnt = 0;

  static uint8_t local_reset_attepmts_cnt = 0;
  // uptime did not change
  if (local_uno_uptime_prev == local_uno_uptime)
  {
    // reset after the sepecified periods (in 10s of seconds)
    if ((local_reset_timeou_cnt >= UNO_RESET_WAIT) && (local_reset_attepmts_cnt <= UNO_RESET_TRIES))
    {
      // timeout of communication with uno, watchdog of uno failed to reset, communication not restored
      DEBUG_PRINT_LN("Resetting uno!");
      reset_uno();
      // clear reset counter
      local_reset_timeou_cnt = 0;
      // increment reset attempts
      local_reset_attepmts_cnt += 1;      
    }
    else if (local_reset_attepmts_cnt > UNO_RESET_TRIES)
    {
      DEBUG_PRINT_LN("Resettings failed, flash uno!");
      // attempt flashing
      flash_uno_req();
      // clear max reset counter, uno will be re-flashed 
      local_reset_attepmts_cnt = 0;
    }
    else
    {
      /* counter not finished yet */
      // uno is not sending data, increase reset counter
      local_reset_timeou_cnt++;
    }

    /* update reported state with error message */
    communication_error = true;
    /* set com error */
    (void)comTransmitData.set_Feedback((uint8_t)UNO_COM_ERR);
  }
  else
  {
    // connection seems ok, clear reset counter as well
    local_reset_attepmts_cnt = 0;
    // comunication restored, clear counter
    local_reset_timeou_cnt = 0;
    /* update reported state with error message */
    communication_error = false;
    /* Com error wil be cleared with the new feedback received */
  }

  // save previous uptime
  local_uno_uptime_prev = local_uno_uptime;
}

String processFeedbackCode(){
  uint8_t local_feedback_code = comTransmitData.get_Feedback();
  String returnText = "";
  /* normal operation */
  switch(local_feedback_code)
  {
    case HEATING_OFF: 
      {
      returnText = F("Heating off."); //heat off
      }
      break;
    case HEATING_ON:
      {
      returnText = F("Heating on!"); //heat on
      }
      break;
    case PUFFER_TEMP_LOW:
      {
      returnText = F("Puffer too cold!"); //low temp
      }
      break;
    case PUFFER_TEMP_OVERHEAT:
      {
      returnText = F("Puffer too hot!"); //high temp
      }
      break;
    case UNO_COM_ERR:
      {
      returnText = F("Control error!"); // uno not responding
      }
      break;
    default:
      {
      returnText = F("Unknown!");
      }
      break;  
  }
  
  return returnText;
}

void receiveValue(uint8_t id, uint32_t value){
  // set the value of the received variable
  //DEBUG_PRINT("Received:");
  //DEBUG_PRINT(id);
  //DEBUG_PRINT("=");
  //DEBUG_PRINT_LN(value);
  switch (id)
  {
    case _ID_HEATER_MODE:
    {
      //DEBUG_PRINT("UNO: Mode request ");
      //DEBUG_PRINT_LN(value);
      (void)comTransmitData.setHeating_mode((uint8_t)value);
      break;
    }
    case _ID_TIMER_ON_TIME:
    {
      //DEBUG_PRINT("UNO: Timer on ");
      //DEBUG_PRINT_LN(value);
      (void)comTransmitData.setHeatingTimer_ontime((uint16_t)value);
      break;
    }
    case _ID_TIMER_OFF_TIME:
    {
      //DEBUG_PRINT("UNO: Timer off ");
      //DEBUG_PRINT_LN(value);
      (void)comTransmitData.setHeatingTimer_offtime((uint16_t)value);
      break;
    }
    case _ID_AMBIENT_TARGET_TEMP:
    {
      //DEBUG_PRINT("Target room temp update from UNO: ");
      //DEBUG_PRINT_LN(value);
      (void)comTransmitData.setHeatingTermostat_target_ambient((uint16_t)value);
      break;
    }
    case _ID_AMBIENT_HYSTERESIS:
    {
      (void)comTransmitData.setHeatingTermostat_hysteresis((uint8_t)value);
      break;
    }
    case _ID_PUFFER_MIN_TEMP:
    {
      (void)comTransmitData.setPuffer_minTemp((uint8_t)value);
      break;
    }
    case _ID_PUFFER_MAX_TEMP:
    {
      (void)comTransmitData.setPuffer_maxTemp((uint8_t)value);
      break;
    }
    case _ID_PUFFER_HYSTERESIS:
    {
      (void)comTransmitData.setPuffer_hysteresis((uint8_t)value);
      break;
    }
    /// runtime values
    case _ID_HEATER_TRIGGER_REQUEST:
    {
      // here is received, don't set the send flag
      (void)comTransmitData.setHeating_TriggerRequest((uint8_t)value);
      break;
    }
    case _ID_TIMER_REMAINING_SEC:
    {
      // receive only, except in case of uno reset-restore
      (void)comTransmitData.setHeatingTimer_TimeRemaining_sec((uint16_t)value);
      break;
    }
    case _ID_AMBIENT_ACTUAL_ROOM_TEMP:  
    {
      // room temperature
      (void)comTransmitData.setHeatingTermostat_AmbientTemp((uint16_t)value, false);
      break;
    }
    case _ID_AMBIENT_ACTUAL_ROOM_TEMP_VALID:
    {
      // room temp valid received
      (void)comTransmitData.setHeatingTermostat_AmbientTemp_Valid((uint8_t)value, false);
      break;
    }
    case _ID_THERMOSTAT_STATE:
    {
      //DEBUG_PRINT("UNO: External thermostat state ");
      //DEBUG_PRINT_LN(value);
      // termostat external pin state
      (void)comTransmitData.setHeatingExternalTermostat_State((uint8_t)value);
      break;
    }
    case _ID_PUFFER_TEMP:
    {
      // receive only
      (void)comTransmitData.setPuffer_Temp((uint16_t)value);
      break;
    }
    case _ID_HEATER_TEMP:
    {
      // receive only
      (void)comTransmitData.setBoiler_Temp((uint16_t)value);
      break;
    }

    case _ID_FEEDBACK_CODE:
    {
      // receive only
      (void)comTransmitData.set_Feedback((uint8_t)value);
      break;
    }
    case _ID_IP_ADDRESS:
    {
      // send only
      break;
    }
    case _ID_UPTIME_UNO:
    {
      // receive only
      (void)comTransmitData.set_unoUptime((uint32_t)value);
      //DEBUG_PRINT("Uptime received:");
      //DEBUG_PRINT_LN(value);
      break;
    }
    case _ID_DATA_GROUP1:
    {
      // send only
      break;
    }
    case _ID_DATA_GROUP2:
    {
      // send only
      break;
    }
    case _ID_VERSION:
    {
      // Receive uno specific data
      version_data_t version_data_temp_store; // temp variable 
      version_data_t version_data_temp_recv; // temp variable 
      // get to temp variable
      version_data_temp_recv.all = (uint32_t)value;

      // get relevant data only, uno version
      version_data_temp_store.version_byte[1] = version_data_temp_recv.version_byte[1]; // uno version

      version_data_temp_store.version_byte[0] = (uint8_t)PROJECT_MAJOR; // keep original
      version_data_temp_store.version_byte[2] = (uint8_t)PROJECT_ESP;   // keep original
      version_data_temp_store.version_byte[3] = (uint8_t)flash_status;  // keep original
      // store received version
      (void)comTransmitData.set_version_all((uint32_t)version_data_temp_store.all, false);
      break;
    }
    case _ID_LAST:
    {
      // Check if the values are requested and the esp been already running and received data already
      if (((uint8_t)value == 1) && (g_esp_uptime > 60) && (data_received == true)) 
      {
        DEBUG_PRINT_LN("Data was requested. Sending last know data.");
        // data was requested from uno
        // send the confirmation that termostat data is received
        sendPacket(_ID_AMBIENT_ACTUAL_ROOM_TEMP, (uint16_t)(comTransmitData.getHeatingTermostat_AmbientTemp() * FLOAT_SCALING));
        sendPacket(_ID_AMBIENT_ACTUAL_ROOM_TEMP_VALID, (uint8_t)(comTransmitData.getHeatingTermostat_AmbientTemp_Valid()));
        // last known heater request
        sendPacket(_ID_HEATER_TRIGGER_REQUEST, (uint8_t)comTransmitData.getHeating_TriggerRequest()); 
        //timer
        sendPacket(_ID_HEATER_MODE, (uint8_t)comTransmitData.getHeating_mode());
        sendPacket(_ID_TIMER_ON_TIME, (uint16_t)comTransmitData.getHeatingTimer_ontime());
        sendPacket(_ID_TIMER_OFF_TIME, (uint16_t)comTransmitData.getHeatingTimer_offtime());
        sendPacket(_ID_TIMER_REMAINING_SEC, (uint16_t)comTransmitData.getHeatingTimer_TimeRemaining_sec());
        // termostat
        sendPacket(_ID_AMBIENT_TARGET_TEMP, (uint16_t)(comTransmitData.getHeatingTermostat_target_ambient() * FLOAT_SCALING));
        sendPacket(_ID_AMBIENT_HYSTERESIS, (uint8_t)(comTransmitData.getHeatingTermostat_hysteresis() * FLOAT_SCALING));
        // puffer
        sendPacket(_ID_PUFFER_MIN_TEMP,(uint8_t)comTransmitData.getPuffer_minTemp());
        sendPacket(_ID_PUFFER_MAX_TEMP, (uint8_t)comTransmitData.getPuffer_maxTemp());
        sendPacket(_ID_PUFFER_HYSTERESIS, (uint8_t)comTransmitData.getPuffer_hysteresis());
        // send version
        sendPacket(_ID_VERSION, (uint32_t)comTransmitData.get_version_all());
      }
    }
    default:
    {
      break;
    }
  }
  data_received = true;
}

void toogle_led(){
  static bool led_status = false;
  // blink led when alive 
  digitalWrite(LEDPIN, led_status); // turn on led 
  led_status = !led_status;
}

void receiveRoomTemp(float room_temperature)
{
  // float scaling applies for 19.5 send 195
  (void)comTransmitData.setHeatingTermostat_AmbientTemp((uint16_t)(room_temperature * FLOAT_SCALING), true);
  // set valid temp flag
  (void)comTransmitData.setHeatingTermostat_AmbientTemp_Valid((bool)true , true);
  /* reset timeout counter */
  roomTempUpdateTimeout_cnt = 0;
  //call the temp aprox fuction
  aproximateRoomTemp(); 
}

void receiveRoomTempBattery(float battery_volt)
{
  // float scaling applies for 2.8 send 28
  (void)comTransmitData.setHeatingTermostat_AmbientTempBattery((uint8_t)(battery_volt * FLOAT_SCALING));
}

uint16_t calculateTimeToIncreaseTemp(float local_puffer_temp, float local_room_temp)
{
  // calculate time in minutes required to increase temperature by 0.1 C
  uint16_t local_timeStep = (uint16_t)((1 / (local_puffer_temp - local_room_temp) * calibData.heating_puffer_coef) * calibData.heating_result_multiplier);

  DEBUG_PRINT("Calculated time step for increase: "); 
  DEBUG_PRINT_LN(local_timeStep);
  
  return local_timeStep;
}

uint16_t calculateTimeToDecreaseTemp(float local_room_temp)
{
  // calculate time in minutes required to decrease temperature by 0.1 C
  uint16_t local_timeStep =  (uint16_t)((1 / local_room_temp * calibData.cooling_room_coef) * calibData.cooling_result_multiplier);
  
  DEBUG_PRINT("Calculated time step for decrease: "); 
  DEBUG_PRINT_LN(local_timeStep);
  
  return local_timeStep;
}

/* aproximate room temp between measurement points (each min)*/
void aproximateRoomTemp()
{
  static uint16_t local_timeSinceLastTempUpdateByStep = 0;
  static bool local_heating_status_prev = true;
  static uint16_t local_calculatedTempStep = 0u;
  static float local_room_temperature_received = 0;
  static uint8_t local_send_counter = 0;
  static float local_room_temperature_aprox = 0;

  float local_room_temperature = 0;
  uint8_t local_feedback_code = HEATING_OFF;
  bool local_heating_status = false;
  bool local_temperature_data_valid = false;

  float local_puffer_temp = 0;

  local_puffer_temp = comTransmitData.getPuffer_Temp();
  local_room_temperature = comTransmitData.getHeatingTermostat_AmbientTemp();
  local_feedback_code =  (uint8_t)comTransmitData.get_Feedback();
  local_temperature_data_valid = comTransmitData.getHeatingTermostat_AmbientTemp_Valid();

  // check if data is valid
  if((local_temperature_data_valid == false) || (local_room_temperature == 0))
  {
    //DEBUG_PRINT("Room temp not valid -");
    //DEBUG_PRINT_LN(local_temperature_data_valid);
    //DEBUG_PRINT(" T: ");
    //DEBUG_PRINT_LN(local_room_temperature);
    // no valid temp, exit
    local_room_temperature_aprox = 0;
    return;
  }

  // identify what is the heating state
  if ((local_feedback_code == HEATING_OFF) || (local_feedback_code == PUFFER_TEMP_LOW))
  {
    // no heating
    local_heating_status = false;
  }
  else if ((local_feedback_code == HEATING_ON) || (local_feedback_code == PUFFER_TEMP_OVERHEAT))
  {
    // heating is on
    local_heating_status = true;
  }

  // Check if timeout is reached
  if (roomTempUpdateTimeout_cnt >= ROOM_TEMP_TIMEOUT)
  {
    DEBUG_PRINT_LN("Room temp timeout reached.");
    // mark room temp data as not valid and send to uno
    (void)comTransmitData.setHeatingTermostat_AmbientTemp_Valid((bool)false, true);
    // set 0 room temp for esp
    (void)comTransmitData.setHeatingTermostat_AmbientTemp((uint16_t)0, false); 
    // clear 
    local_timeSinceLastTempUpdateByStep = 0;
    local_calculatedTempStep = 0;
    // switch to timer mode
    (void)comTransmitData.setHeating_mode(TIMER_MODE, true);
  }
  else  // normal operation, room temp is not timeout
  {
    if (roomTempUpdateTimeout_cnt == 0)
    {
      /// TODO: compare with actual time and adjust the calibData for increase or decrease accordingly

      DEBUG_PRINT("Room temp was updated: ");
      DEBUG_PRINT_LN(local_room_temperature);

      // just received the temperature, clear counter
      local_timeSinceLastTempUpdateByStep = 0;
      if (local_heating_status == true) 
      {
        // calculate time needed increase temperature by 0.1 C while heating
        local_calculatedTempStep = calculateTimeToIncreaseTemp(local_puffer_temp, local_room_temperature);
      }
      else
      {
        // calculate time needed decrease temperature by 0.1 C while cooling
        local_calculatedTempStep = calculateTimeToDecreaseTemp(local_room_temperature);
      }
      // save received temperature
      local_room_temperature_received = local_room_temperature;
      // set calulated aprox room temp to the received value
      local_room_temperature_aprox = local_room_temperature;
    }

    // increment counter for time spent in the last state
    local_timeSinceLastTempUpdateByStep++;
    // increment timeout counter 
    roomTempUpdateTimeout_cnt++;

    // time passed, increase aproximate temp
    if (local_timeSinceLastTempUpdateByStep >= local_calculatedTempStep)
    {
      DEBUG_PRINT_LN("Time step reached.");
      DEBUG_PRINT("Current aprox temp: ");
      DEBUG_PRINT_LN(local_room_temperature_aprox);
      // calculate aproximate room temp while heating
      if (local_heating_status == true) 
      {
        // calculate next time needed increase temperature by 0.1 C
        local_calculatedTempStep = calculateTimeToIncreaseTemp(local_puffer_temp, local_room_temperature_aprox);
        // clear counter
        local_timeSinceLastTempUpdateByStep = 0;
        
        // limit the max increase to 3 C above received value
        if (local_room_temperature_aprox <= local_room_temperature_received + 3)
        {
          // aproximate room temp increase
          local_room_temperature_aprox = local_room_temperature_aprox + 0.1; // increase by 0.1 C
          DEBUG_PRINT("Increase aproximate temp.:");
          DEBUG_PRINT_LN(local_room_temperature_aprox);
        }
      }
      else // no heating
      {
        // calculate next time needed decrease temperature by 0.1 C
        local_calculatedTempStep = calculateTimeToDecreaseTemp(local_room_temperature_aprox);
        // clear counter
        local_timeSinceLastTempUpdateByStep = 0;

        // limit the max decrease to 3 C above received value
        if (local_room_temperature_aprox >= local_room_temperature_received - 3)
        {
          // aproximate room temp decrease
          local_room_temperature_aprox = local_room_temperature_aprox - 0.1; // decrease by 0.1 C
          DEBUG_PRINT("Decrease aproximate temp.:");
          DEBUG_PRINT_LN(local_room_temperature_aprox);
        }
      }
      // send updated value
      DEBUG_PRINT_LN("Send new aproximation.");
      (void)comTransmitData.setHeatingTermostat_AmbientTemp((uint16_t)(local_room_temperature_aprox * FLOAT_SCALING), true); 
      // reset counter
      local_send_counter = 0;
    }
    
    // feedback code changed, heating state changed
    if (local_heating_status != local_heating_status_prev)
    {
      DEBUG_PRINT_LN("Heating state changed");
      // clear counter
       local_timeSinceLastTempUpdateByStep = 0u;
      // update the prev heating status
      local_heating_status_prev = local_heating_status;

      if (local_heating_status == true) 
      {
        // calculate time needed increase temperature by 0.1 C while heating
        local_calculatedTempStep = calculateTimeToIncreaseTemp(local_puffer_temp, local_room_temperature_aprox);
      }
      else
      {
        // calculate time needed decrease temperature by 0.1 C while cooling
        local_calculatedTempStep = calculateTimeToDecreaseTemp(local_room_temperature_aprox);
      }
    }

    if (local_send_counter > 10){ // every 10 min
      DEBUG_PRINT("Send periodic the temperature: ");
      DEBUG_PRINT_LN(local_room_temperature_aprox);
      // send value periodically
      (void)comTransmitData.setHeatingTermostat_AmbientTemp((uint16_t)(local_room_temperature_aprox * FLOAT_SCALING), true); 
      // reset counter
      local_send_counter = 0;
    }
    // increase counter
    local_send_counter++;
  }
}

void handleWebServerRequests(void)
{
  if (uno_flash_req_flag == true)
  {
    /* clear flag */
    uno_flash_req_flag = false;
    // perform flash request
    flash_uno_req();
  }
  if (uno_reset_req_flag == true)
  {
    /* clear flag */
    uno_reset_req_flag = false;
    // perform reset 
    reset_uno();
  }
    if (esp_reset_req_flag == true)
  {
    /* clear flag */
    esp_reset_req_flag = false;
    // perform reset 
    ESP.restart();
  }
}

// *************************************************
//                  Tasks                        
// *************************************************
void fast_task()
{
  //uptime
  g_esp_uptime = (unsigned long)(millis() / 1000);

  // ota loop
  loop_ota();
}

void middle_task()
{

  // blynk loop for application
  loop_blynk();
  // normal running toggle led slow
  toogle_led();
  /* request data groupt 1*/
  sendPacket(_ID_DATA_GROUP1, (uint8_t)1); 

  /* process flags set by web interface */
  handleWebServerRequests();
}

void slow_task()
{
  // request group 2 
  sendPacket(_ID_DATA_GROUP2, (uint8_t)1); 

  // check for connection to the internet
  connectionCheck();
  // check if uno is alive
  check_uno();
  // check if version updated and update firmware
  versionCheck();
  // publish data to MQTT
  publishMqtt();
}

void minute_task()
{
  // webserver data 
  loop_webserver();
  // calculate the aproximate room temp (from last known)
  aproximateRoomTemp();


  /* TODO: Implement heating/fire detection? */
}

// *************************************************
//                  Scheduler                    
// *************************************************

void scheduler(){
  unsigned long local_currentMillis = millis();
  // fast task
  if((local_currentMillis - g_fast_task_interval_cnt) >= FAST_TASK_PERIOD) 
  {
    g_fast_task_interval_cnt = local_currentMillis; 
    // execute task
    fast_task();
  }

  // every second
  if((local_currentMillis - g_mid_task_interval_cnt) >= MID_TASK_PERIOD) 
  {
    g_mid_task_interval_cnt = local_currentMillis; 
    // execute task
    middle_task();
  }

  // every 10 s
  if((local_currentMillis - g_slow_task_interval_cnt) >= SLOW_TASK_PERIOD) 
  {
    g_slow_task_interval_cnt = local_currentMillis; 
    // execute tasks
    slow_task();
  }

  // every 1 min
  if((local_currentMillis - g_minute_task_interval_cnt) >= MINUTE_TASK_PERIOD) 
  {
    g_minute_task_interval_cnt = local_currentMillis; 
    // execute tasks
    minute_task();
  }
}

void flash_uno_req(void)
{
  /* Set variable to trigger flashing at startup  */
  rtcData.flash_request = 1u;
  if (ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData))) {
    DEBUG_PRINT("RTC flag set ");
    DEBUG_PRINT_LN();
  }
  /* reset, fhasling will be done at startup if rtc variable is set */
  ESP.restart();
}

void checkRTCData()
{
/* checkl if need to flash the uno */
  if (ESP.rtcUserMemoryRead(0, (uint32_t *)&rtcData, sizeof(rtcData)))
  {
    DEBUG_PRINT("RTC data available ");
    DEBUG_PRINT_LN(rtcData.flash_request);
    /* flashing was requested */
    if (rtcData.flash_request == 1u) 
    {
      /* perform flash */
      // if flash fails, try again, for 10 times
      while ((FLASH_SUCCESS != flash_status) && (rtcData.flash_attempts < 5))
      {
        rtcData.flash_attempts += 1;
        flash_status = flash_uno();
      }
      DEBUG_PRINT("Perform flash attempts ");
      DEBUG_PRINT_LN(rtcData.flash_attempts);
      DEBUG_PRINT("Flash code written: ");
      DEBUG_PRINT_LN(flash_status);
      // clear flash attempts for next try
      rtcData.flash_attempts = 0u;

      version_data_t version_data_temp_store; // temp variable 
      /* get the current data */
      version_data_temp_store.all = comTransmitData.get_version_all();
      // Change the status byte
      version_data_temp_store.version_byte[3] = (uint8_t)flash_status; // set the new status
      // send data on com and store
      (void)comTransmitData.set_version_all((uint32_t)version_data_temp_store.all, true);

      /* clear flash request */
      rtcData.flash_request = 0u;
      if (ESP.rtcUserMemoryWrite(0, (uint32_t *)&rtcData, sizeof(rtcData))) {
        DEBUG_PRINT("RTC flag cleared ");
        DEBUG_PRINT_LN();
      }
    }
    else
    {
      DEBUG_PRINT_LN("Normal startup!");
      /* normal startup */
    }
  }
  else
  {
    DEBUG_PRINT_LN("RTC read return false!");
  }
}

void versionCheck()
{
  static bool uno_version_checked = false;

  if (uno_version_checked)
  {
    // version already checked, write version periodically and send on com
    (void)comTransmitData.send_version_all();
    // version already checked
    return;
  }
  // wait if com error 
  if (communication_error == true)
  {
    // keep relay disabled while communication is off
    pinMode(RELAY_ENABLE_PIN, OUTPUT);
    digitalWrite(RELAY_ENABLE_PIN, LOW); 
    DEBUG_PRINT("Com error detected, keep relay off!");
    return;
  }
  /* print build time  */
  DEBUG_PRINT_LN();
  DEBUG_PRINT("Build:");
  DEBUG_PRINT_LN(PROJECT_BUILD_TIMESTAMP);
  /* print SW version  */
  
  DEBUG_PRINT("Version:");
  DEBUG_PRINT((uint8_t)PROJECT_MAJOR);
  DEBUG_PRINT(".");
  DEBUG_PRINT((uint8_t)PROJECT_UNO);
  DEBUG_PRINT(".");
  DEBUG_PRINT((uint8_t)PROJECT_ESP);
  DEBUG_PRINT_LN();  

  version_data_t local_version_u32_form;
  /* get the version info stored in com */
  local_version_u32_form.all = comTransmitData.get_version_all();

  /* Check uno version in hex array versus the one received */
  if (local_version_u32_form.version_byte[1] != 0)
  {
    // different than init, check version
    if ((uint8_t)PROJECT_UNO != local_version_u32_form.version_byte[1])
    {
      DEBUG_PRINT_LN("Version change detected!");
      // request flashing of atmega328
      flash_uno_req();
    }
    else
    {
      DEBUG_PRINT("Uno is already at version: ");
      DEBUG_PRINT_LN(local_version_u32_form.version_byte[1]);

      // enable relay if comuncaition ok, and uno is flashed
      pinMode(RELAY_ENABLE_PIN, OUTPUT);
      digitalWrite(RELAY_ENABLE_PIN, HIGH); 
    }
    uno_version_checked = true;
  }
  else
  {
    DEBUG_PRINT_LN("Waiting for uno version!");
  }
}

// *********************************************************************
// SETUP
// *********************************************************************
void setup() {
  #if defined(DEBUG_SERIAL_HW)
  DEBUG_SERIAL.begin(115200);   // start serial
  DEBUG_SERIAL.flush();
  #endif

  // relay enable pin
  pinMode(RELAY_ENABLE_PIN, OUTPUT);
  digitalWrite(RELAY_ENABLE_PIN, LOW); // start with relay disabled

  //reset pin for uno
  pinMode(UNO_RESET_PIN, OUTPUT);
  digitalWrite(UNO_RESET_PIN, HIGH); // when pulled low, the uno will reset
    
  // for signaling
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW); // turn on led 

  version_data_t local_version_u32_setup;
  /* Convert version to data for uno to send */
  local_version_u32_setup.version_byte[0] = (uint8_t)PROJECT_MAJOR; // major
  local_version_u32_setup.version_byte[1] = 0;                      // init  // read only the uno version
  local_version_u32_setup.version_byte[2] = (uint8_t)PROJECT_ESP;   // esp
  local_version_u32_setup.version_byte[3] = FLASH_INFO_NOT_AVAIL;   // init
  // save to com, but dont send yet
  (void)comTransmitData.set_version_all((uint32_t)local_version_u32_setup.all, false);

  // connect to the network and setup web page
  setup_webserver();
  // initialize over the air update feature after connected
  setup_ota();
  // check rtc flag for flasg request
  checkRTCData();

  // connect to blynk after connected
  setup_blynk(); 

  /* setup communication */
  setup_sw_uart();

  /* setup mqtt */
  setup_mqtt();

  /* turn off startup led */
  digitalWrite(LEDPIN, HIGH); // turn off led  
}

// *********************************************************************
// LOOP
// *********************************************************************
void loop() {
  loop_mqtt();
  
  // scheduler
  scheduler(); //call scheduler

  //always
  loop_sw_uart();
}