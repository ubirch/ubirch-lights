/*
 * File:    test/stack_crypto_hashblocks.c
 * Author:  Michael Hutter, Peter Schwabe
 * Version: Wed Aug 13 04:15:13 2014 +0200
 * Public Domain
 */

#include <stdlib.h>
#include "avrnacl.h"
#include "print.h"
#include "fail.h"
#include "avr.h"

#undef crypto_hashblocks
#undef crypto_hashblocks_STATEBYTES
#undef crypto_hashblocks_BLOCKBYTES

#define CONCAT(x,y) x ## y
#define CONCAT3(x,y,z) x ## y ## z
#define XCONCAT(x,y) CONCAT(x,y)
#define XCONCAT3(x,y,z) CONCAT3(x,y,z)
#define XSTR(s) STR(s)
#define STR(s) #s

#define crypto_hashblocks             XCONCAT(crypto_hashblocks_,PRIMITIVE)
#define crypto_hashblocks_STATEBYTES XCONCAT3(crypto_hashblocks_,PRIMITIVE,_STATEBYTES)
#define crypto_hashblocks_BLOCKBYTES  XCONCAT3(crypto_hashblocks_,PRIMITIVE,_BLOCKBYTES)

#define MAXTEST_BYTES 1024

unsigned char h[crypto_hashblocks_STATEBYTES];
unsigned char m[MAXTEST_BYTES];
  
unsigned int i,mlen;

unsigned int ctr=0,newctr;
unsigned char canary;
volatile unsigned char *p;
extern unsigned char _end; 
extern unsigned char __stack; 

static unsigned int stack_count(unsigned char canary)
{
  const unsigned char *p = &_end;
  unsigned int c = 0;
  while(*p == canary && p <= &__stack)
  {
    p++;
    c++;
  }
  return c;
} 

#define WRITE_CANARY(X) {p=X;while(p>= &_end) *(p--) = canary;}

int main(void)
{
  volatile unsigned char a; /* Mark the beginning of the stack */

  for(i=0;i<5;i++)
  {
    canary = random();
    WRITE_CANARY(&a);
    crypto_hashblocks(h,m,0);
    newctr =(unsigned int)&a - (unsigned int)&_end - stack_count(canary);
    ctr = (newctr>ctr)?newctr:ctr;
  }
  print_stack(XSTR(crypto_hashblocks),0,ctr);

  for(mlen=1;mlen<=MAXTEST_BYTES;mlen<<=1)
  {
    for(i=0;i<5;i++)
    {
      canary = random();
      WRITE_CANARY(&a);
      crypto_hashblocks(h,m,mlen);
      newctr =(unsigned int)&a - (unsigned int)&_end - stack_count(canary);
      ctr = (newctr>ctr)?newctr:ctr;
    }
    print_stack(XSTR(crypto_hashblocks),mlen,ctr);
  }

  avr_end();
  return 0;
}
