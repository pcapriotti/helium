#include "core/debug.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/v8086.h"
#include "kmalloc.h"
#include "memory.h"
#include "scheduler.h"
#include "timer.h"

#include <assert.h>

#define SCHEDULER_QUANTUM 20

typedef struct task {
  struct task *next;
  struct task *prev;

  unsigned int timeout;
  void *stack_top;
  isr_stack_t *stack;
  int state;
} task_t;

enum {
  TASK_RUNNING,
  TASK_STOPPED,
  TASK_WAITING
};

/* currently running tasks */
static task_t *current = 0;
/* tasks waiting on an event */
static task_t *waiting = 0;

/* when this is set the current task cannot be preempted, and it has
exclusive access to scheduler data structures */
volatile int scheduler_lock = 0;

static void context_switch(isr_stack_t *stack)
{
  __asm__ volatile
    ("mov %0, %%esp\n"
     "popa\n"
     "add $8, %%esp\n"
     "iret\n"
     : : "m"(stack));
}

void scheduler_yield(isr_stack_t *stack)
{
  assert(scheduler_lock);
  current->timeout = timer_get_tick();

  unsigned long irqmask = stack->ebx;
  wait_condition_t cond = (wait_condition_t) stack->ecx;
  void *data = (void *) stack->edx;

  /* todo: move to waiting queue */
  scheduler_schedule(stack);
}

/* add at the end */
void task_list_add(task_t **list, task_t *task)
{
  if (*list) {
    task_t *prev = (*list)->prev;
    prev->next = task;
    task->prev = prev;

    (*list)->prev = task;
    task->next = *list;
  }
  else {
    task->next = task;
    task->prev = task;
    *list = task;
  }
}

/* add at the front */
void task_list_push(task_t **list, task_t *task)
{
  task_list_add(list, task);
  *list = task;
}

task_t *task_list_take(task_t **list, task_t *task)
{
  if (task->next == task) {
    *list = 0;
    return task;
  }

  task_t *prev = task->prev;
  task_t *next = task->next;
  prev->next = next;
  next->prev = prev;

  if (task == *list) {
    *list = next;
  }
  return task;
}

void scheduler_schedule(isr_stack_t *stack)
{
  unsigned long ticks = timer_get_tick();
  task_t *previous = current;

  /* do nothing if the task still has time left */
  if (previous && previous->timeout > ticks) return;

  /* no task switch if the scheduler is locked */
  if (scheduler_lock) return;

  /* start waiting tasks */
  while (waiting && waiting->timeout <= ticks) {
    task_t *task = task_list_take(&waiting, waiting);
    task_list_add(&current, task);
    kprintf("adding new task %p\n", task);
  }

  /* no tasks */
  if (!current) return;

  /* update current task */
  if (previous) current = previous->next;

  /* no need to switch if the task has not changed */
  if (previous == current) return;

  /* save current stack for previous task */
  if (previous) previous->stack = stack;

  /* do context switch */
  current->timeout = ticks + SCHEDULER_QUANTUM;
  context_switch(current->stack);
}

void scheduler_spawn_task(task_entry_t entry)
{
  scheduler_lock = 1;

  /* allocate a stack */
  task_t *task = kmalloc(sizeof(task_t));
  task->stack_top = falloc(0x4000);
  task->stack = task->stack_top + 0x4000 - sizeof(isr_stack_t);
  task->state = TASK_RUNNING;
  task->timeout = timer_get_tick(); /* start immediately */

  task->stack->eip = (uint32_t) entry;
  task->stack->cs = GDT_SEL(GDT_CODE);
  task->stack->eflags = EFLAGS_IF;

  kprintf("creating new task %p\n", task);
  task_list_push(&waiting, task);

  scheduler_lock = 0;
}
