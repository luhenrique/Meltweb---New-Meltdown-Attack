#define _GNU_SOURCE
#include <stdio.h>

int main(void) {
  size_t secret = 'S';
  // Constantly load the secret data into an AVX register
  while(1)
    asm volatile( //"pause\t\n"
                  "movq %0, %%xmm0" :: "r"(secret) );

  return 0;
}
