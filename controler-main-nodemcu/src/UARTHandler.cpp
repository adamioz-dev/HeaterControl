
// *********************************************************************
// INCLUDES
// *********************************************************************
#include "Main.h"
#include "UARTHandler.h"
#include <Arduino.h>
#include <PacketSerial.h>

// *********************************************************************
// DEFINES
// *********************************************************************

// *********************************************************************
// DATATYPES
// *********************************************************************
// packet value conversion bytes to value
union packet_value_t {
  uint8_t byte[4];
  uint16_t u16[2];
  uint32_t u32;
};
// *********************************************************************
// GLOBAL VARIABLES
// *********************************************************************
// An additional PacketSerial instance.
PacketSerial myPacketSerial;
// *********************************************************************
// FUNCTIONS
// *********************************************************************
// extern functions
extern void receiveValue(uint8_t id, uint32_t value);

// *********************************************************************
// local functions
// send u8
void sendPacket(uint8_t id, uint8_t value){
  uint8_t transmit_buffer[PACKET_SIZE_U8];
  // id (1 byte)
  transmit_buffer[0] = id;
  //value (1 bytes)
  transmit_buffer[1] = value;

  // send the buffer
  myPacketSerial.send(transmit_buffer, PACKET_SIZE_U8);
}

// send u16
void sendPacket(uint8_t id, uint16_t value){
  packet_value_t packet_value;
  uint8_t transmit_buffer[PACKET_SIZE_U16];

  // value (2 bytes)
  packet_value.u16[0] = value;
  // id (1 byte)
  transmit_buffer[0] = id;
  //value (2 bytes)
  transmit_buffer[1] = packet_value.byte[0];
  transmit_buffer[2] = packet_value.byte[1];

  // send the buffer
  myPacketSerial.send(transmit_buffer, PACKET_SIZE_U16);
}

// send u32
void sendPacket(uint8_t id, uint32_t value){
  packet_value_t packet_value;
  uint8_t transmit_buffer[PACKET_SIZE_U32];

  // value (4 bytes)
  packet_value.u32 = value;
  // id (1 byte)
  transmit_buffer[0] = id;
  //value (4 bytes)
  transmit_buffer[1] = packet_value.byte[0];
  transmit_buffer[2] = packet_value.byte[1];
  transmit_buffer[3] = packet_value.byte[2];
  transmit_buffer[4] = packet_value.byte[3];

  // send the buffer
  myPacketSerial.send(transmit_buffer, PACKET_SIZE_U32);
}

void onPacketReceived(const uint8_t* buffer, size_t size)
{
  packet_value_t packet_value;
  uint8_t rx_id = 0;
  
  if (size >= PACKET_SIZE_U8) // at least id (uin8_t) and value (uin8/16/32_t)
  {
    // get first (id)
    rx_id = buffer[0];
    //get value data index 1+
    for (uint16_t i = ID_LEN; i < size; i++)
    {
      // get value data byte(s)
      packet_value.byte[i-ID_LEN] = buffer[i];
    }

    switch (size)
    {
      case PACKET_SIZE_U8: // 1 (value) + 1 (id)
      {
        // byte received
        receiveValue(rx_id, packet_value.byte[0]);
        break;
      }
      case PACKET_SIZE_U16:  // 2 (value) + 1 (id)
      {
        // 2 byte value
        receiveValue(rx_id, packet_value.u16[0]);
        break;
      }
      case PACKET_SIZE_U32:  // 4 (value) + 1 (id)
      {
        // 4 byte value
        receiveValue(rx_id, packet_value.u32);
        break;
      }
      default:
      {
        // wrong len
        return;
      }
    }
  }
}

// *********************************************************************
// SETUP
// *********************************************************************
void setup_sw_uart() {
  // We begin communication with our PacketSerial object by setting the
  // communication speed in bits / second (baud).
  myPacketSerial.begin(115200);

  // If we want to receive packets, we must specify a packet handler function.
  // The packet handler is a custom function with a signature like the
  // onPacketReceived function below.
  myPacketSerial.setPacketHandler(&onPacketReceived);

}

// *********************************************************************
// LOOP
// *********************************************************************
void loop_sw_uart(){
  myPacketSerial.update();
  // Check for a receive buffer overflow (optional).
  if (myPacketSerial.overflow())
  {
    // Send an alert via a pin (e.g. make an overflow LED) or return a
    // user-defined packet to the sender.
    //
    // Ultimately you may need to just increase your recieve buffer via the
    // template parameters (see the README.md).
    return;
  }
}
