#ifndef LCD_HANDLER_H
#define LCD_HANDLER_H
// *********************************************************************
// INCLUDES
// *********************************************************************
#include "CommonData.h"
// *********************************************************************
// DEFINES 
// *********************************************************************
//Define Main menu and sub menus...
#define MainMenu      0    //Main
#define SubM_Heater   1    //Control menu (air draw and temproizator mode
#define SubM_Timer    2    //Manual timer
#define SubM_Puffer   3    //Puffer
#define SubM_Termo    4    //Termostat
#define SubM_Reset    5    //Reset to defaults
#define SubM_Summary  6    //Summary

// constant values
#define MIN_SETTING_VALUE		0
#define MAX_SETTING_VALUE		500

//options for subMenus
#define MIN_VALUE           0 // first option, min value configurations
#define MAX_VALUE           1 // second option, max value configurations
#define MODE_SETTING        1 // second option, mode select configurations

//time intervals
#define MIN_MANUAL_ON_TIMER_ALLOWED 20
#define MAX_MANUAL_ON_TIMER_ALLOWED 90
#define MIN_MANUAL_OFF_TIMER_ALLOWED 0
#define MAX_MANUAL_OFF_TIMER_ALLOWED 360
// puffer
#define MIN_PUFFER_LOW_TEMP_ALLOWED   20
#define MAX_PUFFER_LOW_TEMP_ALLOWED   50
#define MIN_PUFFER_HIGH_TEMP_ALLOWED  60
#define MAX_PUFFER_HIGH_TEMP_ALLOWED  80
//termostat
#define MIN_TARGET_TEMPERATURE_ALLOWED 100
#define MAX_TARGET_TEMPERATURE_ALLOWED 350

/* Setter and getters for settings array */
/* Heater */
#define SET_SETTING_HEATER_MODE(value)     ((value == TIMER_MODE) || (value == TEMPERATURE_MODE) || (value == THERMOSTAT_MODE) ? (Settings_array[SubM_Heater-1][0] = value) : value )
#define SET_SETTING_HEATER_STATE(value)    ((value == HEATER_OFF) || (value == HEATER_ON) ? (Settings_array[SubM_Heater-1][MODE_SETTING] = value) : value )
#define GET_SETTING_HEATER_MODE()          (Settings_array[SubM_Heater-1][0])
#define GET_SETTING_HEATER_STATE()         (Settings_array[SubM_Heater-1][MODE_SETTING])

/* Timer */
#define SET_SETTING_TIME_ON(value)    ((value >= MIN_MANUAL_ON_TIMER_ALLOWED) && (value <= MAX_MANUAL_ON_TIMER_ALLOWED) ? (Settings_array[SubM_Timer-1][MIN_VALUE] = value) : value )
#define SET_SETTING_TIME_OFF(value)   ((value >= MIN_MANUAL_OFF_TIMER_ALLOWED) && (value <= MAX_MANUAL_OFF_TIMER_ALLOWED) ? (Settings_array[SubM_Timer-1][MAX_VALUE] = value) : value )
#define GET_SETTING_TIME_ON()         (Settings_array[SubM_Timer-1][MIN_VALUE])
#define GET_SETTING_TIME_OFF()        (Settings_array[SubM_Timer-1][MAX_VALUE])
/* puffer */
#define SET_SETTING_PUFF_MIN(value)   ((value >= MIN_PUFFER_LOW_TEMP_ALLOWED) && (value <= MAX_PUFFER_LOW_TEMP_ALLOWED) ? (Settings_array[SubM_Puffer-1][MIN_VALUE] = value) :  value )
#define SET_SETTING_PUFF_MAX(value)   ((value >= MIN_PUFFER_HIGH_TEMP_ALLOWED) && (value <= MAX_PUFFER_HIGH_TEMP_ALLOWED) ? (Settings_array[SubM_Puffer-1][MAX_VALUE] = value) : value )
#define GET_SETTING_PUFF_MIN()        (Settings_array[SubM_Puffer-1][MIN_VALUE])
#define GET_SETTING_PUFF_MAX()        (Settings_array[SubM_Puffer-1][MAX_VALUE])

/* room temp */ // FLOAT_SCALING ( 221 -> 22.1C)
#define SET_SETTING_TARGET_TEMP(value)      ((value >= MIN_TARGET_TEMPERATURE_ALLOWED) && (value <= MAX_TARGET_TEMPERATURE_ALLOWED) ? (Settings_array[SubM_Termo-1][0] = value) : value )
#define GET_SETTING_TARGET_TEMP()           (Settings_array[SubM_Termo-1][0])  
#define SET_SETTING_TARGET_TEMP_HYST(value) ((value >= 1) && (value <= 20) ? (Settings_array[SubM_Termo-1][1] = value) : value )
#define GET_SETTING_TARGET_TEMP_HYST()      (Settings_array[SubM_Termo-1][1])  

/* external termostat */
#define GET_STATE_EXT_THERMOSTAT()            (thermostat_pin_state)  
#define SET_STATE_EXT_THERMOSTAT(value)       (thermostat_pin_state = value)  

#define settingMenus 		  4
#define settingsSubMenus 	2


/* Extern variables*/
extern int Settings_array[settingMenus][settingsSubMenus]; // needed to be able to access by macro setters and getters

// ***********************************************************************
// DECLARATIONS
// ***********************************************************************
// declarations for the functions
void loop_common();
void setup_common();
//handle the button press input for manual/termostat mode
void handleButtonInput();
/* Encoder button press input handler */
void encoderButtonPressHandler();
/* Encoder turn input handler */
void encoderTurnHandler();
/* Idle screeen handler */
void IdleScreenLCD(bool first);
/* Start-up text */
void welcome();
//print whole line on the LCD, difference is filled with whitespaces
void printLineLCD(int line, String text);
//fill with whitespaces the difference between len and the length of the string
void fillRowWhitespaces(String text, int totalLen);
/* Settings menu display handler */
void display_settings_menu(const char *menuInput, int ROWS, int COLS);
/* Encoder turn handler down direction */
void move_down();
/* Encoder turn handler up direction */
void move_up();
/* Selection of item in menu */
void chooseMenu();
/* Selection in the main settings menu */
void selectionMainMenu();
/* Sub-menu for settings value handling */
void setValueSetting(int subMenu, int pos);
/* Sub-menu selection */
void selectionSubMenu();
/* Reset setting to default */
void ResetDefaults();
/* Store data to eeprom */
void SaveEEPROM();
// convert seconds to hours, minutes and seconds
void secondsToHMS( const uint32_t seconds, uint16_t &hours_out, uint8_t &min_out, uint8_t &sec_out );
/* Summary menu */
void display_summary();
/* Return to main menu hadler */
void returnToMainMenu();
/* Backlight handler */
void lcdBacklightHandler();
/* Transition to settings menu handler */
void settings_mode_start();
/* Getter for feedback code */
int getFeedbackCode();
#endif // LCD_HANDLER_H
// *********************************************************************