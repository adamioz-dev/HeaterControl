// *********************************************************************
// INCLUDES
// *********************************************************************
#include "LCDHandler.h" 
#include "UARTHandler.h"
#include "ControlVersion.h"
#include "MeasurementHandler.h"
#include "CommonData.h"

// *********************************************************************
// DEFINES
// *********************************************************************

// scheduler interval
#define FAST_TASK_PERIOD    100u  
#define MID_TASK_PERIOD     1000u  // 1 second
#define SLOW_TASK_PERIOD    10000u // 10 second

// timeout for losing communication with esp
#define communication_esp_timeout    60000 // 1 min

// timeout for switching to timer mode in room temp is timeout
#define roomTemp_update_time_timeout    1200000 // 20 min 

// temperature hysteresis for puffer
#define temperatureHysteresis 	2 	// puffer hysteresis (+/- x C) 


// *********************************************************************
// DATATYPES
// *********************************************************************

// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************

// actual room temp
uint16_t roomTemperature = SNA; // FLOAT_SCALING  // start with big value so heating does not start after startup
bool roomTemperature_data_valid = true;
unsigned long roomTemperature_update_time = 0;  // time since data was received

//timer mode switch flag
bool timerModeON_selected = true;
//timer heating period
int16_t remainingTimeInMode_sec = 0;  // variable to store calculated remaining time in current mode
int16_t remainingTimeInMode_min = 0;  // variable to store calculated remaining time minutes

uint16_t heatingTimePassed = 0u; //init
uint16_t restoredTimerValue = 0u;

// requested pump state
bool pumpState_request;
// the requested state of the pump from the timer
bool timer_pumpState_req;

// Scheduler variables
// intervals for scheduler
unsigned long g_fast_task_interval_cnt =    0; 
unsigned long g_mid_task_interval_cnt =     0; 
unsigned long g_slow_task_interval_cnt =    0; 

// external termostat pin state
bool thermostat_pin_state = LOW;

// feedback code for the pump state
int feedbackCode = PUFFER_TEMP_LOW;

// *********************************************************************
// DECLARATIONS
// *********************************************************************
void request_heating_state_change(int input_value);
void toogle_led();
int calculateRequiredTime();
void heatingStateHandler();
void set_room_temperature(uint16_t input_value);
void temperatureInputChecker();
bool heatingTemperatureStateHandler();
bool heatingThermostatStateHandler();
bool heatingTimerStateHandler();
void heatingPumpHandler();
void communicationTimoCheck();
void receiveValue(uint8_t id, uint32_t value);
void restoreRemainingTime();
void sendMsg_group1();
void sendMsg_group2();
int getFeedbackCode(void);

void fast_task();
void middle_task();
void slow_task();
void scheduler();

void setup ();
void loop ();
// *********************************************************************
// FUNCTIONS
// *********************************************************************
void request_heating_state_change(int input_value) {
  bool local_heater_state =     (bool)GET_SETTING_HEATER_STATE();
  uint8_t local_heater_mode = (uint8_t)GET_SETTING_HEATER_MODE();
  // if toggle of value requested
  if (input_value == TOOGLE_MODE) 
  {
    local_heater_state = !local_heater_state;
    SET_SETTING_HEATER_STATE((bool)local_heater_state);
  } 
  // if accepted values
  else if ((input_value == HEATER_OFF) || (input_value == HEATER_ON))
  {
    SET_SETTING_HEATER_STATE((bool)input_value);
  }

  if ((HEATER_ON == local_heater_state) && ((local_heater_mode == TIMER_MODE) || (local_heater_mode == TEMPERATURE_MODE)))
  {
    timerModeON_selected = true;
  }

  sendPacket(_ID_HEATER_TRIGGER_REQUEST, (uint8_t)local_heater_state); 
}

void toogle_led(){
  static bool led_status = false;
  // blink led when alive 
  digitalWrite(LEDPIN, led_status); // turn on led 
  led_status = !led_status;
}

// calculate and update the required on or off time
int calculateRequiredTime(){
  int local_heater_timer_ontime = GET_SETTING_TIME_ON();
  int local_heater_timer_offtime = GET_SETTING_TIME_OFF();
  if (timer_pumpState_req == HIGH)
  {  
    //Calculate next "on time"
    return (local_heater_timer_ontime * 60);
  }
  else
  { 
    //Calculate next "Off time"
    return (local_heater_timer_offtime * 60);
  }
}

void heatingStateHandler(){
  uint8_t local_heater_mode = (uint8_t)GET_SETTING_HEATER_MODE();
  switch (local_heater_mode)
  {
    case TIMER_MODE:
    {
      // call timer handler 
      pumpState_request = heatingTimerStateHandler();
      break;
    }
    case TEMPERATURE_MODE:
    {
      // check the input temoerature values
      temperatureInputChecker();
      // call temperature based handler 
      pumpState_request = heatingTemperatureStateHandler();
      break;
    }
    case THERMOSTAT_MODE:
    {
      // call external termostat handler 
      pumpState_request = heatingThermostatStateHandler();
      break;
    }
    default:
      // unknown state use timer
      SET_SETTING_HEATER_MODE(TIMER_MODE);
      break;
  }
}

// Room temperature transmited in termostat mode on web
void set_room_temperature(uint16_t input_value) {
  //limit inputs
  if (input_value >400){
    roomTemperature = 400;
  }else if (input_value < 10){
    roomTemperature = 10;
  }else{
    // get temperature in room
    roomTemperature = input_value;
  }
  // mark new room temperature received
  roomTemperature_update_time = millis();
  // mark receiving of data from termostat
  roomTemperature_data_valid = true;
}

void temperatureInputChecker(){
  if (roomTemperature_data_valid){
    unsigned long currentMillis = millis();
    if (roomTemp_update_time_timeout <= (currentMillis - roomTemperature_update_time))
    {
      // lost communication with the termostat, switch temporarely to timer
      // clear flag
      roomTemperature_data_valid = false;
      roomTemperature_update_time = 0;
      // invalidate the stored measured room temp
      roomTemperature = SNA;

      SET_SETTING_HEATER_MODE(TIMER_MODE);
    }
  }
  else // not valid termosat data 
  {
    //data not valid anymore
    roomTemperature_update_time = 0;
    // invalidate the stored measured room temp
    roomTemperature = SNA;
  }
}

// temperature controlled heating handler
bool heatingTemperatureStateHandler()
{
  static bool temperature_pumpState_req =               LOW;
  uint16_t local_termostat_ambiet_target_temperature =  (uint16_t)GET_SETTING_TARGET_TEMP();
  uint16_t local_termostat_hysteresis =                 (uint16_t)GET_SETTING_TARGET_TEMP_HYST(); 
  int local_heaterStateRequest =                        GET_SETTING_HEATER_STATE();
  /* timer baset limitation */
  static bool local_timer_request =                     false;
  int termocontrol_heatTimerSwitchPeriod =              0; //init
  static bool local_hold_timer_flag =                   false;

  /* Update timer if not on hold */
  if (local_hold_timer_flag == false)
  {
    /** Get the timer value */
    local_timer_request = heatingTimerStateHandler();
  }

  // check if new data was received from the room in the timeout interval
  if (roomTemperature_data_valid){
    // Timeout not reached, continue using termostat
    // check heating request state
    if (HEATER_ON == local_heaterStateRequest){
      // if heatiung is requested and room temp is not known
      if (roomTemperature != (uint16_t)SNA)
      {      
        //get the timer limit for current state
        termocontrol_heatTimerSwitchPeriod = calculateRequiredTime();
        /* reached setpoint (target temperature reached) */
        if (roomTemperature >= (local_termostat_ambiet_target_temperature + local_termostat_hysteresis))
        {
          /* timer is still requesting ON */
          if (local_timer_request == HIGH)
          {
            /* Force timer off state */
            /* Force the timer counter over the limit to trigger going to OFF */
            heatingTimePassed = termocontrol_heatTimerSwitchPeriod;
          }
          /* timer was requesting off and finished the off waiting time (is at the last second)*/
          else if ((local_timer_request == LOW) && ((int)heatingTimePassed >= termocontrol_heatTimerSwitchPeriod - 1))
          {
            /* freeze timer at the end, ambient temp is still above setpoint */
            local_hold_timer_flag = true;
          }
          /* set/keep the pump OFF */
          temperature_pumpState_req = LOW; //stop pump request
        }
        // below the setpoint + hysteresis 
        else if (roomTemperature <= (local_termostat_ambiet_target_temperature - local_termostat_hysteresis))
        {
          // allow start only when temperature lowered, and then take the actual timer state request
          temperature_pumpState_req = local_timer_request;
          /* Clear flag to resume timer handling */
          local_hold_timer_flag = false;
        }
        else
        {
          /* still between hysteresis values, freeze counter at the end to allow the temperature to lower below target - hysteresis */
          if ((local_timer_request == LOW) && ((int)heatingTimePassed >= termocontrol_heatTimerSwitchPeriod - 1))
          {
            /* freeze timer at the end, ambient temp is still above setpoint */
            local_hold_timer_flag = true;
          }
          // take the last timer controlled state
          temperature_pumpState_req = local_timer_request;
        }
      }
      else
      {
        // room temp not known but heating is requested
        // provide heating until room temperature will be knwon or fallback to timer happens
        // take the actual timer state request
        temperature_pumpState_req = local_timer_request;
      }
    }
    else
    {
      // heating is NOT requested
      temperature_pumpState_req = LOW; //stop pump request
    }

  }else if (roomTemperature_data_valid == false){
    // no data received, switch to timer mode
    //switch to timer mode in whatever state it was before
    if(temperature_pumpState_req == HIGH){
      // if heating was on when connection was lost enable timer
      request_heating_state_change(HEATER_ON);
    }
    // temperature not valid, switch to timer
    SET_SETTING_HEATER_MODE(TIMER_MODE);
  }
  return temperature_pumpState_req;
}

// external termostat controlled heating handler
bool heatingThermostatStateHandler()
{
  static bool thermostat_pumpState_req = LOW;
  int local_heaterStateRequest =         GET_SETTING_HEATER_STATE();

  boolean local_termostat_state =  digitalRead(thermostatPin);

  // check heating request state
  if (HEATER_ON == local_heaterStateRequest){
    // if heatiung is requested and room temp is not known
    if (local_termostat_state == HIGH)
    {
      // external thermostat requested heating
      thermostat_pumpState_req = HIGH; // start pump request
    }
    else
    {
      // external thermostat does not request heating
      thermostat_pumpState_req = LOW; // stop heating 
    }
  }
  else
  {
    // heating is NOT requested
    thermostat_pumpState_req = LOW; //stop pump request
  }
  // store pin state
  SET_STATE_EXT_THERMOSTAT(local_termostat_state);

  return thermostat_pumpState_req;
}

/* Function used to get the remaining time from the esp in timer mode in case of uno reset  */
void restoreRemainingTime()
{
  int heatTimerSwitchPeriod = 0; //init
  heatTimerSwitchPeriod = calculateRequiredTime();
  /* Calculate the time passed */
  heatingTimePassed = heatTimerSwitchPeriod - restoredTimerValue;
  // restored, clear the data received to restore
  restoredTimerValue = 0u; 
}

//timer handler
bool heatingTimerStateHandler()
{
  int heatTimerSwitchPeriod =    0; //init
  int local_feedback_code =     feedbackCode;
  int local_heater_timer_offtime = GET_SETTING_TIME_OFF();
  int local_heaterStateRequest = GET_SETTING_HEATER_STATE();

  // Check if heating is requested
  if (local_heaterStateRequest == HEATER_ON) 
  {   
    if(local_heater_timer_offtime == 0) //off time set to 0, Continuosly on mode
    { 
      timer_pumpState_req = HIGH;
    }
    else if (timerModeON_selected)
    {
      timerModeON_selected = false; //clear flag
      timer_pumpState_req = HIGH; //requets start pump from timer

     /* There is a value to restore */
     if (restoredTimerValue != 0u)
      {
        /* restore after reset */
        restoreRemainingTime();
      }
      else
      {
        // normal operation, start with count 0
        heatingTimePassed = 0;
      }
      // calculate how much time should it stay in the starting mode (on mode)
      //heatTimerSwitchPeriod is continously calculated according to the current state of trigger
    }
    else
    {
      //continously calculate what should be the to wait in this cycle in case it changes from the user
      heatTimerSwitchPeriod = calculateRequiredTime();

      // if requesting heat, and actually heating, then count
      if (timer_pumpState_req == HIGH){
        //heating is allowed, (ex: no low temp puffer blocking)
        if(local_feedback_code == HEATING_ON){
          // incremet passed time in seconds
          heatingTimePassed += 1; //increment each loop (second) 
        }
        else{
          // heating is requested but refused (the mode is not hearing on)
          // wait until can 
        }
      }
      // in off time, no heating is requested
      else if (timer_pumpState_req == LOW){
        // allow count down, in all cases, puffer is not used here
        heatingTimePassed += 1; //increment each loop (second) 
      }

      //interval time passed, switch state
      if ((int)heatingTimePassed >= heatTimerSwitchPeriod)
      { 
        // clear counter
        heatingTimePassed = 0;
        remainingTimeInMode_sec = 0; // clear remaining time
        remainingTimeInMode_min = 0;

        //toggle pump state according to configured time interval
        timer_pumpState_req = !timer_pumpState_req;
        //calculate how much should wait in the next cycle acording to what state we are, heat ON or OFF 
      }
      else
      {
        // currently counting
        //Calculate remaining time in current mode
        remainingTimeInMode_sec = (int)((heatTimerSwitchPeriod - heatingTimePassed));
        remainingTimeInMode_min = int(remainingTimeInMode_sec / 60);
        if (heatingTimePassed != 0)
        {
          /* round up */
          remainingTimeInMode_sec = remainingTimeInMode_sec + 1;
        }
      }
    }   
  }
  else if (local_heaterStateRequest == HEATER_OFF) 
  {
    // timer not triggered, no heating requested
    timer_pumpState_req = LOW;
    remainingTimeInMode_sec = 0; // clear remaining time
    remainingTimeInMode_min = 0;
  }
  // set the actual pump request to the timer request state
  return timer_pumpState_req;
}

// PUMP
// *********************************************
//Output signal handler for pump 
void heatingPumpHandler()
{
  static bool pumpState = LOW;  // the current state of the output pin
  uint16_t local_puffer_min_temp = ((uint16_t)GET_SETTING_PUFF_MIN() * FLOAT_SCALING);
  uint16_t local_puffer_max_temp = ((uint16_t)GET_SETTING_PUFF_MAX() * FLOAT_SCALING);
  uint16_t local_puffer_hysteresis = temperatureHysteresis * FLOAT_SCALING;
  uint16_t local_puffer_temperature = puffer_temp;

  //uper limit, overheat danger, open heating to cool down the puffer
  if (local_puffer_temperature >= local_puffer_max_temp) 
  {
    pumpState = HIGH; //start pump
    feedbackCode = PUFFER_TEMP_OVERHEAT; //overheat temp
  }
  // allowed puffer temperature
  else if (local_puffer_temperature >= (local_puffer_min_temp + local_puffer_hysteresis)) 
  {
    //check the requested pump state
    if (pumpState_request == HIGH) 
    {
      pumpState = HIGH; //start pump
      feedbackCode = HEATING_ON; //heating
    } 
    else 
    {
      pumpState = LOW; //Stop pump
      feedbackCode = HEATING_OFF; //heating off
    }
  }
  //lower limit, turn off
  else if (local_puffer_temperature <= (local_puffer_min_temp - local_puffer_hysteresis)) 
  {
    pumpState = LOW; //stop pump
    feedbackCode = PUFFER_TEMP_LOW; //low temp
  }
  else 
  { //between hysteresis limits
    // requesting on, but just above the min limit
    if ((pumpState_request == HIGH) && (pumpState == LOW))
    {
      // report low temperature
      feedbackCode = PUFFER_TEMP_LOW; //low temp
    }
    else if (pumpState_request == LOW)
    {
      //turn off requsted
      pumpState = LOW; //Stop pump
    }
  }

  //set the corret pump state output pin
  digitalWrite(pumpPin, pumpState);
}

// COMMUNICATION
// *********************************************
void communicationTimoCheck(){
  unsigned long currentMillis = millis();
  if (communication_esp_timeout <= (currentMillis - last_communication_time))
  {
    // no data received
    IP_address.u32 = NOT_CONNECTED_STATE;
    last_communication_time = 0;
  }
}

void receiveValue(uint8_t id, uint32_t value){
  // set the value of the received variable
  switch (id)
  {
    case _ID_HEATER_MODE:
    {
      SET_SETTING_HEATER_MODE((int)value);
      //(void)settingsData.setHeating_mode((uint8_t)value);
      //if ((value == TIMER_MODE) && (termostatConnectionLost_FallbackMode_Handler(CHECK_FALLBACK))){
        // timer was selected, clear the fallback
      //  (void)termostatConnectionLost_FallbackMode_Handler(CLEAR_FALLBACK);
      //}
      break;
    }
    case _ID_TIMER_ON_TIME:
    {
      SET_SETTING_TIME_ON((int)value);
      //(void)settingsData.setHeatingTimer_ontime((uint16_t)value);
      break;
    }
    case _ID_TIMER_OFF_TIME:
    {
      SET_SETTING_TIME_OFF((int)value);
      //(void)settingsData.setHeatingTimer_offtime((uint16_t)value);
      break;
    }
    case _ID_AMBIENT_TARGET_TEMP:
    {
      //(void)settingsData.setHeatingTermostat_target_ambient((uint16_t)value);
      SET_SETTING_TARGET_TEMP((int)value);
      break;
    }
    case _ID_AMBIENT_HYSTERESIS:
    {
      SET_SETTING_TARGET_TEMP_HYST((int)value);
      //(void)settingsData.setHeatingTermostat_hysteresis((uint8_t)value);
      break;
    }
    case _ID_PUFFER_MIN_TEMP:
    {
      SET_SETTING_PUFF_MIN((int)value);
      //(void)settingsData.setPuffer_minTemp((uint8_t)value);
      break;
    }
    case _ID_PUFFER_MAX_TEMP:
    {
      SET_SETTING_PUFF_MAX((int)value);
      //(void)settingsData.setPuffer_maxTemp((uint8_t)value);
      break;
    }
    case _ID_PUFFER_HYSTERESIS:
    {
      //(void)settingsData.setPuffer_hysteresis((uint8_t)value);
      break;
    }
    /// runtime values
    case _ID_HEATER_TRIGGER_REQUEST:
    {
      request_heating_state_change(value);
      //request_heating_state_change(value);
      break;
    }
    case _ID_TIMER_REMAINING_SEC:
    {
      // Only sent by the UNO, get in case of reset only
      restoredTimerValue = (uint16_t)value;
      break;
    }
    case _ID_AMBIENT_ACTUAL_ROOM_TEMP:  
    {
      // Receive Only
      set_room_temperature((uint16_t)value);
      //set_room_temperature(value);
      break;
    }
    case _ID_AMBIENT_ACTUAL_ROOM_TEMP_VALID:
    {
      // Validity of the termostat data from esp
      roomTemperature_data_valid = (bool)value;
      break;
    }
    case _ID_THERMOSTAT_STATE:
    {
      // Only sent by the UNO
      break;
    }
    case _ID_PUFFER_TEMP:
    {
      // Only sent by the UNO
      break;
    }
    case _ID_HEATER_TEMP:
    {
      // Only sent by the UNO
      break;
    }
    case _ID_FEEDBACK_CODE:
    {
      // Only sent by the UNO
      break;
    }
    case _ID_IP_ADDRESS:
    {
      // Receive Only
      IP_address.u32 = value;
      break;
    }
    case _ID_UPTIME_UNO:
    {
      // send only
      break;
    }
    case _ID_DATA_GROUP1:
    {
      // Receive Only
      if ((uint8_t)value == 1)
      {
        sendMsg_group1(); // send back requested data group 
      }
      break;
    }
    case _ID_DATA_GROUP2:
    {
      // Receive Only
      if ((uint8_t)value == 1)
      {
        sendMsg_group2(); // send back requested data group 
      }
      break;
    }
    case _ID_VERSION:
    {
      // Receive esp related data Only, send uno version
      version_data_t version_data_temp; // temp variable 
      // get to temp variable
      version_data_temp.all = (uint32_t)value;
      // get relevant data only
      version_data.version_byte[0] = version_data_temp.version_byte[0]; // major
      //don't overtake                                                  // Byte 1 is uno version
      version_data.version_byte[2] = version_data_temp.version_byte[2]; // esp vers
      version_data.version_byte[3] = version_data_temp.version_byte[3]; // flash success flag 
      break;
    }
    default:
    {
      break;
    }
  }
  // new data received, update counter
  last_communication_time = millis();
}

int getFeedbackCode(void)
{
  // return the feedback code
  return feedbackCode;
}

// send response messages, group 1
void sendMsg_group1()
{    
  // send feeback info (STATUS)
  sendPacket(_ID_FEEDBACK_CODE, (uint8_t)feedbackCode); 
  // send the remaining time
  sendPacket(_ID_TIMER_REMAINING_SEC, (uint16_t)remainingTimeInMode_sec); 
  // increment uptime counter and send current uptime on uno // send it anyway because the esp will reset the uno if present and not receiving this
  sendPacket(_ID_UPTIME_UNO, (uint32_t)millis() / 1000); 
}

// send recurent messages 10 sec
void sendMsg_group2()
{
  uint8_t local_heater_mode = (uint8_t)GET_SETTING_HEATER_MODE();


  // sync known data 
  if ((roomTemperature_data_valid == true) && (roomTemperature != (uint16_t)SNA)){
    sendPacket(_ID_AMBIENT_ACTUAL_ROOM_TEMP, (uint16_t)roomTemperature);
  }
    
  // heater request
  sendPacket(_ID_HEATER_TRIGGER_REQUEST, (uint8_t)GET_SETTING_HEATER_STATE());
  // send the puffer temp
  sendPacket(_ID_PUFFER_TEMP, (uint32_t)puffer_temp);   
  // send the heater temp
  sendPacket(_ID_HEATER_TEMP, (uint32_t)heater_temp); 

  // Settings period sync
  sendPacket(_ID_HEATER_MODE, (uint8_t)local_heater_mode); 
  //timer
  sendPacket(_ID_TIMER_ON_TIME, (uint16_t)GET_SETTING_TIME_ON());
  sendPacket(_ID_TIMER_OFF_TIME, (uint16_t)GET_SETTING_TIME_OFF());

    // termometer based control
  sendPacket(_ID_AMBIENT_TARGET_TEMP, (uint16_t)GET_SETTING_TARGET_TEMP());
  sendPacket(_ID_AMBIENT_HYSTERESIS, (uint8_t)GET_SETTING_TARGET_TEMP_HYST());
  // send the confirmation that termostat data is received
  sendPacket(_ID_AMBIENT_ACTUAL_ROOM_TEMP_VALID, (uint8_t)roomTemperature_data_valid);

  if (local_heater_mode == THERMOSTAT_MODE)
  {
    // external thermostate control
    // send the pin state from extenal thermostat
    sendPacket(_ID_THERMOSTAT_STATE, (uint8_t)GET_STATE_EXT_THERMOSTAT());
  }
  // puffer
  sendPacket(_ID_PUFFER_MIN_TEMP,(uint8_t)GET_SETTING_PUFF_MIN());
  sendPacket(_ID_PUFFER_MAX_TEMP,(uint8_t)GET_SETTING_PUFF_MAX());

  // version data, uno version is relevant
  sendPacket(_ID_VERSION, (uint32_t)version_data.all);
}

// *************************************************
//                Initialization                        
// *************************************************
void setup ()
{
  // setup the communication
  setup_sw_uart();
  // for signaling
  pinMode(LEDPIN, OUTPUT);
  //pump output
  pinMode(pumpPin, OUTPUT);
  //thermostat pin init
  pinMode(thermostatPin, INPUT);
  
  // puffer and fire temp sensor init
  sensorAverageInit();
  // lcd setup
  setup_common(); //comon setup

}//end of setup

// *************************************************
//                  Tasks                        
// *************************************************
void fast_task()
{
    //measure puffer temperature
  getPufferTemperature();  

  // measure heater temperature
  getHeaterTemperature();
}

void middle_task()
{
  // timer handler
  heatingStateHandler();

  // handle pump state and output pin value
  heatingPumpHandler();

  // toogle the alive counter
  toogle_led();
}

void slow_task()
{
  // Connection check
  communicationTimoCheck();
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
}

// *************************************************
//                  Main loop                    
// *************************************************
void loop ()
{
  //common tasks
  loop_common();

  // scheduler
  scheduler(); //call scheduler

  // uart packet loop
  loop_sw_uart();
}//end loop()
