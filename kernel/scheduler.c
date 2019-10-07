#include "atomic.h"
#include "core/debug.h"
#include "core/gdt.h"
#include "core/interrupts.h"
#include "core/v8086.h"
#include "kmalloc.h"
#include "memory.h"
#include "scheduler.h"
#include "timer.h"

#include <assert.h>

#define SCHED_DEBUG 0
#if SCHED_DEBUG
#define TRACE(...) serial_printf(__VA_ARGS__)
#else
#define TRACE(...) do {} while(0)
#endif

#define SCHED_QUANTUM 20

list_t *sched_runqueue = 0;
task_t *sched_current = 0;

/* when this is set the current task cannot be preempted, and it has
exclusive access to scheduler data structures */
volatile int sched_locked = 1;

static void context_switch(isr_stack_t *stack)
{
  __asm__ volatile
    ("mov %0, %%esp\n"
     "popa\n"
     "add $8, %%esp\n"
     "iret\n"
     : : "m"(stack));
}

void task_list_insert(list_t *list, task_t *task)
{
  list_insert(list, &task->head);
}

/* add at the end */
void task_list_add(list_t **list, task_t *task)
{
  list_add(list, &task->head);
}

/* add at the front */
void task_list_push(list_t **list, task_t *task)
{
  list_push(list, &task->head);
}

/* remove from the front */
task_t *task_list_pop(list_t **list)
{
  return TASK_LIST_ENTRY(list_pop(list));
}

task_t *task_list_take(list_t **list, task_t *task)
{
  return TASK_LIST_ENTRY(list_take(list, &task->head));
}

void sched_schedule(isr_stack_t *stack)
{
  /* no task switch if the scheduler is locked */
  if (sched_locked) return;
  barrier();

  /* TODO: take the lock and reenable interrupts */

  unsigned long ticks = timer_get_tick();

  /* do nothing if the task still has time left */
  if (sched_current &&
      sched_current->state == TASK_RUNNING &&
      sched_current->timeout > ticks) return;

  /* put task back into runqueue */
  if (sched_current) {
    sched_current->stack = stack;
    if (sched_current->state == TASK_RUNNING) {
      task_list_add(&sched_runqueue, sched_current);
    }
    else if (sched_current->state == TASK_TERMINATED) {
      ffree(sched_current->stack_top);
    }
  }

  /* update current task */
  task_t *previous = sched_current;
  sched_current = task_list_pop(&sched_runqueue);

  /* same task, no switch necessary */
  if (sched_current == previous) return;

  if (previous || sched_current) {
    TRACE("switch %p => %p\n", previous, sched_current);
  }

  if (!sched_current) {
    /* no more tasks, idle */
    sti();
    hang_system();
    return;
  }

  /* do context switch */
  sched_current->timeout = ticks + SCHED_QUANTUM;
  context_switch(sched_current->stack);
}

void task_terminate()
{
  TRACE("task %p terminating\n", sched_current);
  sched_disable_preemption();
  sched_current->state = TASK_TERMINATED;
  sched_yield();
}

void sched_spawn_task(task_entry_t entry)
{
  sched_disable_preemption();

  /* allocate memory for the task */
  task_t *task = kmalloc(sizeof(task_t));
  task->stack_top = falloc(0x4000);

  void *stack = task->stack_top + 0x4000;
  /* add stack guard */
  stack -= sizeof(void *);
  {
    void **guard = (void **)stack;
    *guard = task_terminate;
  }

  stack -= sizeof(isr_stack_t);
  task->stack = stack;
  task->state = TASK_RUNNING;
  task->timeout = timer_get_tick(); /* start immediately */

  /* TODO set up a stack guard for when the task terminates */
  task->stack->eip = (uint32_t) entry;
  task->stack->cs = GDT_SEL(GDT_CODE);
  task->stack->eflags = EFLAGS_IF;

  task_list_add(&sched_runqueue, task);
  TRACE("spawned %p\n", task);

  sched_enable_preemption();
}

void sched_enable_preemption(void)
{
  assert(sched_locked);
  cli();
  sched_locked--;
  sti();
}

void sched_disable_preemption(void)
{
  cli();
  sched_locked++;
  sti();
}

/* only call this function while preemption is disabled */
void sched_yield(void)
{
  assert(sched_locked == 1);
  cli();
  sched_locked = 0;
  TRACE("%p yield (state = %u)\n", sched_current, sched_current ? sched_current->state : 0);
  syscall_yield();
  sti();
}
