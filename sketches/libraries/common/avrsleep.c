#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>


void sleepabit(int howlong) {
  int i2 = 0;
  while (i2 < (howlong / 8)) {
    cli();
    // disable ADC
    //ADCSRA = 0;
    //prepare interrupts
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    // Set Watchdog settings:
    WDTCSR = (1 << WDIE) | (1 << WDE) | (1 << WDP3) | (0 << WDP2) | (0 << WDP1) | (1 << WDP0);
    sei();
    //wdt_reset();
    set_sleep_mode (SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    // turn off brown-out enable in software
    //MCUCR = bit (BODS) | bit (BODSE);
    //MCUCR = bit (BODS);
    sleep_cpu();
    // cancel sleep as a precaution
    sleep_disable();
    i2++;
  }
  wdt_disable();
}
