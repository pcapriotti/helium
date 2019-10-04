#ifndef SCHEDULER_H
#define SCHEDULER_H

struct isr_stack;

typedef void (*task_entry_t)(void);

typedef struct task {
  struct task *next;
  struct task *prev;

  unsigned int timeout;
  void *stack_top;
  struct isr_stack *stack;
  int state;
} task_t;

enum {
  TASK_RUNNING,
  TASK_STOPPED,
  TASK_WAITING,
  TASK_TERMINATED,
};

extern task_t *sched_current;
extern task_t *sched_runqueue;

void task_list_insert(task_t *list, task_t *task);
void task_list_add(task_t **list, task_t *task);
void task_list_push(task_t **list, task_t *task);
task_t *task_list_pop(task_t **list);
task_t *task_list_take(task_t **list, task_t *task);

void sched_schedule(struct isr_stack *stack);
void sched_spawn_task(task_entry_t entry);
void sched_yield(void);

void sched_disable_preemption();
void sched_enable_preemption();

#endif /* SCHEDULER_H */
