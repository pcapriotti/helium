#include "core/debug.h"
#include "console.h"
#include "scheduler.h"
#include "timer.h"

void sleep(unsigned long ticks)
{
  unsigned long t1 = timer_get_tick() + ticks;
  while (timer_get_tick() < t1) {
    scheduler_yield();
  }
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
