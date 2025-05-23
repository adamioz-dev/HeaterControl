#ifndef MEASUREMENTHANDLER_H
#define MEASUREMENTHANDLER_H

// *********************************************************************
// INCLUDES 
// *********************************************************************

// *********************************************************************
// DEFINES
// *********************************************************************

// *********************************************************************
// DATATYPES
// *********************************************************************

// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************
//puffer temp
extern uint16_t puffer_temp;
extern uint16_t puffer_temp_old;

//heater temp
extern uint16_t heater_temp;
extern uint16_t heater_temp_old;


// *********************************************************************
// DECLARATIONS
// *********************************************************************
void sensorAverageInit();
void getPufferTemperature();
void getHeaterTemperature();

#endif // MEASUREMENTHANDLER_H
// *********************************************************************