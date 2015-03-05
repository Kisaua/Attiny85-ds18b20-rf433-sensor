/*
This code is based on the Atmel Corporation Manchester
Coding Basics Application Note.

http://www.atmel.com/dyn/resources/prod_documents/doc9164.pdf

Quotes from the application note:

"Manchester coding states that there will always be a transition of the message signal
at the mid-point of the data bit frame.
What occurs at the bit edges depends on the state of the previous bit frame and
does not always produce a transition. A logical '1' is defined as a mid-point transition
from low to high and a '0' is a mid-point transition from high to low.

We use Timing Based Manchester Decode.
In this approach we will capture the time between each transition coming from the demodulation
circuit."

Timer 1 is used for a ATtiny85.

This code gives a basic data rate as 1200 bauds. In manchester encoding we send 1 0 for a data bit 0.
We send 0 1 for a data bit 1. This ensures an average over time of a fixed DC level in the TX/RX.
This is required by the ASK RF link system to ensure its correct operation.
The data rate is then 600 bits/s.
*/
#define F_CPU 1000000UL

#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#include "manchester.h"

#define DIR DDRB
#define TPORT PORTB
#define RPORT PINB


static volatile uint8_t RxPin = 255;

static int16_t rx_sample = 0;
static int16_t rx_last_sample = 0;
static uint8_t rx_count = 0;
static uint8_t rx_sync_count = 0;
static uint8_t rx_mode = RX_MODE_IDLE;

static uint16_t rx_manBits = 0; //the received manchester 32 bits
static uint8_t rx_numMB = 0; //the number of received manchester bits
static uint8_t rx_curByte = 0;

static uint8_t rx_maxBytes = 2;
static uint8_t rx_default_data[2];
static uint8_t* rx_data = rx_default_data;


void init(Manchester * man, uint8_t address){
  man->addr = address;
}

void setTxPin(Manchester *man, uint8_t pin)
{
  man->TxPin = pin;
  DIR |= pin;
}

void setRxPin(Manchester *man, uint8_t pin)
{
  RxPin = pin;
  DIR &= ~pin;
}

void workAround1MhzTinyCore(Manchester *man, uint8_t applyWorkaround)
{
  man->applyWorkAround1Mhz = applyWorkaround;
}

void delay_ms( int ms )
{
   for (int i = 0; i < ms+55; i++)
   {
      _delay_us(1);
   }
}

void setupTransmit(Manchester *man, uint8_t pin, uint8_t SF)
{
  setTxPin(man, pin);
  man->speedFactor = SF;
  //we don't use exact calculation of passed time spent outside of transmitter
  //because of high overhead associated with it, instead we use this
  //emprirically determined values to compensate for the time loss
  
  #if F_CPU == 1000000UL
    uint16_t compensationFactor = 88; //must be divisible by 8 for workaround
  #elif F_CPU == 8000000UL
    uint16_t compensationFactor = 12;
  #else //16000000Mhz
    uint16_t compensationFactor = 4;
  #endif

  man->delay1 = (HALF_BIT_INTERVAL >> man->speedFactor) - compensationFactor;
  man->delay2 = (HALF_BIT_INTERVAL >> man->speedFactor) - 2;
  man->delay3 = (HALF_BIT_INTERVAL >> man->speedFactor) - 2;
  //man->delay4 = (HALF_BIT_INTERVAL >> man->speedFactor) - 2;
  //man->delay5 = (HALF_BIT_INTERVAL >> man->speedFactor) - 2;
  #if F_CPU == 1000000UL
    man->delay2 -= 44; //22+2 = 24 is divisible by 8
    if (man->applyWorkAround1Mhz) { //definition of micro delay is broken for 1MHz speed in tiny cores as of now (May 2013)
      //this is a workaround that will allow us to transmit on 1Mhz
      //divide the wait time by 8
      man->delay1 >>= 4;
      man->delay2 >>= 2;
      man->delay3 <<= 2;
      man->delay4 = man->delay3 / 3;
      man->delay5 = man->delay3 / 6;
    }
  #endif
}


void setupReceive(Manchester *man, uint8_t pin, uint8_t SF)
{
  setRxPin(man, pin);
  MANRX_SetupReceive(SF);
}

void setup(Manchester *man, uint8_t Tpin, uint8_t Rpin, uint8_t SF)
{
  setupTransmit(man, Tpin, SF);
  setupReceive(man, Rpin, SF);
}

/*
The 433.92 Mhz receivers have AGC, if no signal is present the gain will be set
to its highest level.

In this condition it will switch high to low at random intervals due to input noise.
A CRO connected to the data line looks like 433.92 is full of transmissions.

Any ASK transmission method must first sent a capture signal of 101010........
When the receiver has adjusted its AGC to the required level for the transmisssion
the actual data transmission can occur.

We send 14 0's 1010... It takes 1 to 3 10's for the receiver to adjust to
the transmit level.

The receiver waits until we have at least 10 10's and then a start pulse 01.
The receiver is then operating correctly and we have locked onto the transmission.
*/
void transmitArray(Manchester *man, uint8_t numBytes, uint8_t *data)
{
  sendLock(man);
  
    sendZero(man); //end of capture pulses
 
  // Send a single 1
  //sendOne(man); //start data pulse
 
  // Send the user data
  for (uint8_t i = 0; i < numBytes; i++)
  {
    uint8_t mask = 0b10000000; //mask to send bits
    uint8_t d = data[i] ^ DECOUPLING_MASK;
    for (uint8_t j = 0; j < 8; j++)
    {
      if ((d & mask) == 0)
        sendZero(man);
      else
        sendOne(man);
      mask >>= 1; //get prev bit
    }//end of byte
  }//end of data

  // Send 2 terminatings 0's to correctly terminate the previous bit and to turn the transmitter off
  sendZero(man);
  sendZero(man);
}//end of send the data

void sendLock(Manchester *man)
{  
  delay_ms(man->delay3);
  TPORT |= man->TxPin;
    
   delay_ms(man->delay4);   
    TPORT &= ~(man->TxPin);

   delay_ms(man->delay5);
   TPORT |= man->TxPin;
}//end of send a Lock

void sendZero(Manchester *man)
{
   delay_ms(man->delay1);
  TPORT |= man->TxPin;
 
  delay_ms(man->delay1);
    TPORT &= ~(man->TxPin);
  }//end of send a zero

void sendOne(Manchester *man)
{
  delay_ms(man->delay1); 
  TPORT |= man->TxPin;
  
  delay_ms(man->delay2);
  TPORT &= ~(man->TxPin);
}//end of send one

void beginReceiveArray(uint8_t maxBytes, uint8_t *data)
{
  MANRX_BeginReceiveBytes(maxBytes, data);
}

uint8_t receiveComplete(void)
{
  return MANRX_ReceiveComplete();
}

//global functions

void MANRX_SetupReceive(uint8_t speedFactor)
{
// may be optional
  DDRB &= ~(RxPin);
  /*
  Timer 1 is used with a ATtiny85.
  http://www.atmel.com/Images/Atmel-2586-AVR-8-bit-Microcontroller-ATtiny25-ATtiny45-ATtiny85_Datasheet.pdf page 88
  How to find the correct value: (OCRxA +1) = F_CPU / prescaler / 1953.125
  OCR1C is 8 bit register
  */

  #if F_CPU == 1000000UL
    TCCR1 = _BV(CTC1) | _BV(CS12); // 1/8 prescaler
    OCR1C = (64 >> speedFactor) - 1;
  #elif F_CPU == 8000000UL
    TCCR1 = _BV(CTC1) | _BV(CS12) | _BV(CS11) | _BV(CS10); // 1/64 prescaler
    OCR1C = (64 >> speedFactor) - 1;
  #elif F_CPU == 16000000UL
    TCCR1 = _BV(CTC1) | _BV(CS12) | _BV(CS11) | _BV(CS10); // 1/64 prescaler
    OCR1C = (128 >> speedFactor) - 1;
  #elif F_CPU == 16500000UL
    TCCR1 = _BV(CTC1) | _BV(CS12) | _BV(CS11) | _BV(CS10); // 1/64 prescaler
    OCR1C = (132 >> speedFactor) - 1;
  #else
  #error "Manchester library only supports 1mhz, 8mhz, 16mhz, 16.5Mhz clock speeds on ATtiny85 chip"
  #endif
  
  OCR1A = 0; // Trigger interrupt when TCNT1 is reset to 0
  TIMSK |= _BV(OCIE1A); // Turn on interrupt
  TCNT1 = 0; // Set counter to 0
} //end of setupReceive

void MANRX_BeginReceiveBytes(uint8_t maxBytes, uint8_t *data)
{
  rx_maxBytes = maxBytes;
  rx_data = data;
  rx_mode = RX_MODE_PRE;
}

uint8_t MANRX_ReceiveComplete(void)
{
  return (rx_mode == RX_MODE_MSG);
}

void AddManBit(uint16_t *manBits, 
								uint8_t *numMB,
								uint8_t *curByte, uint8_t *data,
								uint8_t bit ){
  *manBits <<= 1;
  *manBits |= bit;
  (*numMB)++;
  if (*numMB == 16)
  {
    uint8_t newData = 0;
    for (int8_t i = 0; i < 8; i++)
    {
      // ManBits holds 16 bits of manchester data
      // 1 = LO,HI
      // 0 = HI,LO
      // We can decode each bit by looking at the bottom bit of each pair.
      newData <<= 1;
      newData |= (*manBits & 1); // store the one
      *manBits = *manBits >> 2; //get next data bit
    }
    data[*curByte] = newData ^ DECOUPLING_MASK;
    (*curByte)++;
    *numMB = 0;
  }
}

ISR(TIMER1_COMPA_vect)
{
  if (rx_mode < RX_MODE_MSG) //receiving something
  {
    // Increment counter
    rx_count += 8;
    
    // Check for value change
    rx_sample = (((RPORT & RxPin) > 0 ) ? 1 : 0);
    uint8_t transition = (rx_sample != rx_last_sample);

    if (rx_mode == RX_MODE_PRE)
    {
      // Wait for first transition to HIGH
      if (transition && (rx_sample == 1))
      {
        rx_count = 0;
        rx_sync_count = 0;
        rx_mode = RX_MODE_SYNC;
      }
    }
    else if (rx_mode == RX_MODE_SYNC)
    {
      // Initial sync block
      if (transition)
      {
        if(((rx_sync_count < 20) || (rx_last_sample == 1)) &&
           ((rx_count < MinCount) || (rx_count > MaxCount)))
        {
          // First 20 bits and all 1 bits are expected to be regular
          // Transition was too slow/fast
          rx_mode = RX_MODE_PRE;
        }
        else if((rx_last_sample == 0) &&
                ((rx_count < MinCount) || (rx_count > MaxLongCount)))
        {
          // 0 bits after the 20th bit are allowed to be a double bit
          // Transition was too slow/fast
          rx_mode = RX_MODE_PRE;
        }
        else
        {
          rx_sync_count++;
          if((rx_last_sample == 0) &&
             (rx_sync_count >= 20) &&
             (rx_count >= MinLongCount))
          {
            // We have seen at least 10 regular transitions
            // Lock sequence ends with unencoded bits 01
            // This is encoded and TX as HI,LO,LO,HI
            // We have seen a long low - we are now locked!
            rx_mode = RX_MODE_DATA;
            rx_manBits = 0;
            rx_numMB = 0;
            rx_curByte = 0;
          }
          else if (rx_sync_count >= 32)
          {
            rx_mode = RX_MODE_PRE;
          }
          rx_count = 0;
        }
      }
    }
    else if (rx_mode == RX_MODE_DATA)
    {
      // Receive data
      if (transition)
      {
        if((rx_count < MinCount) ||
           (rx_count > MaxLongCount))
        {
          // wrong signal length, discard the message
          rx_mode = RX_MODE_PRE;
        }
        else
        {
          if(rx_count >= MinLongCount) // was the previous bit a double bit?
          {
            AddManBit(&rx_manBits, &rx_numMB, &rx_curByte, rx_data, rx_last_sample);
          }
          if ((rx_sample == 1) &&
              (rx_curByte >= rx_maxBytes))
          {
            rx_mode = RX_MODE_MSG;
          }
          else
          {
            // Add the current bit
            AddManBit(&rx_manBits, &rx_numMB, &rx_curByte, rx_data, rx_sample);
            rx_count = 0;
          }
        }
      }
    }
    
    // Get ready for next loop
    rx_last_sample = rx_sample;
  }
}
