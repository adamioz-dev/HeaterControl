// *****************************************************************************
// INCLUDES
// *****************************************************************************
#include <EEPROM.h>
#include <rotary.h>                 // rotary handler
#include <LiquidCrystal_I2C.h>
#include <Button.h>
#include "LCDHandler.h" 
#include "UARTHandler.h"
#include "ControlVersion.h"
#include "CommonData.h"

// *********************************************************************
// DEFINES
// *********************************************************************
//periods
#define idleScreenPeriod      6000 //the value is a number of milliseconds
#define TimeOutDelayBacklight 60000  

//// menu
#define maxItemSize			  12 //longest menu item..includes char \0

/* menu related */
#define itemsPerScreen  3  // one less than max rows..

#define VALUE 	0
#define V_MIN 	1
#define V_MAX 	2
#define LINE 	  3
#define COLUMN 	4

// *********************************************************************
// DATATYPES
// *********************************************************************

// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************
int cursorLine = 1;
int displayFirstLine = 1;

//Define Menu Arrays 
char startMenu[][maxItemSize] = {"Heating", "Timer" ,"Puffer", "Temperature", "Save", "Restore", "Summary"};
//sub-options are handled

char One[][maxItemSize] = 	{"Mode",        "Timer state",	"Back"};  
char Two[][maxItemSize] = 	{"On time ",  	"Off time", 	  "Back"};
char Three[][maxItemSize] = {"Temp. min",   "Temp. max", 	  "Back"};
char Four[][maxItemSize] =  {"Temp. set",   "Hysteresis", 	"Back"};
							
const int Default[][settingsSubMenus]= {{ TIMER_MODE, HEATER_ON   },  //control 
                                        { 30,         60          },  //Temporarizare
                                        { 40,         80          },  //Puffer 
                                        { 200,        3           }}; //termostat   // FLOAT_SCALING
int Settings_array[settingMenus][settingsSubMenus]; //used Settings_array during operation

int menuItems;
char *menuSelected = NULL;
bool valueSettingHandler = false;

/* Value submenu setting */
int valueSetting[] = {0,0,0,0,0}; //value, min, max, target_line, target_column (form Settings_array 2d array)

int menuOption = 0;

//setting mode timo
unsigned long startMillisSettingsMenuTimo;
// the last time the output pin was toggled
unsigned long lastTimeBacklight = 0; 

// feedback codes
int feedbackCode_old = 0;

// flag marking idle screen status
bool isIdleScreen = false;

//backlight 
bool requestBacklight = false;

//mode select button
bool button_press_state = false;
bool button_press_data_changed = false;

// Initialize common objects
//lcd 20*4
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
// Rotary(Encoder Pin 1, Encoder Pin 2, Button Pin) Attach center to ground
Rotary mainRotaryEncoder = Rotary(PINCLK, PINDT, PINSW);        // there is no must for using interrupt pins !!
//button handler, mode selection
Button button_mode_select(BUTTON_PIN); // button between pin 2 and GND


// external variables
extern uint16_t puffer_temp;
extern uint16_t puffer_temp_old;
extern uint16_t heater_temp;
extern uint16_t heater_temp_old;
extern int16_t remainingTimeInMode_min;
extern bool pumpState_request;
extern uint16_t roomTemperature;
extern bool thermostat_pin_state;

// *********************************************************************
// DECLARATIONS
// *********************************************************************

// exten functions 
void request_heating_state_change(int input_value);

// *********************************************************************
// FUNCTION DEFINITIONS
// *********************************************************************

void loop_common(){	

	//button input for manual temp control
	handleButtonInput();
	
	//handle encoder turning 
	encoderTurnHandler();

	//handle encoder button press
	encoderButtonPressHandler();

	//handle backlight requests
	lcdBacklightHandler(); 
}

void setup_common(){
  //button init
  button_mode_select.begin();
  //init LCD
  lcd.init(); 
  
  welcome();
  //init Settings_array with default data 
  EEPROM.get(save_eeprom_addr, Settings_array);
  if (Settings_array[0][0] == -1){ // if first value is uninitialized, use defaults
	  ResetDefaults();
	}
    
  digitalWrite (PINCLK, HIGH);     // enable pull-ups
  digitalWrite (PINDT, HIGH);
  digitalWrite (PINSW, HIGH);
  
  startMillisSettingsMenuTimo = millis();  //initial start time for Settings timeout
  
  //menuItems = sizeof(startMenu) / sizeof(startMenu[0]);
  menuItems = sizeof startMenu / sizeof * startMenu;
  menuSelected = &startMenu[0][0]; //Start Menu
  menuOption = MainMenu; //Main Menu
  display_settings_menu(menuSelected, menuItems, maxItemSize); 
}

void handleButtonInput()
{ 	
	//if button is kept pressed, the state of air handling can be changed with the encoder wheel
	if (button_mode_select.pressed()){
		//set flag to mark button being currently pressed
		button_press_state = true; //button is being pressed
		requestBacklight = true;
	}
	if (button_mode_select.released()){
		//data was changed by holding down the button, the release of the button should be ignored
		button_press_state = false; //clear flag for button released
		if(button_press_data_changed){//ignore this release
			button_press_data_changed = false; //clear flag
			return;
		}
    request_heating_state_change(TOOGLE_MODE);
	} 
}

void encoderButtonPressHandler(){
  if (mainRotaryEncoder.buttonPressedReleased(25)) 
  {
    settings_mode_start();  //wake up LCD, start setting menu timeout

    // check if in idle screen
    if (isIdleScreen) //button press on idle screen, wake up and enter Settings
    { 
      if (button_press_state){
        // pressed while button was held down
        lcd.init(); // reinit displaye
      }
      //menuItems = sizeof(startMenu) / sizeof(startMenu[0]);
      menuItems = sizeof startMenu / sizeof * startMenu;
      menuSelected = &startMenu[0][0]; //Start Menu
      menuOption = MainMenu; //Main Menu
      display_settings_menu(menuSelected, menuItems, maxItemSize);
    }
    // check if in main setting menu
    else if (menuOption == MainMenu) 
    {
      selectionMainMenu();
    }
    // check if in summary menu
    else if (menuOption == SubM_Summary)
    {
      // in summary menu
      returnToMainMenu();
    } 
    else // in sub-menu
    {
      selectionSubMenu();
    }//end if menuSelected

    if ((valueSettingHandler == false) && (isIdleScreen == false))  {
      chooseMenu();
    }//end if valueSettingHandler
  }//endif buttonPressedReleased
}

/* Encoder turn input handler */
void encoderTurnHandler(){
  volatile unsigned char result = mainRotaryEncoder.process(); //process the encoder
  static bool first_idle_trigger = false; //used to avoid clearing the screen after the first time
  uint8_t local_op_mode = (uint8_t)GET_SETTING_HEATER_MODE(); //get the current operation mode
  static uint8_t local_op_mode_prev = TIMER_MODE; //init value

  /* Check if in idle screen, timeout has passed */
  if (millis() - startMillisSettingsMenuTimo >= idleScreenPeriod) 
  {
    if (isIdleScreen == false) // avoid cleaning the lcd after done once
    { 
      /* set init screen flag*/
      isIdleScreen = true;
      lcd.clear();
      first_idle_trigger = true;
    }
    // check if the operation mode was changed
    else if (local_op_mode != local_op_mode_prev) // avoid cleaning the lcd after done once
    {
      // op mode changed, update the screen
      lcd.clear();
      first_idle_trigger = true;
      requestBacklight = true; //request backlight on
      local_op_mode_prev = local_op_mode; //update the previous operation mode
    }


    /* If encoder turned and currently in idle screen */
    if ((result) && (isIdleScreen))
    {
      requestBacklight = true; //request backlight on
      // set termostat trigger temperature on idle screen
      if (result == DIR_CW)
      {
        if (TIMER_MODE == local_op_mode)
        {
          if (button_press_state == false) //button is being pressed now
          { 
            // change timer off
            if (MIN_MANUAL_OFF_TIMER_ALLOWED < GET_SETTING_TIME_OFF())
            {
              SET_SETTING_TIME_OFF(GET_SETTING_TIME_OFF() - 5); //DECREASE STEPS 
            }
          }
          else
          {
            if (MIN_MANUAL_ON_TIMER_ALLOWED < GET_SETTING_TIME_ON())
            {
              SET_SETTING_TIME_ON(GET_SETTING_TIME_ON() - 5); //DECREASE STEPS 
            }
            button_press_data_changed = true;
          }
        }
        else if (TEMPERATURE_MODE == local_op_mode)
        {
          if (MIN_TARGET_TEMPERATURE_ALLOWED < GET_SETTING_TARGET_TEMP())
          {
            SET_SETTING_TARGET_TEMP(GET_SETTING_TARGET_TEMP() - 1); //INCREASE STEPS
          }
        }
      }
      else //DIR_CCW
      { 
        if (TIMER_MODE == local_op_mode)
        {
          if (button_press_state == false) //button is being pressed now
          { 
            if (MAX_MANUAL_OFF_TIMER_ALLOWED > GET_SETTING_TIME_OFF())
            {
              // change timer of time
              SET_SETTING_TIME_OFF(GET_SETTING_TIME_OFF() + 5); //INCREASE STEPS
            }
          }
          else
          { 
            if (MAX_MANUAL_ON_TIMER_ALLOWED > GET_SETTING_TIME_ON())
            {
              SET_SETTING_TIME_ON(GET_SETTING_TIME_ON() + 5); //INCREASE STEPS
            }
            button_press_data_changed = true;
          }
        }
        else if (TEMPERATURE_MODE == local_op_mode)
        {
          if (MAX_TARGET_TEMPERATURE_ALLOWED > GET_SETTING_TARGET_TEMP())
          {
            SET_SETTING_TARGET_TEMP(GET_SETTING_TARGET_TEMP() + 1); //INCREASE STEPS
          }
        }
      }
    }
    /* Go to idle screen */
    IdleScreenLCD(first_idle_trigger); //draw/update idle screen
    first_idle_trigger = false;
  }
  else //not in idle screen 
  {
    isIdleScreen = false;
  }//End if currenMillis...

  /* Turned and NOT in idle screen */
  if ((result) && (isIdleScreen == false)) 
  {
    settings_mode_start();  //wake up LCD, refresh start settings counter 

    /* Encoder rotated counter clockwise*/
    if (result == DIR_CCW) 
    {
      /* In value seeting sub menu */
      if (valueSettingHandler == true) //only used to increase or decrease valueSetting
      {      
        /* check if in range of setting */
        if(valueSetting[V_MAX] > valueSetting[VALUE]) 
        {
          //increment one by one 
				  valueSetting[VALUE]++;
        }
        else // Reached max value
        {
          /* set to max value */
			    valueSetting[VALUE] = valueSetting[V_MAX];
		    }
        /* update the LCD displayed value */
        printLineLCD(3,String(valueSetting[VALUE]));
      } 
      else /* Currently in setting menu */
      { 
        // navigate settings menu up
        move_up();
        chooseMenu(); // mark selection
      }
    } 
    else /* Clockwise rotation */
    {
      /* Check if in value setting submenu */
      if (valueSettingHandler == true) 
      {    
        /* Check if in range of setting*/
        if(valueSetting[V_MIN] < valueSetting[VALUE])
        {
          //decrement one by one
				  valueSetting[VALUE]--;
        }
        else
        {
          /* Use min value */
			    valueSetting[VALUE] = valueSetting[V_MIN];
		    }
        /* update LCD displayed value */
        printLineLCD(3,String(valueSetting[VALUE]));
      } 
      else /* In setting menu */ 
      { 
        // navigate settings menu
        move_down();
        chooseMenu();
      }
    }
  }//end if Result
}

void IdleScreenLCD(bool first){  
  isIdleScreen = true;
  // keep the menu reset to main page
  returnToMainMenu();
  valueSettingHandler = false;

  int local_feedback_code = getFeedbackCode(); // get the current feedback code
  static int settingTimeOn_prev = 0;
  static int settingTimeOff_prev = 0;
  static int16_t time_remaining_prev = 0;
  if (TIMER_MODE == GET_SETTING_HEATER_MODE())
  {
    if ((settingTimeOn_prev != GET_SETTING_TIME_ON()) ||((settingTimeOff_prev != GET_SETTING_TIME_OFF())) ||	(first))
    {
      lcd.setCursor(0, 0);  // Move the cursor at origin
      lcd.print(F("Int "));
      if (GET_SETTING_TIME_OFF() != 0)
      {
        lcd.print(String(GET_SETTING_TIME_ON()));
        lcd.print("|"); //separator
        lcd.print(String(GET_SETTING_TIME_OFF()));
        lcd.print("' ");
      }
      else
      {
        lcd.print("constant ");
      }
      settingTimeOn_prev = GET_SETTING_TIME_ON();
      settingTimeOff_prev = GET_SETTING_TIME_OFF();
      //lcd.setCursor(9, 0);
      //lcd.write(SEPARATOR_SYMBOL);
    }
    
    // remainng time
    if ((time_remaining_prev != remainingTimeInMode_min) || 	(first))
    {
      lcd.setCursor(12, 0);
      if (GET_SETTING_HEATER_STATE() == HEATER_ON)
      {
        lcd.print(remainingTimeInMode_min);
        lcd.print("' ");
      }
      else
      {
        lcd.print("   ");
      }
      time_remaining_prev = remainingTimeInMode_min;
    }
  }
  else if(TEMPERATURE_MODE == GET_SETTING_HEATER_MODE())
  {
    lcd.setCursor(0, 0);  // Move the cursor at origin
    lcd.print(F("Set "));
    lcd.print(((float)GET_SETTING_TARGET_TEMP() / FLOAT_SCALING), 1);
		lcd.print((char)223); //degrees symbol
    lcd.setCursor(10, 0);
    lcd.print(F(":"));
    if (roomTemperature != (uint16_t)SNA){
      lcd.print(((float)roomTemperature / FLOAT_SCALING), 1);
    }
    else
    {
      lcd.print("--.-");
    }
    lcd.print((char)223); //degrees symbol
  }
  else if(THERMOSTAT_MODE == GET_SETTING_HEATER_MODE())
  {
    lcd.setCursor(0, 0);  // Move the cursor at origin
    lcd.print(F("Thermostat "));
    if (HIGH == GET_STATE_EXT_THERMOSTAT()){
      lcd.print("on! ");
    }
    else
    {
      lcd.print("off.");
    }
    lcd.print("  ");
  }

  static int local_heater_mode_prev = 0;
  if ((local_heater_mode_prev != GET_SETTING_HEATER_STATE()) || 	(first))
  {
    lcd.setCursor(17, 0);
    if (GET_SETTING_HEATER_STATE() == HEATER_ON)
    {
      lcd.print(F("ON "));
    }
    else if (GET_SETTING_HEATER_STATE() == HEATER_OFF)
    {
      lcd.print(F("OFF"));
    }
    local_heater_mode_prev = GET_SETTING_HEATER_STATE();
	}

  
  if ((puffer_temp != puffer_temp_old) || (first)){
    lcd.setCursor(0, 1); 
    lcd.print(F("Puf"));
    lcd.setCursor(4, 1);
    fillRowWhitespaces(String(puffer_temp), 3); //the first chars with whitespace, 3 chars are alocated
	  if (puffer_temp == SNA)
    {
		  lcd.print(F("--.-"));
		}else
    {
		  lcd.print(((float)puffer_temp / FLOAT_SCALING), 1);
		}
		lcd.print((char)223); //degrees symbol
	  lcd.print(" ");
    /* update puffer temp*/
    puffer_temp_old = puffer_temp;
  }

  if ((heater_temp != heater_temp_old) || (first)){
    lcd.setCursor(10, 1); 
    lcd.print(F("Heat"));
    lcd.setCursor(15, 1);
    fillRowWhitespaces(String(heater_temp), 3); //the first chars with whitespace, 3 chars are alocated
	  if (heater_temp == SNA)
    {
		  lcd.print(F("--"));
		}else
    {
		  lcd.print(((float)heater_temp / FLOAT_SCALING), 1);
		}
		//lcd.print((char)223); //degrees symbol
	  lcd.print(" ");
    /* update puffer temp*/
    heater_temp_old = heater_temp;
  }

	//lcd.setCursor(9, 1);
	//lcd.write(SEPARATOR_SYMBOL);

  static uint32_t connection_status_old = 0;
	if ((IP_address.u32 != connection_status_old) || (first)){
		lcd.setCursor(0, 2); 
    // display conection status if air handling is not used
    switch(IP_address.u32) 
    {
      case AP_STATE:  
      {
        printLineLCD( 2, F("AP Configurator"));
        break;
      }
      case SCAN_STATE:  
      {
        printLineLCD( 2, F("Idle: Scan..."));
        break;
      }
      case CONNECTION_LOST_STATE:  
      {
        printLineLCD( 2, F("Connection lost"));
        break;
      }
      case DISCONNECTED_STATE:  
      {
        printLineLCD( 2, F("Disconnected"));
        break;
      }
      case CONNECT_FAILED_STATE:  
      {
        printLineLCD( 2, F("Connection fail"));
        break;
      }
      case WRONG_PASSWORD_STATE:  
      {
        printLineLCD( 2, F("Wrong password"));
        break;
      }
      case NOT_CONNECTED_STATE:  
      {
        printLineLCD( 2, F("Not connected"));
        break;
      }
      default:
      {
        // Conencted, show ip
        lcd.setCursor(0, 2);
        lcd.print(F("IP:")); 
        if (IP_address.u32 != 0){
        lcd.print(IP_address.byte[0]); 
        lcd.print(F("."));
        lcd.print(IP_address.byte[1]); 
        lcd.print(F("."));
        lcd.print(IP_address.byte[2]); 
        lcd.print(F("."));
        lcd.print(IP_address.byte[3]); 
        lcd.print(F("   "));
        }
        break;
      }
    }
		connection_status_old = IP_address.u32;
		//requestBacklight = true; //request backlight on, fire handling info changed
  }

  if ((local_feedback_code != feedbackCode_old) || (first)){
    lcd.setCursor(0, 3); 
    switch(local_feedback_code){
    case HEATING_OFF: 
      {
      printLineLCD( 3, F("Heating off.")); // heater off 
      }
      break;
    case HEATING_ON:
      {
      printLineLCD( 3, F("Heating on!")); //heat on
      }
      break;
    case PUFFER_TEMP_LOW:
      {
      printLineLCD( 3, F("Puffer too cold!")); //low temp
      }
      break;
    case PUFFER_TEMP_OVERHEAT:
      {
      printLineLCD( 3, F("Puffer too hot!")); //high temp
      }
      break;
    default:
      {
      printLineLCD( 3, F("Unknown code!"));
      }
      break;  
    }
    feedbackCode_old = local_feedback_code; //update feedback code
	//requestBacklight = true; //request backlight on, feedback info changed
  }
}

void welcome()
{
  String frameUpDown = F("*------------------*");
  lcd.clear();
  lcd.backlight(); // turn on backlight.
  lcd.setCursor(0, 0);
  lcd.print(frameUpDown);
  lcd.setCursor(0, 1);
  lcd.print(F("|   Control panel  |"));
  lcd.setCursor(0, 2);
  lcd.print(F("|      Heater      |"));
  lcd.setCursor(0, 3);
  lcd.print(frameUpDown);
  delay(500);
}//end welcome

void printLineLCD(int line, String text){
  lcd.setCursor(0, line);
  lcd.print(text);
  // fill the rest with white spaces
  for(uint8_t i=0; i<(20 - text.length()); i++){
       lcd.print(" ");
   }
}

void fillRowWhitespaces(String text, int totalLen){
  for(uint8_t i=0; i<(totalLen - text.length()); i++){
       lcd.print(" ");
   }
}

void display_settings_menu(const char *menuInput, int ROWS, int COLS)
{
  int n = 4;     //4 rows
  lcd.clear();
  printLineLCD(0,F("...Settings........."));
  
  if (ROWS < n - 1) {
    n = ROWS + 1;
  }

  for (int i = 0; i < n - 1; i++) {
    lcd.setCursor(1, i + 1); //(col, row)
    for (int j = 0; j < COLS; j++) {
      if (*(menuInput + ((displayFirstLine + i - 1) * COLS + j)) != '\0') {
        lcd.print(*(menuInput + ((displayFirstLine + i - 1) * COLS + j)));
      }//end if
    }//end for j
  }//end for i

  lcd.setCursor(0, (cursorLine - displayFirstLine) + 1);
  lcd.print(">");
}//end display_settings_menu

void move_down()
{
  if (cursorLine == (displayFirstLine + itemsPerScreen - 1)) {
    displayFirstLine++;
  }
  //If reached last item...roll over to first item
  if (cursorLine == menuItems) {
    cursorLine = 1;
    displayFirstLine = 1;
  } else {
    cursorLine = cursorLine + 1;
  }
}//end move_down

/* Encoder turn handler up direction */
void move_up()
{
  if ((displayFirstLine == 1) & (cursorLine == 1)) {
    if (menuItems > itemsPerScreen - 1) {
      displayFirstLine = menuItems - itemsPerScreen + 1;
    }
  } else if (displayFirstLine == cursorLine) {
    displayFirstLine--;
  }

  if (cursorLine == 1) {
    if (menuItems > itemsPerScreen - 1) {
      cursorLine = menuItems; //roll over to last item
    }
  } else {
    cursorLine = cursorLine - 1;
  }
}//end move_up

void chooseMenu()
{
  // Check in which menu we are
  if (menuOption == SubM_Summary)
  {
    display_summary();
  } else {
    display_settings_menu(menuSelected, menuItems, maxItemSize);
  }
}//end chooseMenu

void selectionMainMenu()
{
  //valueSettingHandler activates valueSetting to decrease/increase amount
  if (valueSettingHandler == true) {
    //Settings at line , column
    //              subMenu            option             actual value
    Settings_array[valueSetting[LINE]][valueSetting[COLUMN]] = valueSetting[VALUE]; // set the value
    //reset
    valueSettingHandler = false;
    valueSetting[VALUE] = 	0; // actual value
    valueSetting[V_MIN] = 	0; // min
    valueSetting[V_MAX] = 	0; // max
    valueSetting[LINE] = 	0; // line
    valueSetting[COLUMN] = 	0; // column
    returnToMainMenu();
  } else {

    lcd.clear();

    switch (cursorLine - 1)
    {
      case 0:
        //go to the first line in submenu
        displayFirstLine = 1;       
        cursorLine = 1;             
        menuItems = sizeof One / sizeof * One;
        menuSelected = &One[0][0];
        menuOption = SubM_Heater;
        break;
      case 1:
        //go to the first line in submenu
        displayFirstLine = 1;       
        cursorLine = 1;             
        menuItems = sizeof Two / sizeof * Two;
        menuSelected = &Two[0][0];
        menuOption = SubM_Timer;
        break;
      case 2:
        //go to the first line in submenu
        displayFirstLine = 1;       
        cursorLine = 1;             
        menuItems = sizeof Three / sizeof * Three;
        menuSelected = &Three[0][0];
        menuOption = SubM_Puffer;
        break;
      case 3:
        //go to the first line in submenu
        displayFirstLine = 1;       
        cursorLine = 1;             
        menuItems = sizeof Four / sizeof * Four;
        menuSelected = &Four[0][0];
        menuOption = SubM_Termo;
        break;
      case 4:       
        SaveEEPROM();
        returnToMainMenu();
        break;
	    case 5:
		    ResetDefaults();
        returnToMainMenu();
        break;
	    case 6:
        //go to the first line in submenu
        displayFirstLine = 1;       
        cursorLine = 1;             
        menuItems = sizeof startMenu / sizeof * startMenu;
        menuOption = SubM_Summary;
        break;
    }//end switch
  }//end else
}//end selectionMainMenu

void setValueSetting(int subMenu, int pos){
  printLineLCD(0,String(startMenu[subMenu-1]));
  String subMenuName = "";
  int minimalValue = MIN_SETTING_VALUE;
  int maximalValue = MAX_SETTING_VALUE;
  // select the correct submenu for current menu
  if(subMenu == SubM_Heater){
    if (pos == MODE_SETTING){ 
      printLineLCD(2,F("0-OFF/1-ON"));
	    minimalValue = HEATER_OFF;
      maximalValue = HEATER_ON;
    }else{
      printLineLCD(2,F("1-Tim/2-Tem/3-Ter")); // heater modes( timer,temperature or termostat )
	    minimalValue = TIMER_MODE;
      maximalValue = THERMOSTAT_MODE; 
    }
    subMenuName = One[pos];
  }
  else if(subMenu == SubM_Timer)
  { // manual timer (on/off)
    if (pos == MIN_VALUE){
        printLineLCD(2,F("timer pump on (m.)"));
        minimalValue = MIN_MANUAL_ON_TIMER_ALLOWED;
        maximalValue = MAX_MANUAL_ON_TIMER_ALLOWED;
      }else if (pos == MAX_VALUE){
        printLineLCD(2,F("timer pump off (m.)"));
        minimalValue = MIN_MANUAL_OFF_TIMER_ALLOWED;
        maximalValue = MAX_MANUAL_OFF_TIMER_ALLOWED;
    }
    subMenuName = Two[pos];
  }
  else if(subMenu == SubM_Puffer)
  {
    if (pos == MIN_VALUE){
      minimalValue = MIN_PUFFER_LOW_TEMP_ALLOWED;
      maximalValue = MAX_PUFFER_LOW_TEMP_ALLOWED;
      printLineLCD(2,F("min puffer temp.(C)"));
    }else if (pos == MAX_VALUE){
      minimalValue = MIN_PUFFER_HIGH_TEMP_ALLOWED;
      maximalValue = MAX_PUFFER_HIGH_TEMP_ALLOWED;
      printLineLCD(2,F("max puffer temp.(C)"));
    }
    subMenuName = Three[pos];
  }
  else if(subMenu == SubM_Termo)
  {
    if (pos == MIN_VALUE){
      minimalValue = MIN_TARGET_TEMPERATURE_ALLOWED;
      maximalValue = MAX_TARGET_TEMPERATURE_ALLOWED;
      printLineLCD(2,F("target temp.(/10)"));
    }else if (pos == MAX_VALUE){
      minimalValue = 1;
      maximalValue = 20;
      printLineLCD(2,F("hyst. (C) (/10)"));
    }
    subMenuName = Four[pos];
  }
  printLineLCD(1,String(subMenuName));

  valueSettingHandler = true;
  valueSetting[VALUE] = Settings_array[menuOption - 1][pos]; //get the actual value
  valueSetting[V_MIN] = minimalValue;
  valueSetting[V_MAX] = maximalValue;
  valueSetting[LINE] = 	menuOption - 1;
  valueSetting[COLUMN] = pos;
  
  printLineLCD(3,String(valueSetting[VALUE])); 
}

void selectionSubMenu()
{
  lcd.clear();
  switch (cursorLine - 1)
  {
    case 0:
      if (menuOption != SubM_Summary){ 
        setValueSetting(menuOption, cursorLine - 1);
      }
      break;
    case 1:
      if (menuOption != SubM_Summary){ 
        setValueSetting(menuOption, cursorLine - 1);
      }
      break;
    case 2:
      break;
    default:
      break;
  }//end switch
  //delay(800);
  returnToMainMenu();
}//end selectionSubMenu

void ResetDefaults()
{
  //Reset Settings to defaults
  for (int i = 0; i < settingMenus; i++)
  {
    for (int j = 0; j < settingsSubMenus; j++)
    {
      Settings_array[i][j] = Default[i][j];
    }
  }

  //exit settings
  startMillisSettingsMenuTimo = 0; //clear wait
  IdleScreenLCD(true); //draw/update idle screen
}//end ResetDefaults

void SaveEEPROM()
{
  //save Settings_array to eeprom
  EEPROM.put(save_eeprom_addr, Settings_array);

  //exit settings
  startMillisSettingsMenuTimo = 0; //clear wait
  IdleScreenLCD(true); //draw/update idle screen
}//end SaveEEPROM

void secondsToHMS( const uint32_t seconds, uint16_t &hours_out, uint8_t &min_out, uint8_t &sec_out )
{
    uint32_t t = seconds;
    sec_out = t % 60;
    t = (t - sec_out)/60;
    min_out = t % 60;
    t = (t - min_out)/60;
    hours_out = t;
}

void display_summary()
{ 
  // uptime vars
  uint16_t hours_out = 0;
  uint8_t min_out = 0;
  uint8_t sec_out = 0;
  uint32_t total_seconds = (uint32_t)millis() / 1000;

  int n = 4;      // 4 rows in LCD
  lcd.clear();
  printLineLCD(0,F("...Summary.........."));

  if (menuItems < n - 1) {
    n = menuItems + 1;
  }

  for (int lcd_line_index = 1; lcd_line_index < n; lcd_line_index++)
  {
    int pos = displayFirstLine + lcd_line_index - 2; // calculate position
    lcd.setCursor(1, lcd_line_index);   //(col, row)
    if (pos < settingMenus){ // dont display for reset, memorise and sumary
      // display the settings
      lcd.print(startMenu[pos]);
      lcd.setCursor(13, lcd_line_index);
      lcd.print(Settings_array[pos][MIN_VALUE]);
      lcd.setCursor(17, lcd_line_index);
      lcd.print(Settings_array[pos][MAX_VALUE]);
    }
    else // pos after the last settings
    {
      // instead of reset, memorize and summay display other info
      switch (pos){
        case settingMenus:
        {
          lcd.print("UP: ");
          // convert to hours minutes and seconds
          secondsToHMS(total_seconds, hours_out, min_out, sec_out);
          // display hours 
          if (hours_out >= 1){
            // hours
            lcd.print(hours_out);
            lcd.print(F("h "));
          }
          // display minutes 
          if (min_out >= 1){
            // in minute range
            lcd.print(min_out);
            lcd.print(F("m "));
          }
          // and the seconds
          lcd.print(sec_out);
          lcd.print(F("s "));
          break;
        }
        case settingMenus + 1:
        {
          lcd.print("Vers: ");
          lcd.print(version_data.version_byte[0]);  // major verion, controled by esp build, received in com
          lcd.print(".");
          lcd.print(PROJECT_UNO_VERSION);  //  uno version, baked in
          lcd.print(".");
          lcd.print(version_data.version_byte[2]);  // nodemcu version, received in com
          /*display if flashing was done, received in com */
          if (version_data.version_byte[3] != (uint8_t)FLASH_INFO_NOT_AVAIL)
          {
            lcd.print("/");
            if (version_data.version_byte[3] == (uint8_t)FLASH_SUCCESS)
            {
              lcd.print("OK");
            }
            else
            {
              /* display error code */
              lcd.print("E");
              lcd.print((uint8_t)version_data.version_byte[3]);
            }
          }
        break;
        }
        case settingMenus + 2:
        {
          lcd.print("WiFi PW: 23456789");
          break;
        }
        default:
          break;
      }
    }
  }
  lcd.setCursor(0, (cursorLine - displayFirstLine) + 1);
  lcd.print(">");
}//end display_summary

void returnToMainMenu()
{
  displayFirstLine = 1;
  cursorLine = 1;
  menuItems = sizeof startMenu / sizeof * startMenu;
  menuSelected = &startMenu[0][0];
  menuOption = MainMenu;
}//end returnToMainMenu

void lcdBacklightHandler(){
  
  if (requestBacklight == true) 
  {
    lastTimeBacklight = millis();
    lcd.backlight(); // turn on backlight.
  }
  if ((millis() - lastTimeBacklight) > TimeOutDelayBacklight) {
    lcd.noBacklight(); // turn off backlight
  }
  requestBacklight = false;
}

void settings_mode_start()
{
  requestBacklight = true;
  startMillisSettingsMenuTimo = millis();  //initial start time
}//end settings_mode_start
