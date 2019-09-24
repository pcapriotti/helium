#include "core/debug.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/v8086.h"
#include "list.h"
#include "kmalloc.h"
#include "memory.h"
#include "scheduler.h"

#define SCHEDULER_QUANTUM 20

typedef struct {
  list_t head;
  unsigned int ticks; /* remaining ticks */
  void *stack_top;
  isr_stack_t *stack;
  int state;
} task_t;

enum {
  TASK_RUNNING,
  TASK_STOPPED,
  TASK_WAITING
};

static list_t tasks = LIST_INIT(tasks);

task_t *current = 0;

static void context_switch(isr_stack_t *stack)
{
  __asm__ volatile
    ("mov %0, %%esp\n"
     "popa\n"
     "add $8, %%esp\n"
     "iret\n"
     : : "m"(stack));
}

void scheduler_schedule(isr_stack_t *stack)
{
  if (current && current->ticks > 0) {
    current->ticks--;
    return;
  }

  task_t *previous = current;
  if (current) {
    if (current->head.next == &tasks) {
      current = list_entry(tasks.next, task_t, head);
    }
    else {
      current = list_next(current, head);
    }
  }
  else {
    if (list_empty(&tasks)) return;
    current = list_entry(tasks.next, task_t, head);
  }
  current->ticks = SCHEDULER_QUANTUM;

  /* do context switch */
  if (current != previous) {
    if (previous) previous->stack = stack;
    context_switch(current->stack);
  }
}

void scheduler_spawn_task(task_entry_t entry)
{
  /* allocate a stack */
  task_t *task = kmalloc(sizeof(task_t));
  task->stack_top = falloc(0x4000);
  task->stack = task->stack_top + 0x4000 - sizeof(isr_stack_t);
  task->state = TASK_RUNNING;
  task->ticks = 0;

  task->stack->eip = (uint32_t) entry;
  task->stack->cs = GDT_SEL(GDT_CODE);
  task->stack->eflags = EFLAGS_IF;

  /* add it to the task list */
  list_add(&task->head, &tasks);
}
