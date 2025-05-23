
// -------- includes --------

#include "common.h"
#include "Main.h"
#include "Secrets.h"

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>          //https://github.com/me-no-dev/ESPAsyncTCP
#include <ESPAsyncWebServer.h>    //https://github.com/me-no-dev/ESPAsyncWebServer
#include <ESPAsyncWiFiManager.h>  //https://github.com/alanswx/ESPAsyncWiFiManager
#include <Ticker.h>
#include <math.h>
#include "MQTT.h"

#include <NTPClient.h>
#include <WiFiUdp.h>

// web server pages
#include "WebContent.h"

// -------- defines --------
#define TIMER_TASK_AP_INTERVAL_MS 100

// connection states
#define AP_STATE               1
#define SCAN_STATE             2
#define CONNECTION_LOST_STATE  3
#define DISCONNECTED_STATE     4
#define CONNECT_FAILED_STATE   5
#define WRONG_PASSWORD_STATE   6
#define NOT_CONNECTED_STATE    7

// history data
#define MAX_HISTORY_DATA_LEN     120 //MAX should be 180 // max history length * data collection interval (1 min) => the total time

// datatypes
enum chart_history_datapoints {
  DATAPOINT_0 = 0,
  DATAPOINT_1,
  DATAPOINT_2,
  DATAPOINT_3,
  DATAPOINT_4,
  DATAPOINT_5,
  MAX_DATA_POINTS, // elements in history data
};
union connectionData_t {
  uint8_t byte[4];
  uint32_t u32;
};

//history data
typedef struct {
  uint16_t      data[MAX_DATA_POINTS+1][MAX_HISTORY_DATA_LEN+1];
  unsigned long recordTimes[MAX_HISTORY_DATA_LEN+1];
}historyData_t;

// -------- function declarations --------

// -------- Function declarations  --------
//Basic functionality when in wifiManager AP configuration mode (separate timer)
void TimerCallbackAPModeHandler();
void configModeCallback(AsyncWiFiManager *myWiFiManager);


//HTML Processor, replaces PLACEHOLDERs with the correct and filled in information
String processor(const String &var);
// Gets output state of the pin
String outputState_request();
// Check if input string is a integer
boolean isValidNumber(String tString);
//HTML settings Processor, replaces PLACEHOLDERs with the correct and filled in information
String setting_processor(const String &var) ;
String createHistoryDataForPlaceHolders(uint8_t selected_data);
//HTML settings Processor, replaces PLACEHOLDERs with the correct and filled in information
String charts_processor(const String &var);
void server_page_setup();
void initHistoryArray();
void appendToHistoryArray();
String processFeedbackChartString();
//extern functions
extern String processFeedbackCode();
extern String processFeedbackChartString();
extern void reset_uno();
extern void flash_uno_req();
extern void receiveRoomTemp(float temp);

// -------- Global variables --------
extern unsigned long g_esp_uptime; // uptime
extern ComTransmitData comTransmitData; // transmitted variables

// web page button input
const char *PARAM_STATE_SLIDER_BUTTON =   "state_slider_button";
// puffer limits
const char *PARAM_MIN_PUFFER_TEMP =       "min_puffer_temp";
const char *PARAM_MAX_PUFFER_TEMP =       "max_puffer_temp";
// mode select
const char *PARAM_HEATING_MODE =          "heating_mode";
// timer mode data
const char *PARAM_TIMER_ON_TIME =         "timer_on_time";
const char *PARAM_TIMER_OFF_TIME =        "timer_off_time";
// termometer mode data
const char *PARAM_TEMPERATURE_ROOM_TEMP = "temperature_room";         // actual room temp
const char *PARAM_TEMPERATURE_ROOM_BAT =  "termometer_battery_room";  // room termometer battery 
const char *PARAM_TERMOSTAT_MIN_TEMP =    "termostat_min_temp";       // the setpoint in termometer mode
// history data
const char *PARAM_STORE_HISTORY_DATA =    "store_history_data";
// Mqtt data
const char *PARAM_MQTT_FEATURE_STATE =    "mqtt_state";
const char *PARAM_MQTT_BROKER_IP =        "broker_ip";
const char *PARAM_MQTT_BROKER_PORT =      "broker_port";
const char *PARAM_MQTT_USER =             "mqtt_user";
const char *PARAM_MQTT_PASS =             "mqtt_pass";


// login info for web page
const char* http_username = HTTP_PAGE_USERNAME;
const char* http_password = HTTP_PAGE_PASSWORD;

//short connection info
connectionData_t connectionData;   
int8_t connectionStrength = 0; 
bool AP_mode_active = false; 

//curent time in seconds
unsigned long epochTime = 0;

// structure to hod the history data
historyData_t g_historyData;
uint16_t g_historyDataIndex = 0;
// marker to record if the history data is filled and should be read in ring mode
bool filled_history_data = false; 
bool store_history_data_flag = false; 

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
//DNS server for wifiManager
DNSServer DNS_Server;

//WiFiManager
AsyncWiFiManager wifiManager(&server, &DNS_Server);
//Ticker timer
Ticker taskAP_Timer;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

//webserver request flags 
extern bool uno_flash_req_flag;
extern bool uno_reset_req_flag;
extern bool esp_reset_req_flag;

// -------- Function definitions --------
//Basic functionality when in wifiManager AP configuration mode (separate timer)
void TimerCallbackAPModeHandler() {
  static uint16_t counter_1s = 0u;
  /* keep mark AP flag */
  AP_mode_active = true;
  // keep enabled relay
  digitalWrite(RELAY_ENABLE_PIN, HIGH); // enable the relay in AP

  // toggle the led faster than normal to signal AP mode
  toogle_led();
  
  // every 1 sec @ 100ms
  if (counter_1s < 10u)
  {
    counter_1s += 1u;
  }
  else
  {
    // clear counter
    counter_1s = 0u;
    // mark ap mode
    connectionData.u32 = AP_STATE;
    // inform uno of AP state
    (void)comTransmitData.set_IPAddress(connectionData.u32, true);
  }
  
  //comunicate with uno
  loop_sw_uart();
}

void configModeCallback(AsyncWiFiManager *myWiFiManager) {
  DEBUG_PRINT_LN("Entered config mode");
  DEBUG_PRINT_LN(WiFi.softAPIP());

  DEBUG_PRINT_LN(myWiFiManager->getConfigPortalSSID());

  // mark ap mode
  connectionData.u32 = AP_STATE;
  // enable relay
  digitalWrite(RELAY_ENABLE_PIN, HIGH); // enable the relay in AP

  /* setup communication with uno */
  setup_sw_uart();

  // Interval in microsecs
  taskAP_Timer.attach_ms(TIMER_TASK_AP_INTERVAL_MS, TimerCallbackAPModeHandler);
}

void connectionCheck(){
  // default
  connectionStrength = 0;
  //check wifi connection periodically
  switch(WiFi.status()) 
  {
    case WL_CONNECTED:  
    {
      //connection ok, write IP
      connectionData.u32 = WiFi.localIP();
      connectionStrength = WiFi.RSSI();
      break;
    }
    case WL_IDLE_STATUS:  
    {
      connectionData.u32 = SCAN_STATE;
      //connection_info = "Idle: Scan...";
      break;
    }
    case WL_CONNECTION_LOST:  
    {
      connectionData.u32 = CONNECTION_LOST_STATE;
      //connection_info = "Connection lost";
      break;
    }
    case WL_DISCONNECTED:  
    {
      if (AP_mode_active == true){
        connectionData.u32 = AP_STATE;
        //connection_info = "AP: " + String(AP_NAME);
      }else{
        connectionData.u32 = DISCONNECTED_STATE;
        //connection_info = "Disconnected";
      }
      break;
    }
    case WL_CONNECT_FAILED:  
    {
      connectionData.u32 = CONNECT_FAILED_STATE;
      //connection_info = "Connection fail";
      break;
    }
    case WL_WRONG_PASSWORD:  
    {
      connectionData.u32 = WRONG_PASSWORD_STATE;
      //connection_info = "Wrong password";
      break;
    }
    default:
    {
      if (AP_mode_active == true){
        connectionData.u32 = AP_STATE;
        //connection_info = "AP: " + String(AP_NAME);
      }else{
        connectionData.u32 = NOT_CONNECTED_STATE;
        //connection_info = "Not connected";
      }
      break;
    }
  }
  (void)comTransmitData.set_IPAddress(connectionData.u32, true);
}

//HTML Processor, replaces PLACEHOLDERs with the correct and filled in information
String processor(const String &var) {
  if (var == "FEEDBACK_PLACEHOLDER") 
  {
    // return the current feedback value
    return String(processFeedbackCode());
  }
  else if (var == "BUTTON_PLACEHOLDER") 
  {
    // return the button state value
    return outputState_request();
  }
  else if (var == "SETPOINT_PLACEHOLDER") 
  {
    // return the current setpoint value
    return String(comTransmitData.getHeatingTermostat_target_ambient(), 1); // 1 decimal point
  }
  else if (var == "TIME_ON_PLACEHOLDER") 
  {
    // return the current timer on time value
    return String(comTransmitData.getHeatingTimer_ontime());
  }
  else if (var == "TIME_OFF_PLACEHOLDER") 
  {
    // return the current timer off time value
    return String(comTransmitData.getHeatingTimer_offtime());
  }
  else if (var == "PUF_T_PLACEHOLDER") {
    return String(comTransmitData.getPuffer_Temp(),1); // 1 decimal point
  }
  else if (var == "PUF_T_MIN_PLACEHOLDER") {
    return String(comTransmitData.getPuffer_minTemp());
  }
  else if (var == "PUF_T_MAX_PLACEHOLDER") {
    return String(comTransmitData.getPuffer_maxTemp());
  }
  else if (var == "HEAT_T_PLACEHOLDER") {
    return String(comTransmitData.getBoiler_Temp(),1); // 1 decimal point
  }
  else if (var == "VERSIONPLACEHOLDER") {
    String text = "";
    version_data_t local_version_u32_form;
    // get from com 
    local_version_u32_form.all = comTransmitData.get_version_all();
    text += "Version: ";
    text += String(local_version_u32_form.version_byte[0]) + ".";
    text += String(local_version_u32_form.version_byte[1]) + ".";
    text += String(local_version_u32_form.version_byte[2]) + " ";
    if (local_version_u32_form.version_byte[3] != FLASH_INFO_NOT_AVAIL)
    {
      text += " flash: ";
      /* select text for flash status */
      switch (local_version_u32_form.version_byte[3])
      {
        case FLASH_ERROR:
        {
          text += "GENERAL_ERROR";
          break;
        }
        case FLASH_SUCCESS:
        {
          text += "SUCCESS";
          break;
        }
        case FLASH_SYNC_ERROR:
        {
          text += "SYNC_ERROR";
          break;
        }
        case FLASH_WRITE_ERROR:
        {
          text += "WRITE_ERROR";
          break;
        }
        case FLASH_WRITE_ADDR_ERROR:
        {
          text += "WRITE_ADDR_ERROR";
          break;
        }
        case FLASH_NOT_ACCEPT_ERROR:
        {
          text += "NOT_ACCEPTED_ERROR";
          break;
        }
        case FLASH_LEAVE_PROG_ERROR:
        {
          text += "LEAVE_PROG_ERROR";
          break;
        }
        default:
        {
          text += "UNKNOWN_ERROR";
          break;
        }
      }
    }
    return text;
  }
  else
  {
    // return empty string if no match found
    return String();
  }
  
}

// Gets output state of the pin
String outputState_request() {
  if (comTransmitData.getHeating_TriggerRequest()) {
    return "checked";
  } else {
    return "";
  }
  return "";
}

//HTML settings Processor, replaces PLACEHOLDERs with the correct and filled in information
String setting_processor(const String &var) {
  int local_heating_mode = comTransmitData.getHeating_mode();
  // settings menu replacements
  if (var == "AMBIENT_MIN_TEMP_PLACEHOLDER") {
    return String(comTransmitData.getHeatingTermostat_target_ambient(), 1); // 1 decimal point
  }
  else if (var == "TIMER_ON_PLACEHOLDER") {
    return String(comTransmitData.getHeatingTimer_ontime());
  }
  else if (var == "TIMER_OFF_PLACEHOLDER") {
    return String(comTransmitData.getHeatingTimer_offtime());
  }
  else if (var == "PUFFER_MIN_PLACEHOLDER") {
    return String(comTransmitData.getPuffer_minTemp());
  }
  else if (var == "PUFFER_MAX_PLACEHOLDER") {
    return String(comTransmitData.getPuffer_maxTemp());
  }
  else if (var == "BROKER_IP_PLACEHOLDER") {
    return String(mqtt.getBrokerIP());
  }
  else if (var == "MQTT_PORT_PLACEHOLDER") {
    return String(mqtt.getPort());
  }
  else if (var == "MQTT_USER_PLACEHOLDER") {
    return String(mqtt.getUsername());
  }
  else if (var == "MQTT_PASS_PLACEHOLDER") {
    return String(mqtt.getPassword());
  }
  else if (var == "MQTT_ENABLED_SELECTED") {
    return String(mqtt.isEnabled() ? "selected" : "");
  }
  else if (var == "MQTT_DISABLED_SELECTED") {
    return String(mqtt.isEnabled() ? "" : "selected");
  }
  else if (var == "HEATING_MODE_SELECT_PLACEHOLDER") {
    String text = "";
    // change the order of the select options based on the current mode
    if (local_heating_mode == TIMER_MODE){
      text += "<option value=\""+ String(TIMER_MODE)+"\">Timer</option>";
		  text += "<option value=\""+ String(TEMPERATURE_MODE)+"\">Temperature</option>";
      text += "<option value=\""+ String(THERMOSTAT_MODE)+"\">Thermostat</option>";
    }
    else if (local_heating_mode == TEMPERATURE_MODE)
    {
      text += "<option value=\""+ String(TEMPERATURE_MODE)+"\">Temperature</option>";
      text += "<option value=\""+ String(TIMER_MODE)+"\">Timer</option>";
      text += "<option value=\""+ String(THERMOSTAT_MODE)+"\">Thermostat</option>";
    }
    else if (local_heating_mode == THERMOSTAT_MODE)
    {
      text += "<option value=\""+ String(THERMOSTAT_MODE)+"\">Thermostat</option>";
      text += "<option value=\""+ String(TIMER_MODE)+"\">Timer</option>";
      text += "<option value=\""+ String(TEMPERATURE_MODE)+"\">Temperature</option>";
    }
    return text;
  }
  return String();
}

String createHistoryDataForPlaceHolders(uint8_t selected_data){
  if (false == store_history_data_flag)
  {
    // history not enabled, return empty
     return(String(""));
  }
  // the variable used
  String history_content = "";
  // current length of history
  uint16_t local_read_index = 0;
  uint16_t local_start_entry_index = 0;
  int16_t local_read_entry_index = 0;
  uint16_t local_buffer_size = 0;
  uint16_t local_historyDataIndex = g_historyDataIndex;

  if(filled_history_data == true){
    // buffer full, must use in ring mode
    if (local_historyDataIndex == 0){
      // back to the last element written
      local_start_entry_index = MAX_HISTORY_DATA_LEN - 1;
    }
    else
    {
      // offset start index
      local_start_entry_index = local_historyDataIndex;
    }
    // length is max
    local_buffer_size = MAX_HISTORY_DATA_LEN;
  }
  else{
    // buffer not yet full
    local_start_entry_index = 0;
    // take the latest written index as length/size buffer not filled yet
    local_buffer_size = local_historyDataIndex;
  }

  while( local_read_index < local_buffer_size ){
    // read position from buffer
    local_read_entry_index = local_start_entry_index + local_read_index;
    // if the history was filled already, older data should be read 
    if (local_read_entry_index >= local_buffer_size){
      // ring buffer mode
      local_read_entry_index = local_read_entry_index - local_buffer_size;
    }
    
    // get data points
    history_content += ("["+String(g_historyData.recordTimes[local_read_entry_index]) +"000,"+  String(((float)g_historyData.data[selected_data][local_read_entry_index] / FLOAT_SCALING)) + "]"); // in ms for highcharts
    
    // select next entry
    local_read_index++;
    if (local_read_index < local_buffer_size){
      // not reached last data point, add separator
      history_content += ",\n";
    }
  }
  //return the string of data to be replaced
  return(String(history_content)); // was history_content
}

//HTML settings Processor, replaces PLACEHOLDERs with the correct and filled in information
String charts_processor(const String &var)
{
  /* disable watchdog */
  ESP.wdtDisable();
  hw_wdt_disable();
   // settings menu replacements
  if (var == "FEEDBACK_DATA_PLACEHODLER") 
  {
    return createHistoryDataForPlaceHolders(DATAPOINT_0); // position in array 
  }
  else if (var == "PUFFER_DATA_PLACEHODLER") 
  {
    return createHistoryDataForPlaceHolders(DATAPOINT_1);
  }
  else if (var == "TIME_REMAINING_DATA_PLACEHODLER") 
  {
    return createHistoryDataForPlaceHolders(DATAPOINT_2);
  }
  else if (var == "ROOM_DATA_PLACEHODLER") 
  {
    return createHistoryDataForPlaceHolders(DATAPOINT_3);
  }
  else if (var == "ROOM_TARGET_DATA_PLACEHODLER") 
  {
    return createHistoryDataForPlaceHolders(DATAPOINT_4);
  }
  else if (var == "HEATER_DATA_PLACEHODLER") 
  {
    return createHistoryDataForPlaceHolders(DATAPOINT_5);
  }
    /* enable back watchdog */
  hw_wdt_enable();
  ESP.wdtEnable(8000);
  ESP.wdtFeed();
  return String();
}


void secondsToHMS( const uint32_t seconds, uint16_t &hours_out, uint8_t &min_out, uint8_t &sec_out )
{
    uint32_t t = seconds;
    sec_out = t % 60;
    t = (t - sec_out)/60;
    min_out = t % 60;
    t = (t - min_out)/60;
    hours_out = t;
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Not found");
}

void server_page_setup() {
  /* MAIN PAGES */
  /**************/
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", index_html, processor);
  });

  // chart info page
  server.on("/chart.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", html_content_chart, charts_processor);
  });

  // settings page
  server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200, "text/html", settings_html, setting_processor);
  });

  /* SET VALUE FROM PAGE*/
  /**********************/
  // button press on web
  // Send a GET request to <ESP_IP>/update?<variable>=<inputMessage>
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/update?state_slider_button=<inputMessage>
    if (request->hasParam(PARAM_STATE_SLIDER_BUTTON)) {
      inputMessage = request->getParam(PARAM_STATE_SLIDER_BUTTON)->value();
      inputParam = PARAM_STATE_SLIDER_BUTTON;
      if (isValidNumber(String(inputMessage))) {
        (void)comTransmitData.setHeating_TriggerRequest((uint8_t)TOOGLE_MODE, true);// here was changed, set the send flag
      }
      request->send(200, "text/plain", "OK");
    }
    //termostat room temperature port - from termometer
    // GET input7 value on <ESP_IP>/update?temperature_room=<inputMessage>
    else if (request->hasParam(PARAM_TEMPERATURE_ROOM_TEMP)) {
      inputMessage = request->getParam(PARAM_TEMPERATURE_ROOM_TEMP)->value();
      inputParam = PARAM_TEMPERATURE_ROOM_TEMP;
      if (isValidNumber(String(inputMessage))) {
        float room_temperature = inputMessage.toFloat();
        //update the room temp
        receiveRoomTemp(room_temperature);
      }
      request->send(200, "text/plain", "OK");
    }else if (request->hasParam(PARAM_TEMPERATURE_ROOM_BAT)) {
      inputMessage = request->getParam(PARAM_TEMPERATURE_ROOM_BAT)->value();
      inputParam = PARAM_TEMPERATURE_ROOM_BAT;
      if (isValidNumber(String(inputMessage))) {
        float room_temperature_bat = inputMessage.toFloat();
        //update the room temp battery volt
        receiveRoomTempBattery(room_temperature_bat);
      }
      request->send(200, "text/plain", "OK");
    }else {
      inputMessage = "No message sent";
      inputParam = "none";
      request->send(404, "text/plain", "Not available");
    }
    DEBUG_PRINT("Web data input ");
    DEBUG_PRINT(inputParam);
    DEBUG_PRINT(": ");
    DEBUG_PRINT_LN(inputMessage);
    
  });

  /* SET SETTINGS BY USER */
  /************************/
  // Get values inserted by user in the form fields (min puffer temp, boiler out temp)
  server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
    String returnMessageHTML;
    String inputMessage;
    String inputParam;
    // GET value on <ESP_IP>/set?input1=<inputMessage>
    if (request->hasParam(PARAM_MIN_PUFFER_TEMP)) {
      inputMessage = request->getParam(PARAM_MIN_PUFFER_TEMP)->value();
      inputParam = PARAM_MIN_PUFFER_TEMP;
      if (isValidNumber(String(inputMessage))) {
        (void)comTransmitData.setPuffer_minTemp(inputMessage.toInt(), true);
      } else {
        returnMessageHTML = "Only numbers are allowed!<br><a href=\"/settings.html\">Go back</a>";
      }
    }
    // GET input1 value on <ESP_IP>/set?max_puffer_temp=<inputMessage>
    else if (request->hasParam(PARAM_MAX_PUFFER_TEMP)) {
      inputMessage = request->getParam(PARAM_MAX_PUFFER_TEMP)->value();
      inputParam = PARAM_MAX_PUFFER_TEMP;
      if (isValidNumber(String(inputMessage))) {
        (void)comTransmitData.setPuffer_maxTemp(inputMessage.toInt(), true);
      } else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/settings.html\">Go back</a>";
      }
    } 
    // GET input1 value on <ESP_IP>/set?termostat_min_temp=<inputMessage>
    else if (request->hasParam(PARAM_TERMOSTAT_MIN_TEMP)) {
      inputMessage = request->getParam(PARAM_TERMOSTAT_MIN_TEMP)->value();
      inputParam = PARAM_TERMOSTAT_MIN_TEMP;
      if (isValidNumber(String(inputMessage))) {
        DEBUG_PRINT("Target room temp update from web page: ");
        DEBUG_PRINT_LN(inputMessage);
        (void)comTransmitData.setHeatingTermostat_target_ambient((uint16_t)(inputMessage.toFloat() * FLOAT_SCALING), true);
      } else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/\">Go back to control panel</a>";
      }
    }
    // GET input1 value on <ESP_IP>/set?heating_mode=<inputMessage>
    else if (request->hasParam(PARAM_HEATING_MODE)) {
      inputMessage = request->getParam(PARAM_HEATING_MODE)->value();
      inputParam = PARAM_HEATING_MODE;
      if (isValidNumber(String(inputMessage))) {
        (void)comTransmitData.setHeating_mode(inputMessage.toInt(), true);
      } else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/settings.html\">Go back</a>";
      }
    }
    // GET input1 value on <ESP_IP>/set?timer_on_time=<inputMessage>
    else if (request->hasParam(PARAM_TIMER_ON_TIME)) {
      inputMessage = request->getParam(PARAM_TIMER_ON_TIME)->value();
      inputParam = PARAM_TIMER_ON_TIME;
      if (isValidNumber(String(inputMessage))) {
        (void)comTransmitData.setHeatingTimer_ontime(inputMessage.toInt(), true);
      } else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/\">Go back to control panel</a>";
      }
    }
    // GET input1 value on <ESP_IP>/set?timer_off_time=<inputMessage>
    else if (request->hasParam(PARAM_TIMER_OFF_TIME)) {
      inputMessage = request->getParam(PARAM_TIMER_OFF_TIME)->value();
      inputParam = PARAM_TIMER_OFF_TIME;
      if (isValidNumber(String(inputMessage))) {
        (void)comTransmitData.setHeatingTimer_offtime(inputMessage.toInt(), true);
      } else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/\">Go back to control panel</a>";
      }
    }
	// GET input1 value on <ESP_IP>/set?timer_off_time=<inputMessage>
    else if (request->hasParam(PARAM_MQTT_FEATURE_STATE)) {
      inputMessage = request->getParam(PARAM_MQTT_FEATURE_STATE)->value();
      inputParam = PARAM_MQTT_FEATURE_STATE;
      if (isValidNumber(String(inputMessage))) {
        int value_flag = inputMessage.toInt();
        if (value_flag == 1) {
          mqtt.setEnabled(true);
        } else {
          mqtt.setEnabled(false);
        }
      }
      else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/\">Go back to control panel</a>";
      }
    }
    // GET input1 value on <ESP_IP>/set?broker_ip=<inputMessage>
    else if (request->hasParam(PARAM_MQTT_BROKER_IP)) {
      inputMessage = request->getParam(PARAM_MQTT_BROKER_IP)->value();
      inputParam = PARAM_MQTT_BROKER_IP;
      DEBUG_PRINT_LN(inputMessage);
      if ((IPAddress().fromString(inputMessage)) or (inputMessage.endsWith(".local"))) {
        mqtt.setBrokerIP(inputMessage.c_str());
      } else {
        returnMessageHTML = "Only IPs are allowed! <br><a href=\"/settings.html\">Go back</a>";
      }
    } 
    // GET input1 value on <ESP_IP>/set?broker_port=<inputMessage>
    else if (request->hasParam(PARAM_MQTT_BROKER_PORT)) {
      inputMessage = request->getParam(PARAM_MQTT_BROKER_PORT)->value();
      inputParam = PARAM_MQTT_BROKER_PORT;
      DEBUG_PRINT_LN(inputMessage);
      if (isValidNumber(String(inputMessage))) {
        uint16_t broker_port = inputMessage.toInt();
        // check if the port is in the range of 1-65535
        if (broker_port < 1 || broker_port > 65535) {
          returnMessageHTML = "Port must be in the range of 1-65535! <br><a href=\"/settings.html\">Go back</a>";
        }
        else {
          mqtt.setPort(broker_port);
        }
      } else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/settings.html\">Go back</a>";
      }
    } 
    // GET input1 value on <ESP_IP>/set?mqtt_user=<inputMessage>
    else if (request->hasParam(PARAM_MQTT_USER)) {
      inputMessage = request->getParam(PARAM_MQTT_USER)->value();
      inputParam = PARAM_MQTT_USER;
      DEBUG_PRINT_LN(inputMessage);
      mqtt.setUser(inputMessage.c_str());
    }
    // GET input1 value on <ESP_IP>/set?mqtt_pass=<inputMessage>
    else if (request->hasParam(PARAM_MQTT_PASS)) {
      inputMessage = request->getParam(PARAM_MQTT_PASS)->value();
      inputParam = PARAM_MQTT_PASS;
      DEBUG_PRINT_LN(inputMessage);
      mqtt.setPassword(inputMessage.c_str());
    }
    // GET input1 value on <ESP_IP>/set?store_history_data=<inputMessage>
    else if (request->hasParam(PARAM_STORE_HISTORY_DATA)) {
      inputMessage = request->getParam(PARAM_STORE_HISTORY_DATA)->value();
      inputParam = PARAM_STORE_HISTORY_DATA;
      if (isValidNumber(String(inputMessage))) {
        switch (inputMessage.toInt()){
          case 1:
            store_history_data_flag = true;
            returnMessageHTML = "Store history data enabled. Max " + String(MAX_HISTORY_DATA_LEN)+ " minutes. <br><a href=\"/chart.html\">Go back</a>";
            break;
          default:
            store_history_data_flag = false;
            //clear stored
            g_historyDataIndex = 0;
            filled_history_data = false;    
            returnMessageHTML = "Store history data disabled!<br><a href=\"/chart.html\">Go back</a>";
            break;
        }
      } else {
        returnMessageHTML = "Only numbers are allowed! <br><a href=\"/chart.html\">Go back</a>";
      }
    }
     else {
      inputMessage = "No message sent";
      inputParam = "none";
    }
    DEBUG_PRINT("Web setting data ");
    DEBUG_PRINT(inputParam);
    DEBUG_PRINT(": ");
    DEBUG_PRINT_LN(inputMessage);
    request->send(200, "text/html", "<h2>" + returnMessageHTML + "</h2>");
  });


  /* DISCONNECT WIFI USER */
  /************************/
  //disconnect,reset wifi settings and board from current wifi option
  //(button on web page accesing sub-page /disconnect)
  server.on("/disconnect", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(200, "text/html", "<h2> Disconnected! <br><a href=\"/settings.html\">Go back</a></h2>");
    wifiManager.resetSettings();
    WiFi.disconnect(true);
    esp_reset_req_flag = true;
  });

  /* RESET BY USER OF CONTROLLERS*/
  /*******************************/
  //reset the uno from settings
  server.on("/flash_uno", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(200, "text/html", "<h2> Uno flash triggered! <br><a href=\"/settings.html\">Go back</a></h2>");
    uno_flash_req_flag = true;
  });

  server.on("/reset_uno", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(200, "text/html", "<h2> Uno reset triggered! <br><a href=\"/settings.html\">Go back</a></h2>");
    uno_reset_req_flag = true;
  });

  server.on("/reset_esp", HTTP_GET, [](AsyncWebServerRequest *request) {
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send(200, "text/html", "<h2> ESP reset triggered! <br><a href=\"/settings.html\">Go back</a></h2>");
    esp_reset_req_flag = true;
  });

  /* VALUE TRANSMISION TO WEB */
  /****************************/
  // Send a GET request to <ESP_IP>/state_slider_button
  server.on("/state_slider_button", HTTP_GET, [](AsyncWebServerRequest *request) {  // PARAM_STATE_SLIDER_BUTTON
    request->send(200, "text/plain", String(comTransmitData.getHeating_TriggerRequest()).c_str());
  });
  // Send a GET request to <ESP_IP>/puffer_temperature
  server.on("/puffer_temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(comTransmitData.getPuffer_Temp(),1).c_str()); //one decimal point
  });
  // Send a GET request to <ESP_IP>/boiler_temperature
  server.on("/boiler_temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(comTransmitData.getBoiler_Temp(),1).c_str()); // one decimal point
  });
  // Send a GET request to <ESP_IP>/history_len
  server.on("/history_len", HTTP_GET, [](AsyncWebServerRequest *request) {
    String content = "";
    if (false == store_history_data_flag)
    {
      // return disabled text if off
      content = "disabled!";
    }
    else // history is on
    {
      if (filled_history_data == true) {
        content += ("(" + String(g_historyDataIndex) + ") " + String(MAX_HISTORY_DATA_LEN));
      }
      else{
        content += String(g_historyDataIndex);
      }
    }
    // return response
    request->send(200, "text/plain", String(content));
  });

  //server.on("/history", HTTP_GET, [](AsyncWebServerRequest *request) {
  //  request->send(200, "text/plain", String(createHistoryDataForPlaceHolders(DATAPOINT_0)));
  //});
  // Send a GET request to <ESP_IP>/feedback_text
  server.on("/feedback_text", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(processFeedbackCode()));
  });
  // Send a GET request to <ESP_IP>/chart_info - for chart info
  server.on("/chart_info", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(processFeedbackChartString()));
  });
  // Send a GET request to <ESP_IP>/connection_status
  server.on("/connection_status", HTTP_GET, [](AsyncWebServerRequest *request) {
    String display_msg = "Connection OK ";
    if(connectionStrength != 0)
    {
      display_msg += "@ ";
      display_msg += String(connectionStrength);
      display_msg += " dBm";
    }
    request->send(200, "text/plain", display_msg.c_str());
  });
  // Send a GET request to <ESP_IP>/uno_uptime
  server.on("/uno_uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
    uint32_t local_uno_uptime = comTransmitData.get_unoUptime();
    uint16_t local_hours = 0;
    uint8_t  local_min = 0;
    uint8_t  local_sec = 0;
    secondsToHMS(local_uno_uptime, local_hours, local_min, local_sec);
    request->send(200, "text/plain", (String(local_hours) + "h "+ String(local_min) + "m " + String(local_sec) + "s").c_str());
  });
  // Send a GET request to <ESP_IP>/esp_uptime
  server.on("/esp_uptime", HTTP_GET, [](AsyncWebServerRequest *request) {
    uint32_t local_esp_uptime = (uint32_t)g_esp_uptime;
    uint16_t local_hours = 0;
    uint8_t  local_min = 0;
    uint8_t  local_sec = 0;
    secondsToHMS(local_esp_uptime, local_hours, local_min, local_sec);
    request->send(200, "text/plain", (String(local_hours) + "h "+ String(local_min) + "m " + String(local_sec) + "s").c_str());
  });
  // Send a GET request to <ESP_IP>/min_room_temp
  server.on("/min_room_temp", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", String(comTransmitData.getHeatingTermostat_target_ambient()));
  });
  // Send a GET request to <ESP_IP>/heating_info
  server.on("/heating_info", HTTP_GET, [](AsyncWebServerRequest *request) {
    int local_heating_mode = comTransmitData.getHeating_mode();
    int local_heating_off_time = comTransmitData.getHeatingTimer_offtime();
    if (local_heating_mode == TIMER_MODE){
      // response message 
      String response = "Time: ";
      // display timer data
      if (local_heating_off_time == 0){
        response += "constant";
      }
      else
      {
        uint32_t local_remaining_time = comTransmitData.getHeatingTimer_TimeRemaining_sec();
        uint16_t local_hours = 0;
        uint8_t  local_min = 0;
        uint8_t  local_sec = 0;
        secondsToHMS(local_remaining_time, local_hours, local_min, local_sec);
        if (local_hours > 0){
          response += String(local_hours) + " h ";
        }
        if (local_min > 0){
          response += String(local_min) + " min ";
        }
        response += String(local_sec) + " sec ";
      }
      float local_room_temp_bat = comTransmitData.getHeatingTermostat_AmbientTempBattery();
      // display termostat data
      if ((comTransmitData.getHeatingTermostat_AmbientTemp_Valid()) && (comTransmitData.getHeatingTermostat_AmbientTemp() != 0))
      {
        response += ("| Temperature:" + String(comTransmitData.getHeatingTermostat_AmbientTemp(),1) + " &deg;C");
        if (local_room_temp_bat <= LOW_BAT_WARN)
        {
          response += " [Low battery:";
          response += String(local_room_temp_bat);
          response += " V]";
        }
      }
      else
      {
        // not known temp
      }

      request->send(200, "text/plain", response.c_str());
    }
    else if (local_heating_mode == TEMPERATURE_MODE)
    {
      String msg_text = "";
      float local_room_temp_bat = comTransmitData.getHeatingTermostat_AmbientTempBattery();
      // display termostat data
      if ((comTransmitData.getHeatingTermostat_AmbientTemp_Valid()) && (comTransmitData.getHeatingTermostat_AmbientTemp() != 0))
      {
        msg_text = ("Temperature: " + String(comTransmitData.getHeatingTermostat_AmbientTemp(),1) + " &deg;C");
      }
      else
      {
        msg_text = ("Temperature: "+ String("--.- &deg;C"));
      }

      if (local_room_temp_bat <= LOW_BAT_WARN)
      {
        msg_text += " [Low battery:";
        msg_text += String(local_room_temp_bat);
        msg_text += " V]";
      }

      /*Display timer info as well in temperature mode */
      msg_text += " | Timer: ";
      // display timer data
      if (local_heating_off_time == 0){
        msg_text += "constant";
      }
      else
      {
        uint32_t local_remaining_time = comTransmitData.getHeatingTimer_TimeRemaining_sec();
        uint16_t local_hours = 0;
        uint8_t  local_min = 0;
        uint8_t  local_sec = 0;
        secondsToHMS(local_remaining_time, local_hours, local_min, local_sec);
        if (local_hours > 0){
          msg_text += String(local_hours) + " h ";
        }
        if (local_min > 0){
          msg_text += String(local_min) + " min ";
        }
        msg_text += String(local_sec) + " sec |";
      }
      request->send(200, "text/plain", msg_text.c_str());
    }
    else if (local_heating_mode == THERMOSTAT_MODE)
    {
      String response = "Thermostat: ";
      // display termostat data
      if (comTransmitData.getHeatingExternalTermostat_State())
      {
        response += "Requesting!";
      }
      else
      {
        response += "No request.";
      }
      float local_room_temp_bat = comTransmitData.getHeatingTermostat_AmbientTempBattery();
      // display termostat data
      if ((comTransmitData.getHeatingTermostat_AmbientTemp_Valid()) && (comTransmitData.getHeatingTermostat_AmbientTemp() != 0))
      {
        response += ("| Temperature:" + String(comTransmitData.getHeatingTermostat_AmbientTemp(),1) + " &deg;C");
        if (local_room_temp_bat <= LOW_BAT_WARN)
        {
          response += " [Low battery:";
          response += String(local_room_temp_bat);
          response += " V]";
        }
      }
      else
      {
        // not known temp
      }

      request->send(200, "text/plain", response.c_str());
    }
  });

  // logout handling
  server.on("/logout", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(401);
  });

  // logout page
  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html, processor);
  });

  // no page 
  server.onNotFound(notFound);
}

void initHistoryArray(){
  g_historyData.data[MAX_DATA_POINTS][MAX_HISTORY_DATA_LEN] = {0};
  g_historyData.recordTimes[MAX_HISTORY_DATA_LEN] = {0};
}

void appendToHistoryArray(){
  uint16_t local_historyDataIndex_prev = 0;
  uint16_t local_historyDataIndex = g_historyDataIndex;
  // get new data
  uint16_t local_new_datatpoint_0 =  (uint16_t)((float)comTransmitData.get_Feedback() * 10 * 10);
  uint16_t local_new_datatpoint_1 =  (uint16_t)(((float)comTransmitData.getPuffer_Temp() * 10));
  uint16_t local_new_datatpoint_2 =  (uint16_t)((((float)comTransmitData.getHeatingTimer_TimeRemaining_sec() / 60) * 10));
  uint16_t local_new_datatpoint_3 =  (uint16_t)(((float)comTransmitData.getHeatingTermostat_AmbientTemp() * 10));
  uint16_t local_new_datatpoint_4 =  (uint16_t)(((float)comTransmitData.getHeatingTermostat_target_ambient() * 10));
  uint16_t local_new_datatpoint_5 =  (uint16_t)(((float)comTransmitData.getBoiler_Temp() * 10));

  // limit to max
  if (local_historyDataIndex >= MAX_HISTORY_DATA_LEN)
  {
    // back to index 0
    local_historyDataIndex = 0;
  }
  
  // pervious index
  if (local_historyDataIndex == 0){
    // for zero, take the last in the ring buffer
    local_historyDataIndex_prev = MAX_HISTORY_DATA_LEN - 1;
  }else
  {
    // previous measurement
    local_historyDataIndex_prev = local_historyDataIndex - 1;
  }

  // add data points (x10 for increased resolution) if any changed
  if ((g_historyData.data[DATAPOINT_0][local_historyDataIndex_prev] != local_new_datatpoint_0) ||
      (g_historyData.data[DATAPOINT_1][local_historyDataIndex_prev] != local_new_datatpoint_1) ||
      (g_historyData.data[DATAPOINT_2][local_historyDataIndex_prev] != local_new_datatpoint_2) ||
      (g_historyData.data[DATAPOINT_3][local_historyDataIndex_prev] != local_new_datatpoint_3) ||
      (g_historyData.data[DATAPOINT_4][local_historyDataIndex_prev] != local_new_datatpoint_4) ||
      (g_historyData.data[DATAPOINT_5][local_historyDataIndex_prev] != local_new_datatpoint_5))
  {
    // data is different, store it
    g_historyData.data[DATAPOINT_0][local_historyDataIndex] = local_new_datatpoint_0;
    g_historyData.data[DATAPOINT_1][local_historyDataIndex] = local_new_datatpoint_1;
    g_historyData.data[DATAPOINT_2][local_historyDataIndex] = local_new_datatpoint_2;
    g_historyData.data[DATAPOINT_3][local_historyDataIndex] = local_new_datatpoint_3;
    g_historyData.data[DATAPOINT_4][local_historyDataIndex] = local_new_datatpoint_4;
    g_historyData.data[DATAPOINT_5][local_historyDataIndex] = local_new_datatpoint_5;
    // record the time of storage
    g_historyData.recordTimes[local_historyDataIndex] = epochTime;
    // move the index
    local_historyDataIndex++;
    //if reached end start from the begining
    if (local_historyDataIndex >= MAX_HISTORY_DATA_LEN)
    {
      // mark filled up
      filled_history_data = true;
    }
    // write new index
    g_historyDataIndex = local_historyDataIndex;
  }
}

String processFeedbackChartString(){
  // add live data to open chart, not history
  String returnText = "";
    //update information required by web page with the DATAPOINT_x //data format: ( feedbackCode | air | puffer_temp | remainig time (min) | requested air | room (C) | trigger_on_off)
    // the order should be the same as for the DATAPOINT_x -> data
  returnText = ""+ String(comTransmitData.get_Feedback() * 10) +"|"+ \
                  String(comTransmitData.getPuffer_Temp()) +"|"+ \
                  String((comTransmitData.getHeatingTimer_TimeRemaining_sec() / 60)) +"|"+ \
                  String(comTransmitData.getHeatingTermostat_AmbientTemp()) +"|"+ \
                  String(comTransmitData.getHeatingTermostat_target_ambient()) +"|"+ \
                  String(comTransmitData.getBoiler_Temp());
  return returnText;
}

void setup_webserver() {
  //set host name in network
  WiFi.hostname("heater");

  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //Use this if you need to do something when your device enters configuration mode
  //on failed WiFi connection attempt. Before autoConnect()
  wifiManager.setAPCallback(configModeCallback);

  //it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  wifiManager.autoConnect(AP_NAME, AP_PASS);

  taskAP_Timer.detach();
  AP_mode_active = false;

  server_page_setup();

  // Start server
  server.begin();

  #if defined(WEBSERIAL_MSG)
  WebSerial.begin(&server);
  /* Give time to serial and web page to get initialized */
  delay(100);
  #endif

  connectionData.u32 = WiFi.localIP();

  DEBUG_PRINT("IP address: ");
  DEBUG_PRINT(connectionData.byte[0]);
  DEBUG_PRINT(".");
  DEBUG_PRINT(connectionData.byte[1]);
  DEBUG_PRINT(".");
  DEBUG_PRINT(connectionData.byte[2]);
  DEBUG_PRINT(".");
  DEBUG_PRINT_LN(connectionData.byte[3]);

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  timeClient.setTimeOffset(7200); // +2

  // init array
  initHistoryArray();

  ESP.wdtEnable(8000);
}

void loop_webserver(){
  // store history only if enabled (default off)
  if (store_history_data_flag)
  {
    // update time
    (void)timeClient.update();
    // get epoch time
    epochTime = timeClient.getEpochTime();
    // colect history data
    appendToHistoryArray(); //get new data 
  }
}
