#include "atomic.h"
#include "core/debug.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "scheduler.h"
#include "timer.h"

#include <assert.h>

#define PIT_FREQ 1193182

typedef struct {
  unsigned long count;
  unsigned long quantum;
} timer_t;

static timer_t timer;
static task_t *waiting;

static int timer_waiting_locked = 0;

void timer_waiting_lock()
{
  cli();
  timer_waiting_locked++;
}

void timer_waiting_unlock()
{
  timer_waiting_locked--;
  assert(timer_waiting_locked >= 0);
  if (timer_waiting_locked == 0) {
    sti();
  }
}

void timer_send_command(uint8_t cmd, uint16_t data)
{
  if (data < 0x100) {
    outb(PIT_CMD, cmd | PIT_ACCESS_LO);
    outb(PIT_DATA0, data);
  }
  else if ((data & 0xff) == 0) {
    outb(PIT_CMD, cmd | PIT_ACCESS_HI);
    outb(PIT_DATA0, data >> 8);
  }
  else {
    outb(PIT_CMD, cmd | PIT_ACCESS_LOHI);
    outb(PIT_DATA0, data);
    outb(PIT_DATA0, data >> 8);
  }
}

void timer_set_divider(uint16_t d)
{
  timer_send_command(PIT_MODE_SQUARE_WAVE, d);
}

int timer_init(void)
{
  timer_set_divider(PIT_FREQ / 1000);
  timer.quantum = 25;
  return 0;
}

void timer_irq(isr_stack_t *stack)
{
  pic_eoi(0);
  timer.count++;

  if (timer_waiting_locked) return;
  barrier();

  /* wake sleeping tasks */
  while (waiting && waiting->timeout <= timer.count) {
    task_t *task = task_list_take(&waiting, waiting);
    task->state = TASK_RUNNING;
    task_list_add(&sched_runqueue, task);
  }

  /* run scheduler */
  sched_schedule(stack);
}

unsigned long timer_get_tick(void)
{
  return timer.count;
}

void timer_sleep(unsigned long delay)
{
  sched_disable_preemption();
  sched_current->state = TASK_WAITING;
  sched_current->timeout = timer.count + delay;

  timer_waiting_lock();
  /* insert in the list of waiting tasks in order of timeout */
  if (!waiting || sched_current->timeout <= waiting->timeout) {
    /* insert at the beginning */
    task_list_push(&waiting, sched_current);
  }
  else {
    /* insert before the first task with higher priority */
    task_t *task = waiting->next;
    while (task != waiting && task->timeout < sched_current->timeout) {
      task = task->next;
    }
    task_list_insert(task, sched_current);
  }
  timer_waiting_unlock();

  sched_yield();
}
