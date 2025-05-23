#ifndef SW_UART
#define SW_UART

// *********************************************************************
// INCLUDES
// *********************************************************************
#include <PacketSerial.h>

// *********************************************************************
// DEFINES
// *********************************************************************
// packet data 
#define ID_LEN          1  // length of the id 
#define DATA_U8_LEN     1  // length of the data u8 
#define DATA_U16_LEN    2  // length of the data u16 
#define DATA_U32_LEN    4  // length of the data u32 

#define PACKET_SIZE_U8  (ID_LEN + DATA_U8_LEN)
#define PACKET_SIZE_U16 (ID_LEN + DATA_U16_LEN)
#define PACKET_SIZE_U32 (ID_LEN + DATA_U32_LEN)

 // connection states
#define AP_STATE               1
#define SCAN_STATE             2
#define CONNECTION_LOST_STATE  3
#define DISCONNECTED_STATE     4
#define CONNECT_FAILED_STATE   5
#define WRONG_PASSWORD_STATE   6
#define NOT_CONNECTED_STATE    7
// *********************************************************************
// DATATYPES
// *********************************************************************
// packet value conversion bytes to value
union packet_value_t {
  uint8_t byte[4];
  uint16_t u16[2];
  uint32_t u32;
};

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

/* Types */
enum flash_resp_t { 
  FLASH_ERROR             = 0,   // General error
  FLASH_SUCCESS           = 1,   // succesful flashing 
  FLASH_SYNC_ERROR        = 2,   // sync error
  FLASH_WRITE_ERROR       = 3,   // write error
  FLASH_WRITE_ADDR_ERROR  = 4,   // set address error during write 
  FLASH_NOT_ACCEPT_ERROR  = 5,   // instruction not accepted
  FLASH_LEAVE_PROG_ERROR  = 6,   // Error while leaving programing session
  /* Not available info value */
  FLASH_INFO_NOT_AVAIL    = 255, // Not available
};

// version info received
union version_data_t {
  uint8_t version_byte[4];
  uint32_t all;
};

// IP address received, if connected
union IP_address_t {
  uint8_t byte[4];
  uint32_t u32;
};

enum heaterRequestModes {  // used in request_heating_state_change()
  HEATER_OFF = 0,   // requested heater to stay off
  HEATER_ON  = 1,   // requested heater on
  // special case to toggle the value between on and off
  TOOGLE_MODE = 2
};

// feedback codes
enum feedback_codes_enum {  
  PUFFER_TEMP_LOW = 0,
  HEATING_OFF,
  HEATING_ON,   
  PUFFER_TEMP_OVERHEAT,
};

enum heating_modes
{
 HEATING_MODE_INIT = 0,
 TIMER_MODE,            
 TEMPERATURE_MODE, 
 THERMOSTAT_MODE,     
 HEATING_MODE_LAST,
};

// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************
// version data
extern version_data_t version_data; // init value
// conection info from the esp8266
extern IP_address_t IP_address; // init value
// communication timeout counter
extern unsigned long last_communication_time;  // time since data was received from esp

extern PacketSerial myPacketSerial;

// *********************************************************************
// FUNCTIONS
// *********************************************************************
// send u8
void sendPacket(uint8_t id, uint8_t value);
// send u16
void sendPacket(uint8_t id, uint16_t value);
// send u32
void sendPacket(uint8_t id, uint32_t value);
// on receive packet handler
void onPacketReceived(const uint8_t* buffer, size_t size);
// setup function
void setup_sw_uart();
// loop function
void loop_sw_uart();
#endif //SW_UART
