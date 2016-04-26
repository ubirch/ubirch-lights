// we will ignore this diagnostic warning as we actually _want_ to return the address

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"

/*!
 * Identify approximate free SRAM available in tight conditions.
 *
 * @return available space between end of heap and stack
 */
int query_free_sram() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start: (int) __brkval);
}
#pragma GCC diagnostic pop
