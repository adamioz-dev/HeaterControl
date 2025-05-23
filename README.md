# HeaterControl
The repository contains the code for a heater control project based on the Arduino framework using the PlatformIO envirmoment using python scripts to perform automatic versioning. 

About the project: 
This is used for fire wood heaters which come with water rezervoirs (puffer) to store the heat generated and distribuite it when needed.
The hot water from the rezervoir is transfered to the building heater system to heat it up, this is achieved by controling the pump with a relay. 

Features as implemented in each controller:
1) The slave atmega328 controller (arduino uno) responsable for:
- inputs (lcd, encoder, button, (optional) external termostat on/off connection)
- relay control (for controlling the water pump to transfer heat from the puffer to the building)
- temperature measurements (heater water temperature and the puffer temperature - min/max can be set)
- over-temperature protection for the puffer.
- prevent enabling of pump when the puffer temperature is too low and heating is not possible.
- communicate the measurement/settings/states to the master controller on UART.
2) The master esp8266 controller (nodemcu)  responsable for:
- handle conection to the internet using WiFi
- hosting web pages for settings, control, data logging available on local network.
- conection with blynk (for smartphone access anywhere, blynk OTA) (optional)
- flashing over the air (OTA on same network)
- flashing of slave controller when update is received.
- receive room temperature measurements over direct HTTP or MQTT connection (MQTT broker required).
- MQTT publish measurements and subscribe to control values and room temperature (MQTT broker required).
- estimate home room temperature if it was not received for some time (for up to 2h)
