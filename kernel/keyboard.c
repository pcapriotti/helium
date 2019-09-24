#include "keyboard.h"
#include "core/debug.h"
#include "core/io.h"

#include <stdint.h>

void kb_irq(void)
{
  uint8_t scancode = inb(0x60);
  kprintf("scancode: %02x\n", scancode);
}
