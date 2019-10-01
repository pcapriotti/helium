#include "core/debug.h"
#include "core/interrupts.h"
#include "console.h"
#include "keyboard.h"
#include "scheduler.h"
#include "timer.h"

void task_a()
{
  unsigned long t0 = timer_get_tick();
  while(1) {
    kprintf("A %lu\n", timer_get_tick() - t0);
    timer_sleep(2000);
  }
}

void task_b()
{
  unsigned long t0 = timer_get_tick();
  while (1) {
    kprintf("B %lu\n", timer_get_tick() - t0);
    timer_sleep(5000);
  }
}

void task_kb()
{
  while (1) {
    kb_wait();
    /* TODO: synchronise */
    while (kb_events.start != kb_events.end) {
      kb_event_t *event = &((kb_event_t *)kb_events.items)[kb_events.start];
      if (event->pressed && event->printable) {
        kprintf("%c", event->printable);
      }
      kb_events.start = (kb_events.start + 1) % kb_events.size;
    }
  }
}
