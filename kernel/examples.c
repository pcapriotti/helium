#include "core/debug.h"
#include "core/interrupts.h"
#include "console.h"
#include "scheduler.h"
#include "timer.h"

void sleep(unsigned long ticks)
{
  syscall_yield(IRQ_MASK(IRQ_TIMER), 0, 0, ticks);
}

void task_a()
{
  unsigned long t0 = timer_get_tick();
  while (1) {
    kprintf("A %lu\n", timer_get_tick() - t0);
    sleep(2000);
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
