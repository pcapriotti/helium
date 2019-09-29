#include "core/debug.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/v8086.h"
#include "handlers.h"
#include "keyboard.h"
#include "scheduler.h"
#include "timer.h"

int handle_irq(isr_stack_t *stack)
{
  if (stack->int_num < IDT_IRQ) return 0;
  int irq = stack->int_num - IDT_IRQ;
  if (irq >= NUM_IRQ) return 0;

  switch (irq) {
  case IRQ_TIMER:
    timer_irq(stack);
    break;
  case IRQ_KEYBOARD:
    if (debug_paging) {
      uint8_t scancode = inb(0x60);
      if ((scancode & 0x80) == 0)
        debug_key_pressed = 1;
      break;
    }
    kb_irq();
    break;
  default:
    pic_eoi(irq);
    break;
  }

  return 1;
}

int handle_syscall(isr_stack_t *stack)
{
  if (stack->int_num != IDT_SYSCALL) return 0;

  switch (stack->eax) {
  case SYSCALL_YIELD:
    sched_schedule(stack);
    return 1;
  default:
    kprintf("Invalid syscall %#x\n", stack->eax);
    kprintf("  eip: %#x flags: %#x\n", stack->eip, stack->eflags);
    return 1;
  }
}

void handle_interrupt(isr_stack_t *stack)
{
  int done =
    v8086_manager(stack) ||
    handle_irq(stack) ||
    handle_syscall(stack);

  if (!done) {
    kprintf("Unhandled exception %#x (code: %#x)\n", stack->int_num, stack->error);
    kprintf("  eip: %#x flags: %#x\n", stack->eip, stack->eflags);
    panic();
  }
}

