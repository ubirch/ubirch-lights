#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

// enabled watchdog with the useful toolchain macros, but only in interrupt mode
#define wdt_enable_int_only(value)   \
__asm__ __volatile__ (  \
    "in __tmp_reg__,__SREG__" "\n\t"    \
    "cli" "\n\t"    \
    "wdr" "\n\t"    \
    "sts %0,%1" "\n\t"  \
    "out __SREG__,__tmp_reg__" "\n\t"   \
    "sts %0,%2" "\n\t" \
    : /* no outputs */  \
    : "M" (_SFR_MEM_ADDR(_WD_CONTROL_REG)), \
    "r" (_BV(_WD_CHANGE_BIT) | _BV(WDE)), \
    "r" ((uint8_t) ((value & 0x08 ? _WD_PS3_MASK : 0x00) | \
        _BV(WDIE) | (value & 0x07)) ) \
    : "r0"  \
)

static void _dosleep(unsigned int cycles) {
  while (cycles-- > 0) {
    cli();                         //stop interrupts to ensure the BOD timed sequence executes as required
    set_sleep_mode(SLEEP_MODE_PWR_DOWN);
    sleep_enable();
    //disable brown-out detection while sleeping (20-25µA)
    uint8_t mcucr1 = MCUCR | _BV(BODS) | _BV(BODSE);
    uint8_t mcucr2 = mcucr1 & ~_BV(BODSE);
    MCUCR = mcucr1;
    MCUCR = mcucr2;
    sei();                         //ensure interrupts enabled so we can wake up again

    sleep_cpu();                   //go to sleep
    sleep_disable();               //wake up here
  }
  MCUSR = 0;
  wdt_disable();
}

void sleep(unsigned int seconds) {
  unsigned int cycles = seconds;
  uint8_t rest = 0;

  // if its longer than 8s, sleep in chunks of 8s, else in 1s bits
  if (seconds >= 8) {
    cycles = seconds / 8;
    rest = seconds % 8;
    wdt_enable_int_only(WDTO_8S);
  } else {
    wdt_enable_int_only(WDTO_1S);
  }

  _dosleep(cycles);
  if(rest) {
    wdt_enable_int_only(WDTO_1S);
    _dosleep(rest);
  }
}

// the ISR is necessary to allow the CPU from actually sleeping
ISR (WDT_vect) { }
