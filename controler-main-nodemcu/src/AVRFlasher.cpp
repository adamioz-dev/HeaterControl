#include <Arduino.h>
#include "AVRFlasher.h"
#include "AVRHexArray.h"
#include <avr/pgmspace.h>
#include "Main.h"

/* Gloabal variables */
int readBuff[16];
int readBuffLength;

/// DECLARATIONS 
/* function used to wait and read response */
void readBytes();
/* Function for uploading hex to uno using the STK500 protocol */
flash_resp_t doUpload();

/// IMPLEMENTATIONS 
/* function used to flash the uno avr */
flash_resp_t flash_uno(void) 
{
  flash_resp_t errorCode = FLASH_ERROR;
  int flashAttempts = 0;
  bool repeat = true;

  /* disable HW watchdog, keep SW watchdog enabled*/
  hw_wdt_disable();

  COMMUNICATION_SERIAL.begin(115200);   // start serial

  while (repeat)
  {
    ESP.wdtFeed();
    COMMUNICATION_SERIAL.flush();
    DEBUG_PRINT_LN("Initializing...");
    errorCode = doUpload();
    switch(errorCode)
    {
      case FLASH_ERROR:
      {
        DEBUG_PRINT_LN("Error: Something is wrong...");
        // exit loop
        repeat = false;
        break;
      }
      case FLASH_SYNC_ERROR:
      {
        DEBUG_PRINT_LN("Error: Out of sync. Good bye!");
        // if out of sync, try 3 more times to get in
        if (flashAttempts <= 3){
          flashAttempts++;
          break;
        }
        else
        {
          // exit loop
          repeat = false;
          break;
        }
      }
      case FLASH_WRITE_ADDR_ERROR:
      {
        DEBUG_PRINT_LN("Error: Write address selection error.");
        // exit loop
        repeat = false;
        break;
      }
      case FLASH_NOT_ACCEPT_ERROR:
      {
        DEBUG_PRINT_LN("Error: Not accept instructions. Good bye!");
        // exit loop
        repeat = false;
        break;
      }
      case FLASH_WRITE_ERROR:
      {
        DEBUG_PRINT_LN("Error: Writing to memory failed.");
        // exit loop
        repeat = false;
        break;
      }
      case FLASH_SUCCESS:
      {
        DEBUG_PRINT_LN("Success, flash OK!");
        // exit loop
        repeat = false;
        break;
      }
      default:
      {
        DEBUG_PRINT_LN("Error: Unknown error!");
        // exit loop
        repeat = false;
        break;
      }
    }
  }
  flashAttempts = 0;
  /* enable back HW watchdog */
  hw_wdt_enable();
  
  return errorCode;
}

/* function used to wait and read response */
void readBytes() {
  int max_wait = 40; // 1000ms / 25 delay => 40
  int preBuff = 0;

  readBuffLength = 0;

  while ((COMMUNICATION_SERIAL.available() <= 0) && (max_wait > 0))
  {
    ESP.wdtFeed();
    /* wait for data resp */
    delay(25);
    max_wait--;
  }

  while (COMMUNICATION_SERIAL.available() > 0) {
    preBuff = COMMUNICATION_SERIAL.read();
    if (preBuff == 0xFC) {
      preBuff = 0x10;
    }
    readBuff[readBuffLength] = preBuff;
    readBuffLength++;
  }
}

flash_resp_t doUpload() {

  int groove = 50, i, j, start, end, address, laddress, haddress,buff[128], buffLength;

  /* Configure serial */
  COMMUNICATION_SERIAL.flush();
  COMMUNICATION_SERIAL.begin(115200); // For Mini Pro must be 56700
  COMMUNICATION_SERIAL.flush();

  // Reset the target board
  reset_uno();
  
  DEBUG_PRINT_LN("Get in sync with target AVR");
  DEBUG_PRINT_LN("Send: 30 20");
  for (i = 0; i < 8; i++) {
    COMMUNICATION_SERIAL.write((char)0x30); // STK_GET_SYNC
    COMMUNICATION_SERIAL.write((char)0x20); // STK_CRC_EOP
    delay(200);
    ESP.wdtFeed();
  }
  COMMUNICATION_SERIAL.flush();            
  readBytes();
  
  DEBUG_PRINT("Recv(");
  DEBUG_PRINT(readBuffLength);
  DEBUG_PRINT("): ");
  for (int auxi = 0; auxi < readBuffLength; auxi ++){
    DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
  }
  if (readBuffLength < 2 || readBuff[readBuffLength-2] != (char)0x14 || readBuff[readBuffLength-1] != (char)0x10) {
    return FLASH_SYNC_ERROR;
  }
  DEBUG_PRINT_LN();
  COMMUNICATION_SERIAL.flush();
  DEBUG_PRINT_LN("Send: 41 81 20");
  COMMUNICATION_SERIAL.write((char)0x41);
  COMMUNICATION_SERIAL.write((char)0x81);
  COMMUNICATION_SERIAL.write((char)0x20);
  delay(groove);
  readBytes();
  DEBUG_PRINT("Recv(");
  DEBUG_PRINT(readBuffLength);
  DEBUG_PRINT("): ");
  for (int auxi = 0; auxi < readBuffLength; auxi ++){
    DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINT_LN();
  DEBUG_PRINT_LN("Send: 41 82 20");
  COMMUNICATION_SERIAL.write((char)0x41);
  COMMUNICATION_SERIAL.write((char)0x82);
  COMMUNICATION_SERIAL.write((char)0x20);
  delay(groove);
  readBytes();
  DEBUG_PRINT("Recv(");
  DEBUG_PRINT(readBuffLength);
  DEBUG_PRINT("): ");
  for (int auxi = 0; auxi < readBuffLength; auxi ++){
    DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINT_LN();
  COMMUNICATION_SERIAL.flush();
  // Set the programming parameters
  DEBUG_PRINT_LN("Send: 42 86 00 00 01 01 01 01 03 ff ff ff ff 00 80 04 00 00 80 00 20");
  COMMUNICATION_SERIAL.write((char)0x42);
  COMMUNICATION_SERIAL.write((char)0x86);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x01);
  COMMUNICATION_SERIAL.write((char)0x01);
  COMMUNICATION_SERIAL.write((char)0x01);
  COMMUNICATION_SERIAL.write((char)0x01);
  COMMUNICATION_SERIAL.write((char)0x03);
  COMMUNICATION_SERIAL.write((char)0xff);
  COMMUNICATION_SERIAL.write((char)0xff);
  COMMUNICATION_SERIAL.write((char)0xff);
  COMMUNICATION_SERIAL.write((char)0xff);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x80);
  COMMUNICATION_SERIAL.write((char)0x04);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x80);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x20);
  delay(groove);
  readBytes();
  DEBUG_PRINT("Recv(");
  DEBUG_PRINT(readBuffLength);
  DEBUG_PRINT("): ");
  for (int auxi = 0; auxi < readBuffLength; auxi ++){
    DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINT_LN();
  // Set the extended programming parameters
  DEBUG_PRINT_LN("Send: 45 05 04 d7 c2 00 20");
  COMMUNICATION_SERIAL.write((char)0x45);
  COMMUNICATION_SERIAL.write((char)0x05);
  COMMUNICATION_SERIAL.write((char)0x04);
  COMMUNICATION_SERIAL.write((char)0xd7);
  COMMUNICATION_SERIAL.write((char)0xc2);
  COMMUNICATION_SERIAL.write((char)0x00);
  COMMUNICATION_SERIAL.write((char)0x20);
  delay(groove);
  readBytes();
  DEBUG_PRINT("Recv(");
  DEBUG_PRINT(readBuffLength);
  DEBUG_PRINT("): ");
  for (int auxi = 0; auxi < readBuffLength; auxi ++){
    DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINT_LN();
  DEBUG_PRINT_LN("Send: 50 20");
  COMMUNICATION_SERIAL.write((char)0x50);
  COMMUNICATION_SERIAL.write((char)0x20);
  delay(groove);
  readBytes();
  DEBUG_PRINT("Recv(");
  DEBUG_PRINT(readBuffLength);
  DEBUG_PRINT("): ");
  for (int auxi = 0; auxi < readBuffLength; auxi ++){
    DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINT_LN();
  if (readBuffLength != 2 || readBuff[0] != (char)0x14 || readBuff[1] != (char)0x10) {
    return FLASH_NOT_ACCEPT_ERROR;
  } else {
    DEBUG_PRINT_LN ("AVR device initialized and ready to accept instructions");
  }
  // Now comes the interesting part
  // We pour blocks of data from sketch[] and into buff, ready to write
  // to the AVR's flash
  // We only write in blocks of 128 or less bytes
  DEBUG_PRINT_LN("Send instructions...");
  address = 0;
  for (i = 0; i < sketchLength; i += 128) {
    ESP.wdtFeed();
    start = i;
    end = i + 127;
    if (sketchLength <= end) {
      end = sketchLength - 1;
    }
    buffLength = end - start + 1;
    for (j = 0; j < buffLength; j++) {
      buff[j] = pgm_read_byte(sketch+i+j);
    }
    // The buffer is now filled with the appropriate bytes
    
    // Set the address of the avr's flash memory to write to
    haddress = address / 256;
    laddress = address % 256;
    address += 64; // For the next iteration
    COMMUNICATION_SERIAL.write((char)0x55);
    COMMUNICATION_SERIAL.write(laddress);
    COMMUNICATION_SERIAL.write(haddress);
    COMMUNICATION_SERIAL.write((char)0x20);
    
    readBytes();
    /* debug print flash error cause */
    if (readBuffLength > 2 || ((readBuff[0] != 0x14) && (readBuff[0] != 0xF8)) || readBuff[1] != 0x10) {
      DEBUG_PRINT_LN();
      DEBUG_PRINT("Recv(");
      DEBUG_PRINT(readBuffLength);
      DEBUG_PRINT("): ");    
      for (int auxi = 0; auxi < readBuffLength; auxi ++){
        DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
      }

      DEBUG_PRINT_LN();
      DEBUG_PRINT("Set address error at ");
      DEBUG_PRINT(address);
      DEBUG_PRINT_LN();
      return FLASH_WRITE_ADDR_ERROR;
    }
    // Write the block
    COMMUNICATION_SERIAL.write((char)0x64);
    COMMUNICATION_SERIAL.write((char)0x00);
    COMMUNICATION_SERIAL.write(buffLength);
    COMMUNICATION_SERIAL.write((char)0x46);
    for (j = 0; j < buffLength; j++) {
      COMMUNICATION_SERIAL.write(buff[j]);
    }
    COMMUNICATION_SERIAL.write((char)0x20);

    readBytes();
    /* print fail cause */
    if (readBuffLength > 2 || ((readBuff[0] != 0x14) && (readBuff[0] != 0xF8)) || readBuff[1] != 0x10) {
      DEBUG_PRINT_LN();
      DEBUG_PRINT("Recv(");
      DEBUG_PRINT(readBuffLength);
      DEBUG_PRINT("): ");    
      for (int auxi = 0; auxi < readBuffLength; auxi ++){
        DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
      }
      DEBUG_PRINT_LN("Write the block error! ");
      return FLASH_WRITE_ERROR;
    }
    //DEBUG_PRINT(".");
  }
  DEBUG_PRINT_LN("Done!");
  DEBUG_PRINT_LN("Leave programming mode");
  DEBUG_PRINT_LN("Send: 51 20");
  COMMUNICATION_SERIAL.write((char)0x51);
  COMMUNICATION_SERIAL.write((char)0x20);
  delay(10);
  readBytes();
  DEBUG_PRINT("Recv(");
  DEBUG_PRINT(readBuffLength);
  DEBUG_PRINT("): ");
  for (int auxi = 0; auxi < readBuffLength; auxi ++){
    DEBUG_PRINT_TYPE(readBuff[auxi], HEX); DEBUG_PRINT(" ");
  }
  DEBUG_PRINT_LN("");
  /* check if last instruction is ok */
  /* Last response for leave programming should be 0x14 0x10 for OK but id does not matter because flash was ok 
  If answare is shifter or not catched in time, report OK anyway */
  if (readBuffLength != 2 || readBuff[0] != 0x14 || readBuff[1] != 0x10) {
    return FLASH_LEAVE_PROG_ERROR;
  } else {
    DEBUG_PRINT("Done. Thank you.");
    // Reset the target board
    reset_uno();
  }
  // Please note: no sketch verification is done by the uploader sketch!
  return FLASH_SUCCESS;
}


