#include "core/debug.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/v8086.h"
#include "handlers.h"
#include "keyboard.h"
#include "list.h"
#include "timer.h"

typedef struct {
  list_t head;
  void (*handle)();
} handler_t;

static list_t handlers = LIST_INIT(handlers);

int handle_irq(isr_stack_t *stack)
{
  if (stack->int_num < IDT_IRQ) return 0;
  int irq = stack->int_num - IDT_IRQ;

  switch (irq) {
  case 0:
    timer_irq();
    break;
  case 1:
    if (debug_paging) {
      uint8_t scancode = inb(0x60);
      if ((scancode & 0x80) == 0)
        debug_key_pressed = 1;
      break;
    }
    __asm__ volatile("sti");
    kb_irq();
    break;
  }

  pic_eoi(irq);
  return 1;
}

void handle_interrupt(isr_stack_t *stack)
{
  int done =
    v8086_manager(stack) ||
    handle_irq(stack);

  if (!done) {
    kprintf("Unhandled exception %#x\n");
    kprintf("  eip: %#x flags: %#x", stack->eip, stack->eflags);
    panic();
  }
}

