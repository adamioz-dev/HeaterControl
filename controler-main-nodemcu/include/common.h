#ifndef COMMON_H
#define COMMON_H

// *********************************************************************
// INCLUDES
// *********************************************************************
#include <Arduino.h>

// *********************************************************************
// DEFINES
// *********************************************************************
//int to float value conversion scaling 195 -> 19.5
#define FLOAT_SCALING 10

#define LOW_BAT_WARN 2.3
// *********************************************************************
// DATATYPES
// *********************************************************************

//ids of mesages
enum com_data_ids { 
  //relevant Settings
  _ID_HEATER_MODE = 0,
  _ID_TIMER_ON_TIME,
  _ID_TIMER_OFF_TIME,
  _ID_AMBIENT_TARGET_TEMP,
  _ID_AMBIENT_HYSTERESIS,
  _ID_PUFFER_MIN_TEMP,
  _ID_PUFFER_MAX_TEMP,
  _ID_PUFFER_HYSTERESIS,
  // measured values
  _ID_HEATER_TRIGGER_REQUEST,
  _ID_TIMER_REMAINING_SEC,
  _ID_AMBIENT_ACTUAL_ROOM_TEMP,  
  _ID_AMBIENT_ACTUAL_ROOM_TEMP_VALID,
  _ID_THERMOSTAT_STATE, 
  _ID_PUFFER_TEMP,
  _ID_HEATER_TEMP,
  _ID_FEEDBACK_CODE,
  _ID_IP_ADDRESS,
  _ID_UPTIME_UNO,
    // request data by MCU
  _ID_DATA_GROUP1,
  _ID_DATA_GROUP2,
    // version 
  _ID_VERSION,
  // last entry
  _ID_LAST,
};// max 255 (one byte)

// feedback codes
enum feedback_codes_enum {  
  PUFFER_TEMP_LOW = 0,
  HEATING_OFF,
  HEATING_ON,   
  PUFFER_TEMP_OVERHEAT,
  UNO_COM_ERR = 0xFF,
};

enum timerRequestModes {  // used in request_heating_timer_change()
  HEATER_OFF = 0,   // requested timer to stay off
  HEATER_ON  = 1,   // requested timer on
  // special case to toggle the value between on and off
  TOOGLE_MODE = 2
};

enum heating_modes
{
 HEATING_MODE_INIT = 0,
 TIMER_MODE,            
 TEMPERATURE_MODE, 
 THERMOSTAT_MODE,     
 HEATING_MODE_LAST,
};

// function return codes
enum returnCodes
{
  RET_OK          = 0,  // all ok
  RET_FAIL,             // general error
  RET_MIN_NOK,          // below lower limits
  RET_MAX_NOK,          // above higher limits
};

class ComTransmitData {
  private:
    typedef struct 
    {
      // measured time in minutes
      uint16_t on_time      = 0;
      uint16_t off_time     = 0;
      // runtime measurements
      uint16_t timer_remaining_time_sec = 0;
    }timerDataCom_t;
    /***********************/
    // temperature measured settings
    typedef struct 
    {
      // measured temperature in celsius
      uint16_t ambient_target_temp  = 0;  // Scaled  FLOAT_SCALING
      uint8_t hysteresis            = 0;    // Scaled  FLOAT_SCALING
      // runtime measurements
      uint16_t termostat_ambient_temp   = 0;
      uint8_t termostat_ambient_temp_bat = 3.0 * FLOAT_SCALING; // default value after reset
      bool termostat_ambient_temp_valid = false;
    }termostatDataCom_t;
    // external termostat settings
    typedef struct 
    {
      // the state of request of ext termostat
      bool external_termostat_state = false;
    }ext_termostatDataCom_t;
    /***********************/
    // heating relevant data
    typedef struct 
    {
      // heating request state
      uint8_t heating_trigger_request     = 0;  // general heating request not just timer
      // heating mode:
      //    1 - timer based
      //    2 - measured temp based, 
      //    3 - ext thermostat based, 
      uint8_t  mode   = 0;  
      timerDataCom_t       timer;            // used in heating mode 1 (no measurement in room required)
      termostatDataCom_t   termostat;        // used in heating mode 2 (measurement in room required)
      ext_termostatDataCom_t   ext_termostat;        // used in heating mode 3 (external termostat is attached)
    }heatingDataCom_t;
    /***********************/
    // puffer relevat data
    typedef struct 
    {
      uint8_t min_temp     = 0;    // min allowed puffer temp for heating
      uint8_t max_temp     = 0;    // max allowed puffer temp (overheat protection)
      uint8_t hysteresis   = 0;    // puffer hysteresis  // Scaled  FLOAT_SCALING
      // runtime measurements
      uint16_t puffer_temperature       = 0;
    }pufferDataCom_t;
    /***********************/
    
    // boiler controll relevat data
    typedef struct 
    {
      // runtime measured actual draw
      uint16_t boiler_temperature       = 0;
    }fireDataCom_t;
    /***********************/
    // main settings structure
    typedef struct 
    {
      heatingDataCom_t heater;
      pufferDataCom_t  puffer;
      fireDataCom_t    boiler;
      // runtime actual feedback code
      uint8_t feedbackCode    = UNO_COM_ERR;
      uint32_t IP_address    = 0;
      uint32_t unoUptime    = 0;
      uint32_t version_all  = 0;
    }ComData_t;

    // transmitted settings of controler
    ComData_t     comData;                 //used settings during operation

  public:
    // methods
    // Constructor 
    ComTransmitData();

    //specific
    int getHeating_mode();  // getter heating mode
    // heating settings methods
    bool setHeating_mode(int req_mode, bool send=false);   // setter heating mode
    //timer
    int getHeatingTimer_ontime();
    bool setHeatingTimer_ontime(int req_ontime, bool send=false);
    int getHeatingTimer_offtime();
    bool setHeatingTimer_offtime(int req_offtime, bool send=false);
    //meas
    int getHeating_TriggerRequest();
    bool setHeating_TriggerRequest(int req, bool send=false);
    int getHeatingTimer_TimeRemaining_sec();
    bool setHeatingTimer_TimeRemaining_sec(int time); // used to receive the data

    // termostat
    float getHeatingTermostat_target_ambient();
    bool setHeatingTermostat_target_ambient(int req_ambient, bool send=false);
    float getHeatingTermostat_hysteresis();
    bool setHeatingTermostat_hysteresis(int req_hysteresis, bool send=false);
    //meas
    float getHeatingTermostat_AmbientTemp();
    bool setHeatingTermostat_AmbientTemp(int temp, bool send=true);   // send only
    float getHeatingTermostat_AmbientTempBattery();
    bool setHeatingTermostat_AmbientTempBattery(int bat); 
    bool getHeatingTermostat_AmbientTemp_Valid();
    bool setHeatingTermostat_AmbientTemp_Valid(int state, bool send=false); 
    // external thermostat
    bool getHeatingExternalTermostat_State();
    bool setHeatingExternalTermostat_State(int state);

    //puffer
    int getPuffer_minTemp();
    bool setPuffer_minTemp(int req_temp, bool send=false);
    int getPuffer_maxTemp();
    bool setPuffer_maxTemp(int req_temp, bool send=false);
    float getPuffer_hysteresis();
    bool setPuffer_hysteresis(int req_hysteresis, bool send=false); 
    // meas
    float getPuffer_Temp();
    bool setPuffer_Temp(int temp); // used to receive the data, received unscaled 451 representing 45.1

    // meas
    float getBoiler_Temp();
    bool setBoiler_Temp(int temp); // used to receive the data
    //general
    int get_Feedback();
    bool set_Feedback(int code); // used to receive the data
    uint32_t get_unoUptime();
    bool set_unoUptime(uint32_t uptime); // used to receive the data
    uint32_t get_IPAddress();
    bool set_IPAddress(uint32_t ip_u32, bool send=true); // send only
    uint32_t get_version_all();
    bool set_version_all(uint32_t vers_u32, bool send=true); // send only
    bool send_version_all(); //just send on com
};
/***********************/

// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************

// *********************************************************************
// FUNCTIONS
// *********************************************************************
// function prototypes
boolean isValidNumber(String tString);
void hw_wdt_disable();
void hw_wdt_enable();
#endif //COMMON_H