// **********************************************************************
// INCLUDES
// *********************************************************************
#include "CommonData.h"

// *********************************************************************
// DEFINES
// *********************************************************************
#define termistorPin      A0 //puffer termistor (heat rezervoir)
#define heaterT_Pin       A1 //heater termistor (heat source)

//termistor temperature calculation
#define R1  10000
#define c1	1.009249522e-03
#define c2	2.378405444e-04
#define c3 	2.019202697e-07

#define NR_OF_READINGS			50

// *********************************************************************
// DATATYPES
// *********************************************************************

// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************
//puffer temp
uint16_t puffer_temp = 0;
uint16_t puffer_temp_old = 1;
int16_t readings_puffer[NR_OF_READINGS]; // the readings from the analog input
uint8_t readIndex_puffer = 0u;            // the index of the current reading
int32_t total_puffer = 0;                   // the running total
//heater temp
uint16_t heater_temp = 0;
uint16_t heater_temp_old = 1;
int16_t readings_heater[NR_OF_READINGS]; // the readings from the analog input
uint8_t readIndex_heater = 0u;            // the index of the current reading
int32_t total_heater = 0;                   // the running total


// *********************************************************************
// DECLARATIONS
// *********************************************************************
void sensorAverageInit();
void getPufferTemperature();
void getHeaterTemperature();

// *********************************************************************
// FUNCTIONS
// *********************************************************************
void sensorAverageInit() {
  //init values
  for (int i = 0; i < NR_OF_READINGS; i++) {
    readings_puffer[i] = 0u;
    readings_heater[i] = 0u;
  }
}

void getPufferTemperature()
{
  uint16_t local_temp = 0;
  float logR2, R2, T;
  int Vo; //termistor reading
  //Calculate temperature from termistor reading
  Vo = analogRead(termistorPin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));

  // subtract the last reading:
  total_puffer = total_puffer - readings_puffer[readIndex_puffer];

  // read from the sensor:
  readings_puffer[readIndex_puffer] = (uint16_t )((T - 273.15) * FLOAT_SCALING) ;
  // add the reading to the total:
  total_puffer = total_puffer + readings_puffer[readIndex_puffer];
  // advance to the next position in the array:
  readIndex_puffer = readIndex_puffer + 1u;

  // if we're at the end of the array...
  if (readIndex_puffer >= NR_OF_READINGS) {
    // ...wrap around to the beginning:
    readIndex_puffer = 0u;
  }

  // calculate the average:
  local_temp = (uint16_t)(total_puffer / NR_OF_READINGS);
  // limit between 0 - 99.9 C
  if (local_temp < 0u){
    local_temp = 0u;
  }else if (local_temp > 999u){  // FLOAT_SCALING
    local_temp = 999u; // FLOAT_SCALING
  }
  puffer_temp = local_temp;
}

void getHeaterTemperature()
{
  uint16_t local_temp = 0u;
  float logR2, R2, T;
  int Vo; //termistor reading
  //Calculate temperature from termistor reading
  Vo = analogRead(heaterT_Pin);
  R2 = R1 * (1023.0 / (float)Vo - 1.0);
  logR2 = log(R2);
  T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));

  // subtract the last reading:
  total_heater = total_heater - readings_heater[readIndex_heater];

  // read from the sensor:
  readings_heater[readIndex_heater] = (uint16_t)((T - 273.15) * FLOAT_SCALING);
  // add the reading to the total:
  total_heater = total_heater + readings_heater[readIndex_heater];
  // advance to the next position in the array:
  readIndex_heater = readIndex_heater + 1u;

  // if we're at the end of the array...
  if (readIndex_heater >= NR_OF_READINGS) {
    // ...wrap around to the beginning:
    readIndex_heater = 0u;
  }

  // calculate the average:
  local_temp = (uint16_t)(total_heater / NR_OF_READINGS);
  // limit between 0 - 99.9 C
  if (local_temp < 0u){
    local_temp = 0u;
  }else if (local_temp > 999u){ 
    local_temp = 999u;
  }
  heater_temp = local_temp;
}
// *********************************************************************
// END OF FILE  