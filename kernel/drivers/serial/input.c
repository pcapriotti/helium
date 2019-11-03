#include "core/debug.h"
#include "core/gdt.h"
#include "core/io.h"
#include "core/interrupts.h"
#include "core/serial.h"
#include "core/x86.h"
#include "drivers/serial/input.h"
#include "drivers/keyboard/keyboard.h"
#include "handlers.h"
#include "scheduler.h"
#include "timer.h"

#define SERIAL_IRQ 0x4

kb_event_t event = {
  .pressed = 1,
  .keycode = 0,
  .mods = 0,
  .scancode = 0,
  .printable = 0,
  .timestamp = 0
};

static uint32_t tasklet_stack[512];
static task_t tasklet = {
  .state = TASK_STOPPED,
};
static int tasklet_running = 0;

static void serial_irq(struct isr_stack *stack)
{
  if (!tasklet_running) {
    tasklet.state = TASK_RUNNING;
    list_add(&sched_runqueue, &tasklet.head);
    tasklet_running = 1;
  }

  pic_mask(SERIAL_IRQ);
  pic_eoi(SERIAL_IRQ);
}
HANDLER_STATIC(serial_irq_handler, serial_irq);

static void event_init(kb_event_t *event, char c)
{
  switch (c) {
  case '\r':
    event->keycode = KC_ENT;
    event->printable = '\n';
    break;
  case 0x7f:
  case '\b':
    event->keycode = KC_BSP;
    event->printable = 0;
    break;
  default:
    event->keycode = 0;
    event->printable = c;
    break;
  }
  event->timestamp = timer_get_tick();
}

static void serial_receive(void)
{
  while (1) {
    while ((inb(COM1_PORT + SERIAL_LINE_STATUS) &
            SERIAL_STATUS_DATA_READY) != 0) {
      char c = inb(COM1_PORT + SERIAL_RECEIVE_REGISTER);
      serial_printf("[serial input] %02x\n", c);

      event_init(&event, c);
      kb_emit(&event);
    }

    sched_disable_preemption();
    sched_current->state = TASK_WAITING;
    tasklet_running = 0;
    pic_unmask(SERIAL_IRQ);
    sched_yield();
  }
}

void serial_input_init(void)
{
  outb(COM1_PORT + SERIAL_INTERRUPT_ENABLE, 1);
  irq_grab(SERIAL_IRQ, &serial_irq_handler);

  /* init tasklet */
  isr_stack_t *stack = (void *)tasklet_stack +
    sizeof(tasklet_stack) - sizeof(isr_stack_t);
  stack->eip = (uint32_t) serial_receive;
  stack->eflags = EFLAGS_IF;
  stack->cs = GDT_SEL(GDT_CODE);
  tasklet.stack = stack;
  tasklet.state = TASK_WAITING;
}
