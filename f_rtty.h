#include <stdint.h>
#include "config.h"

typedef enum {
  rttyZero = 0,
  rttyOne = 1,
  rttyEnd = 2
} rttyStates;

rttyStates send_rtty(char *);
extern uint8_t start_bits;
