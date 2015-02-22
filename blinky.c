#define F_CPU 1000000L
#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdio.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include "manchester.h"
#include "1wire.h"

uint16_t ds18b20ReadTemperature(uint8_t bus, uint8_t * id);


#define DS18B20_FAMILY_ID                0x28
#define DS18B20_ID                       77
#define DS18B20_START_CONVERSION         0x44
#define DS18B20_READ_SCRATCHPAD          0xbe
#define DS18B20_ERROR                    1000

#define BUS 							ONEWIRE_PIN_4
#define MAX_DEVICES 					1

#define TxPin 2;
uint8_t moo = 0; //last led status
uint8_t data[5] = {DS18B20_ID, 0, 0, 0, 0};
Manchester man;

uint16_t readVccVoltage(void) {
	
	ADMUX = _BV(MUX3) | _BV(MUX2);
	ADCSRA |= _BV(ADEN) | _BV( ADSC) | _BV(ADPS1) | _BV(ADPS0);
	_delay_ms(1);
    ADCSRA |= _BV(ADSC);				// Start 1st conversion
	while( ADCSRA & _BV( ADSC) ) ;		// Wait for 1st conversion to be ready...	
	ADCSRA |= _BV(ADSC);				// Start a conversion
	while( ADCSRA & _BV( ADSC) ) ;		// Wait for 1st conversion to be ready...
										//..and ignore the result
	uint8_t low  = ADCL;
	uint8_t high = ADCH;

	uint16_t adc = (high << 8) | low;		// 0<= result <=1023
	uint8_t vccx10 = (uint8_t) ( (11 * 1024) / adc); 
	ADCSRA &= ~_BV( ADEN );			// Disable ADC to save power
	return( vccx10 );
}


void system_sleep(void);

void setup_watchdog(int ii)
{
	cli();
	uint8_t tout;
	if (ii > 9)
		ii = 9;
	tout = ii & 7;
	if (ii > 7)
		tout |= _BV(WDP3);

	tout |= _BV(WDCE);

	MCUSR &= ~_BV(WDRF);
	cli();
#if defined(WDTCR)
	WDTCR |= _BV(WDCE) | _BV(WDE);
	WDTCR = tout;
	WDTCR |= _BV(WDIE);
#elif defined (WDTSCR)
	WDTCR |= _BV(WDCE) | _BV(WDE);
	WDTCR = tout;
	WDTCR |= _BV(WDIE);
#endif
sei();
}

volatile uint8_t f_wdt = 1;
uint8_t loop_count = 0;

void system_sleep()
{
	set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
	sleep_enable();
	sleep_mode();                        // System sleeps here
	sleep_disable();                     // System continues execution here when watchdog timed out 
}


int main(void)
{
  wdt_disable();
  setup_watchdog(WDTO_8S);
  init(&man, 100);
  workAround1MhzTinyCore(&man, 1);
  setupTransmit(&man, (1<<PB3),MAN_1200);
  
  
unsigned int temp = 0;
	cli();
	     DDRB |= (1<<4);
		//DDRB |= SENDER_PIN; // PB4 for the sender or Control LED
		DDRB &= ~(1 << PB4); //PB3 as input
		static oneWireDevice devices[MAX_DEVICES];
		oneWireDevice *ds18b20;
		//init_Timer_OOK();
		_delay_us(550);
		oneWireInit(BUS);
		while (oneWireSearchBuses(devices, MAX_DEVICES, BUS) != ONEWIRE_SEARCH_COMPLETE);
		ds18b20 = oneWireFindFamily(DS18B20_FAMILY_ID, devices, MAX_DEVICES);
	sei();
 // PORTB is output, all pins
   //PORTB = 0x00; // Make pins low to start

for ( ; ; ) {
  if (loop_count % 15 == 0) {
  
      for (int j = 0; j < 5; j++ ) {
      uint8_t vccx10 = readVccVoltage();
     data [1] = vccx10;
	  //data [1] = 10;
	if (ds18b20 != NULL){
			temp = ds18b20ReadTemperature(ds18b20->bus, ds18b20->id);
			data [3] = (temp & 0xFF);
			data [2] = ((temp>>8) & 0xFF); 
			//data [2] = 255;
			data [4] = oneWireDataCrc(data);
			_delay_ms(1);
			loop_count = 0;
			transmitArray(&man, 5, data);
			transmitArray(&man, 5, data);
		}
		
    //PORTB = 0x00;
    //_delay_ms(50);
    //transmitArray(&man, 3, data);
    //PORTB ^= (1<<PB4);
    //PORTB = 0x00;
    //_delay_ms(50);
    //PORTB ^= (1<<4); // invert all the pins
    //digitalWrite(3,HIGH) 
    
      }
	  }
system_sleep();
//_delay_ms(1000); // wait some time
loop_count++;
}
return 0;
}

uint16_t ds18b20ReadTemperature(uint8_t bus, uint8_t * id)
{
	int16_t temperature;

	// Reset, presence.
	if (!oneWireDetectPresence(bus))
	return DS18B20_ERROR;

	// Match the id found earlier.
	oneWireMatchRom(id, bus);

	// Send start conversion command.
	oneWireSendByte(DS18B20_START_CONVERSION, bus);

	// Wait until conversion is finished.
	// Bus line is held low until conversion is finished.

	#ifdef ONEWIRE_USE_PARASITIC_POWER
	ONEWIRE_RELEASE_BUS(bus);
	_delay_ms(850);

	#else
	while (!oneWireReadBit(bus))
	;
	#endif
	// Reset, presence.
	if(!oneWireDetectPresence(bus))
	return DS18B20_ERROR;

	// Match id again.
	oneWireMatchRom(id, bus);

	// Send READ SCRATCHPAD command.
	oneWireSendByte(DS18B20_READ_SCRATCHPAD, bus);

	// Read only two first bytes (temperature low, temperature high)
	// and place them in the 16 bit temperature variable.
	temperature = oneWireReceiveByte(bus);
	temperature |= (oneWireReceiveByte(bus) << 8);

	return temperature;
}

ISR(WDT_vect)
{
 sleep_disable();          // Disable Sleep on Wakeup
 f_wdt = 1;  // set global flag
  // Your code goes here...
  // Whatever needs to happen every 1 second
  sleep_enable();           // Enable Sleep Mode
	
}