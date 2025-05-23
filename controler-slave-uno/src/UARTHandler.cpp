// *********************************************************************
// INCLUDES
// *********************************************************************
#include <PacketSerial.h>
#include "ControlVersion.h"

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
version_data_t version_data; // init value
// conection info from the esp8266
IP_address_t IP_address; // init value
// communication timeout counter
unsigned long last_communication_time = 0;  // time since data was received from esp

PacketSerial myPacketSerial;

// *********************************************************************
// FUNCTIONS
// *********************************************************************

// extern functions
extern void receiveValue(uint8_t id, uint32_t value);

// *********************************************************************
// local functions
// send u8
void sendPacket(uint8_t id, uint8_t value){
  uint8_t transmit_buffer[PACKET_SIZE_U8];
  // id (1 byte)
  transmit_buffer[0] = id;
  //value (1 bytes)
  transmit_buffer[1] = value;

  // send the buffer
  myPacketSerial.send(transmit_buffer, PACKET_SIZE_U8);
}

// send u16
void sendPacket(uint8_t id, uint16_t value){
  packet_value_t packet_value;
  uint8_t transmit_buffer[PACKET_SIZE_U16];

  // value (2 bytes)
  packet_value.u16[0] = value;
  // id (1 byte)
  transmit_buffer[0] = id;
  //value (2 bytes)
  transmit_buffer[1] = packet_value.byte[0];
  transmit_buffer[2] = packet_value.byte[1];

  // send the buffer
  myPacketSerial.send(transmit_buffer, PACKET_SIZE_U16);
}

// send u32
void sendPacket(uint8_t id, uint32_t value){
  packet_value_t packet_value;
  uint8_t transmit_buffer[PACKET_SIZE_U32];

  // value (4 bytes)
  packet_value.u32 = value;
  // id (1 byte)
  transmit_buffer[0] = id;
  //value (4 bytes)
  transmit_buffer[1] = packet_value.byte[0];
  transmit_buffer[2] = packet_value.byte[1];
  transmit_buffer[3] = packet_value.byte[2];
  transmit_buffer[4] = packet_value.byte[3];

  // send the buffer
  myPacketSerial.send(transmit_buffer, PACKET_SIZE_U32);
}

void onPacketReceived(const uint8_t* buffer, size_t size)
{
  packet_value_t packet_value;
  uint8_t rx_id = 0;
  
  if (size >= PACKET_SIZE_U8) // at least id (uin8_t) and value (uin8/16/32_t)
  {
    // get first (id)
    rx_id = buffer[0];
    //get value data index 1+
    for (uint16_t i = ID_LEN; i < size; i++)
    {
      // get value data byte(s)
      packet_value.byte[i-ID_LEN] = buffer[i];
    }

    switch (size)
    {
      case PACKET_SIZE_U8: // 1 (value) + 1 (id)
      {
        // byte received
        receiveValue(rx_id, packet_value.byte[0]);
        break;
      }
      case PACKET_SIZE_U16:  // 2 (value) + 1 (id)
      {
        // 2 byte value
        receiveValue(rx_id, packet_value.u16[0]);
        break;
      }
      case PACKET_SIZE_U32:  // 4 (value) + 1 (id)
      {
        // 4 byte value
        receiveValue(rx_id, packet_value.u32);
        break;
      }
      default:
      {
        // wrong len
        return;
      }
    }
    // Test send back
    myPacketSerial.send(buffer, size);
  }
}

// *********************************************************************
// SETUP
// *********************************************************************
void setup_sw_uart() {
  myPacketSerial.begin(115200);
  myPacketSerial.setPacketHandler(&onPacketReceived);

  IP_address.u32 = NOT_CONNECTED_STATE;
  // init version data
  version_data.version_byte[0] = 0;
  version_data.version_byte[1] = PROJECT_UNO_VERSION; // init with the known uno version
  version_data.version_byte[2] = 0;
  // init flash to new version state data
  version_data.version_byte[3]  = (uint8_t)FLASH_INFO_NOT_AVAIL;
  
  // version data, uno version is relevant
  sendPacket(_ID_VERSION, (uint32_t)version_data.all);

  // request data from esp in case this was a reset
  sendPacket(_ID_LAST, (uint8_t)1);
}

// *********************************************************************
// LOOP
// *********************************************************************
void loop_sw_uart(){
  myPacketSerial.update();
  // Check for a receive buffer overflow (optional).
  if (myPacketSerial.overflow())
  {
    // Send an alert via a pin (e.g. make an overflow LED) or return a
    // user-defined packet to the sender.
    //
    // Ultimately you may need to just increase your recieve buffer via the
    // template parameters (see the README.md).
    return;
  }
}
