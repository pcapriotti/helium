#include "core/interrupts.h"
#include "list.h"
#include "kmalloc.h"
#include "memory.h"
#include "scheduler.h"

#define SCHEDULER_QUANTUM 20

typedef struct {
  saved_regs_t regs;
  uint32_t eip, eflags, esp;
} cpu_state_t;

typedef struct {
  list_t head;
  unsigned int ticks; /* remaining ticks */
  cpu_state_t cpu_state;
  int state;
} task_t;

enum {
  TASK_RUNNING,
  TASK_STOPPED,
  TASK_WAITING
};

static list_t tasks = LIST_INIT(tasks);

task_t *current = 0;

void save_cpu_state(cpu_state_t *cpu, isr_stack_t *stack)
{
  cpu->regs = stack->regs;
  cpu->eip = stack->eip;
  cpu->eflags = stack->eflags;
  cpu->esp = stack->esp;
}

void restore_cpu_state(cpu_state_t *cpu, isr_stack_t *stack)
{
  stack->regs = cpu->regs;
  stack->eip = cpu->eip;
  stack->eflags = cpu->eflags;
  stack->esp = cpu->esp;
}

void scheduler_irq(isr_stack_t *stack)
{
  if (!current) current = list_first(tasks, task_t, head);
  if (!current) return; /* no tasks */

  if (current->ticks == 0) {
    /* interrupt task */
    save_cpu_state(&current->cpu_state, stack);
    /* switch to next task */
    current = list_next(current, head);
    restore_cpu_state(&current->cpu_state, stack);
  }
  else {
    current->ticks--;
  }
}

void scheduler_spawn_task(task_entry_t entry)
{
  /* allocate a stack */
  void *stack = falloc(0x4000);

  /* create task structure */
  task_t *task = kmalloc(sizeof(task_t));
  task->cpu_state.eip = (uint32_t) entry;
  task->cpu_state.eflags = 0;
  task->cpu_state.esp = (uint32_t) stack;
  task->ticks = SCHEDULER_QUANTUM;
  task->state = TASK_RUNNING;

  /* add it to the task list */
  list_add(&task->head, &tasks);
}
