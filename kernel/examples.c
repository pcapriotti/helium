#include "core/debug.h"

void task_a()
{
  while (1) {
    kprintf("A\n");
  }
}

void task_b()
{
  while (1) {
    kprintf("B\n");
  }
}
