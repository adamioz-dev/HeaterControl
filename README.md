# HeaterControl
The repository contains the code for a heater control project. 
This is used for fire wood heaters which come with water rezervoirs (puffer) to store the heat generated and distribuite it when needed.
The hot water from the rezervoir is transfered to the building heater system to heat it up. 
Microcontrollers: 
  - slave atmega328 controller (arduino uno) responsable for:
    - inputs (lcd, encoder, button, (optional) external termostat on/off connection)
    - relay control (for controlling the water pump to transfer heat from the puffer to the building)
    - temperature measurements (heater water temperature and the puffer temperature - min/max can be set)
  - master esp8266 controller (nodemcu)  responsable for:
    - flashing of slave controller
    - conection to the internet using WiFi
    - creation of web pages for settings, control, data logging
    - conection with blynk (for smartphone access anywhere, blynk OTA)
    - flashing over the air (OTA on same network)
	- receive room temperature measurements over direct HTTP or MQTT connection (MQTT broker required).
    - MQTT publish measurements and subscribe to control values and room temperature (MQTT broker required).