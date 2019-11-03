#include "core/debug.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/v8086.h"
#include "core/x86.h"
#include "handlers.h"
#include "paging/paging.h"
#include "scheduler.h"

#define DEBUG_LOCAL 1

static list_t *irq_handlers[NUM_IRQ] = {0};

int irq_grab(int irq, handler_t *handler)
{
  if (irq < 0 || irq >= NUM_IRQ) return -1;
  list_t **hs = &irq_handlers[irq];

#if DEBUG_LOCAL
  if (*hs == 0) {
    serial_printf("sharing irq %#x\n", irq);
  }
#endif

  list_add(hs, &handler->head);

  return 0;
}

int irq_ungrab(int irq)
{
  if (irq < 0 || irq >= NUM_IRQ) return -1;
  irq_handlers[irq] = 0;
  return 0;
}

int handle_irq(isr_stack_t *stack)
{
  if (stack->int_num < IDT_IRQ) return 0;
  int irq = stack->int_num - IDT_IRQ;
  if (irq >= NUM_IRQ) return 0;

  list_t *hs = irq_handlers[irq];
  if (!hs) {
#if DEBUG_LOCAL
    outb(PIC_MASTER_CMD, 0x0b);
    outb(PIC_SLAVE_CMD, 0x0b);
    uint16_t isr = (inb(PIC_MASTER_CMD) << 8) | inb(PIC_SLAVE_CMD);
    outb(PIC_MASTER_CMD, 0x0a);
    outb(PIC_SLAVE_CMD, 0x0a);
    uint16_t irr = (inb(PIC_MASTER_CMD) << 8) | inb(PIC_SLAVE_CMD);
    serial_printf("unhandled irq: %u isr: %#04x irr: %#04x\n", irq, isr, irr);
#endif
    pic_eoi(irq);
    return 1;
  }

  /* call all handlers */
  list_t *item = hs;
  do {
    handler_t *handler = LIST_ENTRY(item, handler_t, head);
    handler->handle(stack);
    item = item->next;
  } while (item != hs);

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
  /* if (stack->int_num != IDT_IRQ) serial_printf("[handlers] interrupt %#x @ %p flags: %#x\n", stack->int_num, stack, cpu_flags()); */
  int done =
    v8086_manager(stack) ||
    handle_irq(stack) ||
    handle_syscall(stack) ||
    handle_page_fault(stack);

  if (!done) {
    serial_printf("Unhandled exception %#x (code: %#x)\n", stack->int_num, stack->error);
    serial_printf("  eip: %#x flags: %#x\n", stack->eip, stack->eflags);
    panic();
  }

  /* if (stack->int_num != IDT_IRQ) serial_printf("[handlers] returning from interrupt %#x @ %p flags: %#x\n", stack->int_num, stack, cpu_flags()); */
}

