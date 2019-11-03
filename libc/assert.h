#ifndef ASSERT_H
#define ASSERT_H

#include "core/serial.h"

#define assert(e) do { \
  if (!(e)) { \
  int col = serial_set_colour(SERIAL_COLOUR_ERR); \
  serial_printf("assert: %s\n", #e); \
  serial_set_colour(col); \
  _panic(__FILE__, __LINE__); } } while(0)

#endif /* ASSERT_H */
