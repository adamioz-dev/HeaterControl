#ifndef MAIN_H
#define MAIN_H

#define WEBSERIAL_MSG
//#define DEBUG_SERIAL_HW

// debug messages
#if defined(DEBUG_SERIAL_HW)
/* debug second uart */
#define DEBUG_SERIAL Serial1

// Comment this out to disable prints and save space 
#define DEBUG_PRINT(x) DEBUG_SERIAL.print(x)
#define DEBUG_PRINT_TYPE(x,y) DEBUG_SERIAL.print(x,y)
#define DEBUG_PRINT_LN(x) DEBUG_SERIAL.println(x)
#define DEBUG_PRINT_TYPE_LN(x,y) DEBUG_SERIAL.println(x,y)
#elif defined(WEBSERIAL_MSG)
#include <WebSerial.h>
/* debug second web */
#define DEBUG_SERIAL WebSerial
#define DEBUG_PRINT(x) DEBUG_SERIAL.print(x)
#define DEBUG_PRINT_TYPE(x,y) DEBUG_SERIAL.print(x,y)
#define DEBUG_PRINT_LN(x) DEBUG_SERIAL.println(x)
#define DEBUG_PRINT_TYPE_LN(x,y) DEBUG_SERIAL.println(x,y)
#else
// no serial messages
#define DEBUG_PRINT(x)
#define DEBUG_PRINT_TYPE(x,y) 
#define DEBUG_PRINT_LN(x)
#define DEBUG_PRINT_TYPE_LN(x,y)
#endif

/* HW serial used to communicate with avr board */
#define COMMUNICATION_SERIAL Serial

// *********************************************************************
// INCLUDES
// *********************************************************************
#include <Arduino.h>
#include "common.h"
#include "UARTHandler.h"
#include "OTAHandler.h"
#include "WebServerHandler.h"
#include "BlynkHandler.h"
#include "AVRFlasher.h"
// *********************************************************************
// DEFINES
// *********************************************************************
// PINS
#if defined(DEBUG_SERIAL_HW)
// the LED_BUILTIN is used as HW tx serial for debug 
#define LEDPIN      5 //LED_BUILTIN is used for uart1 "serial1" tx
#else
// builtin can be used as led pin
#define LEDPIN      LED_BUILTIN
#endif

#define RELAY_ENABLE_PIN    0         // D3 conected to and gate on the relay inputs

// reset
#define UNO_RESET_PIN       14        // D5 conected to the reset pin of uno -> to target AVR RESET/DTR/GRN pin
#define UNO_RESET_WAIT      3         // in 10x seconds (is in slow task (3 = ~30sec) ) // should be bigger than the startup time of uno + margin   
#define UNO_RESET_TRIES     3         // The number of reset attempts before trying to re-flash   

// scheduler interval
#define FAST_TASK_PERIOD    100u 
#define MID_TASK_PERIOD     1000u  // 1 second
#define SLOW_TASK_PERIOD    10000u // 10 second
#define MINUTE_TASK_PERIOD  60000u // 1 minute

// room temp aproximation timeout
#define ROOM_TEMP_TIMEOUT    120  // max time to aproximate room temperature

#define SIZE_OF_VERSION  3u
// *********************************************************************
// DATATYPES
// *********************************************************************
// version info to be sent to uno
union version_data_t {
  uint8_t version_byte[4];
  uint32_t all;
};
// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************
// comunicated values 
extern ComTransmitData comTransmitData;

// uptime counter
extern unsigned long g_esp_uptime;

// marker for communication error
extern bool communication_error;

//webserver request flags 
extern bool uno_flash_req_flag;
extern bool uno_reset_req_flag;
extern bool esp_reset_req_flag;

// *********************************************************************
// FUNCTIONS
// *********************************************************************
void reset_uno();
void check_uno();
String processFeedbackCode();
void receiveValue(uint8_t id, uint32_t value);
void toogle_led();
void receiveRoomTemp(float room_temperature);
void receiveRoomTempBattery(float battery_volt);
uint16_t calculateTimeToIncreaseTemp(float local_puffer_temp, float local_room_temp);
uint16_t calculateTimeToDecreaseTemp(float local_room_temp);
/* aproximate room temp between measurement points (each min)*/
void aproximateRoomTemp();
void handleWebServerRequests(void);
void flash_uno_req(void);
void checkRTCData();
// *************************************************
//                  Tasks                        
// *************************************************
void fast_task();
void middle_task();
void slow_task();
void minute_task();
// *************************************************
//                  Scheduler                    
// *************************************************
void scheduler();
// *********************************************************************
// SETUP
// *********************************************************************
void setup();
// *********************************************************************
// LOOP
// *********************************************************************
void loop();

#endif // MAIN_H