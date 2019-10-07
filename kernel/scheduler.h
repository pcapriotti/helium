#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "list.h"

struct isr_stack;

typedef void (*task_entry_t)(void);

typedef struct task {
  list_t head;

  unsigned int timeout;
  void *stack_top;
  struct isr_stack *stack;
  int state;
} task_t;

#define TASK_LIST_ENTRY(item) LIST_ENTRY(item, task_t, head)

enum {
  TASK_RUNNING,
  TASK_STOPPED,
  TASK_WAITING,
  TASK_TERMINATED,
};

extern task_t *sched_current;
extern list_t *sched_runqueue;

void task_list_insert(list_t *list, task_t *task);
void task_list_add(list_t **list, task_t *task);
void task_list_push(list_t **list, task_t *task);
task_t *task_list_pop(list_t **list);
task_t *task_list_take(list_t **list, task_t *task);

void sched_schedule(struct isr_stack *stack);
void sched_spawn_task(task_entry_t entry);
void sched_yield(void);

void sched_disable_preemption();
void sched_enable_preemption();

#endif /* SCHEDULER_H */
