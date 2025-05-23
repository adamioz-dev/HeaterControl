#ifndef AVR_FLASHER_H
#define AVR_FLASHER_H

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
  FLASH_INFO_NOT_AVAIL    = 255,   // Not available
};

/* Function used to flash the uno at startup  */
flash_resp_t flash_uno(void);

#endif //AVR_FLASHER_H
