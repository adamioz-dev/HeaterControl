// -------- includes --------
#include "Common.h"

// *********************************************************************
// EXTERNAL FUNCTIONS
// *********************************************************************
extern void sendPacket(uint8_t id, uint8_t value);
extern void sendPacket(uint8_t id, uint16_t value);
extern void sendPacket(uint8_t id, uint32_t value);

// *********************************************************************
// FUNCTIONS
// *********************************************************************
void hw_wdt_disable(){
  *((volatile uint32_t*) 0x60000900) &= ~(1); // Hardware WDT OFF
}

void hw_wdt_enable(){
  *((volatile uint32_t*) 0x60000900) |= 1; // Hardware WDT ON
}
// *********************************************************************
// CLASSES
// *********************************************************************
// COMMON METHODS
//***********************
//constructor
ComTransmitData::ComTransmitData(){
}

// HEATING
//***********************
int ComTransmitData::getHeating_mode(){
  return comData.heater.mode;
}
// heater mode setter
bool ComTransmitData::setHeating_mode(int req_mode, bool send){
  //in limits
  if ((req_mode > HEATING_MODE_INIT) && (req_mode < HEATING_MODE_LAST)){
    comData.heater.mode = req_mode;
    if (send){
      sendPacket(_ID_HEATER_MODE, (uint8_t)req_mode); // send
    }
  // not known mode
  }else{
    return RET_FAIL;
  }
  return RET_OK;
}

int ComTransmitData::getHeating_TriggerRequest(){
  return comData.heater.heating_trigger_request;
}
bool ComTransmitData::setHeating_TriggerRequest(int req, bool send){
  comData.heater.heating_trigger_request = req;
  if (send){
    sendPacket(_ID_HEATER_TRIGGER_REQUEST, (uint8_t)req); // send
  }
  return RET_OK;
}


// timer 
//***********************
int ComTransmitData::getHeatingTimer_ontime(){
  return comData.heater.timer.on_time;
}
bool ComTransmitData::setHeatingTimer_ontime(int req_ontime, bool send){
  comData.heater.timer.on_time = req_ontime;
  if (send){
    sendPacket(_ID_TIMER_ON_TIME, (uint16_t)req_ontime); // send
  }
  return RET_OK;
}
//  get and set functions timer off time
int ComTransmitData::getHeatingTimer_offtime(){
  return comData.heater.timer.off_time;
}
bool ComTransmitData::setHeatingTimer_offtime(int req_offtime, bool send){
  comData.heater.timer.off_time = req_offtime;
  if (send){
    sendPacket(_ID_TIMER_OFF_TIME, (uint16_t)req_offtime); // send
  }
  return RET_OK;
}

// meas
int ComTransmitData::getHeatingTimer_TimeRemaining_sec(){
  return comData.heater.timer.timer_remaining_time_sec;
}
bool ComTransmitData::setHeatingTimer_TimeRemaining_sec(int time){
  // only allowed when receive data
  comData.heater.timer.timer_remaining_time_sec = time;
  return RET_OK;
}


// termostat
//***********************
float ComTransmitData::getHeatingTermostat_target_ambient(){
  return ((float)comData.heater.termostat.ambient_target_temp / FLOAT_SCALING);
}
//  set functions termostat
bool ComTransmitData::setHeatingTermostat_target_ambient(int req_ambient, bool send){
  comData.heater.termostat.ambient_target_temp = req_ambient;
  if (send){
    sendPacket(_ID_AMBIENT_TARGET_TEMP, (uint16_t)req_ambient); // send
  }
  return RET_OK;
}

float ComTransmitData::getHeatingTermostat_hysteresis(){
  return ((float)comData.heater.termostat.hysteresis / FLOAT_SCALING);
}
//  set functions hysteresis
bool ComTransmitData::setHeatingTermostat_hysteresis(int req_hysteresis, bool send){ 
  comData.heater.termostat.hysteresis = req_hysteresis;
  if (send){
    sendPacket(_ID_AMBIENT_HYSTERESIS, (uint8_t)req_hysteresis); // send
  }
  return RET_OK;
}

// meas
float ComTransmitData::getHeatingTermostat_AmbientTemp(){
  return ((float)comData.heater.termostat.termostat_ambient_temp / FLOAT_SCALING);
}
bool ComTransmitData::setHeatingTermostat_AmbientTemp(int req, bool send){
  comData.heater.termostat.termostat_ambient_temp = (uint16_t)req;
  if (send){
    sendPacket(_ID_AMBIENT_ACTUAL_ROOM_TEMP, (uint16_t)req); // send
  }
  return RET_OK;
}
float ComTransmitData::getHeatingTermostat_AmbientTempBattery(){
  return ((float)comData.heater.termostat.termostat_ambient_temp_bat / FLOAT_SCALING);
}
bool ComTransmitData::setHeatingTermostat_AmbientTempBattery(int volt){
  comData.heater.termostat.termostat_ambient_temp_bat = (uint8_t)volt;
  return RET_OK;
}
bool ComTransmitData::getHeatingTermostat_AmbientTemp_Valid(){
  return comData.heater.termostat.termostat_ambient_temp_valid;
}
bool ComTransmitData::setHeatingTermostat_AmbientTemp_Valid(int state, bool send){
  // only allowed when receive data
  comData.heater.termostat.termostat_ambient_temp_valid = (bool)state;
  if (comData.heater.termostat.termostat_ambient_temp_valid == false){
    // invalidate the current room measurement
    comData.heater.termostat.termostat_ambient_temp = (uint16_t)0; // no temp, don't send
  }
  if (send){
    sendPacket(_ID_AMBIENT_ACTUAL_ROOM_TEMP_VALID, (uint8_t)state); // send
  }
  return RET_OK;
}

// extenal thermostat for external termostat mode
bool ComTransmitData::getHeatingExternalTermostat_State(){
  return comData.heater.ext_termostat.external_termostat_state;
}
bool ComTransmitData::setHeatingExternalTermostat_State(int state){
  // only allowed when receive data
  comData.heater.ext_termostat.external_termostat_state = (bool)state;
  return RET_OK;
}

// puffer 
//***********************
int ComTransmitData::getPuffer_minTemp(){
  return comData.puffer.min_temp;
}
bool ComTransmitData::setPuffer_minTemp(int req_temp, bool send){
  comData.puffer.min_temp = req_temp;
  if (send){
    sendPacket(_ID_PUFFER_MIN_TEMP, (uint8_t)req_temp); // send
  }
  return RET_OK;
}

int ComTransmitData::getPuffer_maxTemp(){
  return comData.puffer.max_temp;
}

bool ComTransmitData::setPuffer_maxTemp(int req_temp, bool send){
  comData.puffer.max_temp = req_temp;
  if (send){
    sendPacket(_ID_PUFFER_MAX_TEMP, (uint8_t)req_temp); // send
  }
  return RET_OK;
}
//get puffer hysteresis
float ComTransmitData::getPuffer_hysteresis(){
  return ((float)comData.puffer.hysteresis / FLOAT_SCALING);
}

//  set functions Puffer hysteresis
bool ComTransmitData::setPuffer_hysteresis(int req_hysteresis, bool send){
  comData.puffer.hysteresis = req_hysteresis;
  if (send){
    sendPacket(_ID_PUFFER_HYSTERESIS, (uint8_t)req_hysteresis); // send
  }
  return RET_OK;
}
//meas
float ComTransmitData::getPuffer_Temp(){
  return ((float)comData.puffer.puffer_temperature / FLOAT_SCALING);
}

//  set functions Puffer hysteresis
bool ComTransmitData::setPuffer_Temp(int temp){
  // only for receiving
  comData.puffer.puffer_temperature = temp;
  return RET_OK;
}


float ComTransmitData::getBoiler_Temp(){
  return ((float)comData.boiler.boiler_temperature / FLOAT_SCALING);
}
bool ComTransmitData::setBoiler_Temp(int temp)
{
  // received only
  comData.boiler.boiler_temperature = temp;
  return RET_OK;
}

uint32_t ComTransmitData::get_unoUptime(){
  return comData.unoUptime;
}
bool ComTransmitData::set_unoUptime(uint32_t uptime)
{
  // received only
  comData.unoUptime = uptime;
  return RET_OK;
}

int ComTransmitData::get_Feedback(){
  return comData.feedbackCode;
}
bool ComTransmitData::set_Feedback(int code)
{
  // received only
  comData.feedbackCode = code;
  return RET_OK;
}

uint32_t ComTransmitData::get_IPAddress(){
  return comData.IP_address;
}
bool ComTransmitData::set_IPAddress(uint32_t value, bool send)
{
  // received only
  comData.IP_address = (uint32_t)value;
  if (send){
    sendPacket(_ID_IP_ADDRESS, (uint32_t)value); // send
  }
  return RET_OK;
}

uint32_t ComTransmitData::get_version_all(){
  return comData.version_all;
}
bool ComTransmitData::set_version_all(uint32_t value, bool send)
{
  // received only
  comData.version_all = (uint32_t)value;
  if (send){
    sendPacket(_ID_VERSION, (uint32_t)value); // send
  }
  return RET_OK;
}
bool ComTransmitData::send_version_all()
{
  // send only
  sendPacket(_ID_VERSION, (uint32_t)comData.version_all); // send
  return RET_OK;
}


// **********************************************************************
// COMMON FUNCTIONS
// **********************************************************************
// Check if input string is a integer
boolean isValidNumber(String tString) {
  String tBuf;
  boolean decPt = false;
 
  if(tString.charAt(0) == '+' || tString.charAt(0) == '-') tBuf = &tString[1];
  else tBuf = tString;

  for(unsigned int x=0;x<tBuf.length();x++)
  {
    if(tBuf.charAt(x) == '.') {
      if(decPt) return false;
      else decPt = true; 
    }   
    else if(tBuf.charAt(x) < '0' || tBuf.charAt(x) > '9') return false;
  }
  return true;
}