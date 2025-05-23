#ifndef COMMON_H
#define COMMON_H
// *********************************************************************
// INCLUDES
// *********************************************************************
#include <Arduino.h>
// *********************************************************************
// DEFINES 
// *********************************************************************
#define save_eeprom_addr		0x0
#define FLOAT_SCALING           10
#define SNA               	    0xFFFFu // max uint16

// Used pins for the project
// Led pin
#define LEDPIN                  LED_BUILTIN
// pump relay pin
#define pumpPin                 5  // pump switch
//encoder pins
#define PINCLK                  4
#define PINDT                   3
#define PINSW                   9
//button pin
#define BUTTON_PIN              2 
//external thermostat pin
#define thermostatPin           8 

// *********************************************************************
// EXTERN VARIABLES 
// *********************************************************************

// *********************************************************************
// EXTERN FUNBCTIONS
// *********************************************************************


#endif // COMMON_H
// *********************************************************************