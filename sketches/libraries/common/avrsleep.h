#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>

/**
 * Sleep a number of seconds. Puts the AVR MCU in low power mode.
 *
 * If the number of seconds is less than 8s, the MCU will sleep in one second bits.
 * If the number of seconds is more than 8s, it will sleep 8 seconds at a time until less than 8s are left over
 * then it will fall back to one seconds bits.
 *
 * The sleep is approximate and for prolonging battery usage.
 *
 * @param seconds the number of seconds to sleep
 */
void sleep(unsigned int seconds);
