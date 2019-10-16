#include "core/debug.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/v8086.h"
#include "handlers.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/rtl8139/driver.h"
#include "paging/paging.h"
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
      pic_eoi(irq);
      break;
    }
    kb_irq();
    break;
  case 11: /* TODO: make this dynamic */
    rtl8139_irq();
    break;
  default:
    {
      outb(PIC_MASTER_CMD, 0x0b);
      outb(PIC_SLAVE_CMD, 0x0b);
      uint16_t isr = (inb(PIC_MASTER_CMD) << 8) | inb(PIC_SLAVE_CMD);
      outb(PIC_MASTER_CMD, 0x0a);
      outb(PIC_SLAVE_CMD, 0x0a);
      uint16_t irr = (inb(PIC_MASTER_CMD) << 8) | inb(PIC_SLAVE_CMD);
      serial_printf("unhandled irq: %u isr: %#04x irr: %#04x\n", irq, isr, irr);
      pic_eoi(irq);
    }
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

int __attribute__((noinline)) handle_page_fault(isr_stack_t *stack)
{
  if (stack->int_num != IDT_PF) return 0;

  serial_printf("Unhandled page fault (code: %#x)\n", stack->error);
  serial_printf("  eip: %#x flags: %#x\n", stack->eip, stack->eflags);
  serial_printf("  cr2: %#x\n", CR_GET(2));
  panic();

  return 1;
}

void handle_interrupt(isr_stack_t *stack)
{
  int done =
    v8086_manager(stack) ||
    handle_irq(stack) ||
    handle_syscall(stack) ||
    handle_page_fault(stack);

  if (!done) {
    kprintf("Unhandled exception %#x (code: %#x)\n", stack->int_num, stack->error);
    kprintf("  eip: %#x flags: %#x\n", stack->eip, stack->eflags);
    panic();
  }
}

