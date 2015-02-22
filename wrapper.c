#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <string.h>

#include "manchester.h"

void init(Manchester * man, uint8_t address){
  man->addr = address;
}


// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
// Adopted from http://github.com/paeaetech/paeae/tree/master/Libraries/ds2482/

// 7 bytes
uint8_t crc8( uint8_t *addr, uint8_t len ){
    uint8_t crc = 0;
    uint8_t i = 0,
            j = 0,
            inbyte = 0,
            mix = 0;
        
    for( i = 0; i < len; i++ ){
        inbyte = addr[ i ];

        for ( j=0; j<8; j++ ){
            mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;

            if (mix)
                crc ^= 0x8C;

            inbyte >>= 1;
        }
    }
    return crc;
}

void send(Manchester* man, uint8_t to, uint8_t* data, uint8_t length){
  uint8_t buffer[BUFFER_SIZE];
  buffer[0] = to;
  buffer[1] = man->addr;
  buffer[3] = length;
  memcpy( buffer+4, data, length );
  buffer[length+4] = crc8(data, length);
  transmitArray(man, length + 5, buffer);
}


// | To | From | Type | Length | Data | Data | Data | Data | ... | Checksum
void recv(Manchester *man, uint8_t* target, uint8_t length){
  uint8_t buffer[BUFFER_SIZE];
  uint8_t done = 0;

  while( !done ){
    beginReceiveArray(length + 5, buffer);
    while(!receiveComplete()){}
    // If this packet was intended for us
    // check to see if the packet is the right size we're expecting
    // crc check?

    done = man->addr == buffer[0] && buffer[3] == length && crc8( buffer + 4, length ) == buffer[length + 4];
  }
  memcpy(target, buffer + 4, length);
}



