#define F_CPU 1000000L
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

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
DDRB = 0xFF; // PORTB is output, all pins
PORTB = 0x00; // Make pins low to start
while (1) {
	for (int j = 0; j < 10; j++) {
		
	PORTB ^= 0xFF; // invert all the pins
	_delay_ms(100); // wait some time
	}
	system_sleep();
	loop_count++;
}
return 0;
}
ISR(WDT_vect)
{
 sleep_disable();          // Disable Sleep on Wakeup
 f_wdt = 1;  // set global flag
  // Your code goes here...
  // Whatever needs to happen every 1 second
  sleep_enable();           // Enable Sleep Mode
	
}