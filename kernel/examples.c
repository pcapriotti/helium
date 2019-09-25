#include "core/debug.h"
#include "core/interrupts.h"
#include "console.h"
#include "scheduler.h"
#include "timer.h"

int sleep_condition(void *data)
{
  uint32_t *t1 = data;
  return timer_get_tick() >= *t1;
}

void sleep(unsigned long ticks)
{
  unsigned long t1 = timer_get_tick() + ticks;
  syscall_yield(IRQ_MASK(IRQ_TIMER),
                sleep_condition, &t1);
}

void task_a()
{
  unsigned long t0 = timer_get_tick();
  while (1) {
    kprintf("A %lu\n", timer_get_tick() - t0);
    sleep(3000);
  }
}

void task_b()
{
  unsigned long t0 = timer_get_tick();
  while (1) {
    kprintf("B %lu\n", timer_get_tick() - t0);
    sleep(5000);
  }
}
