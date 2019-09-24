#ifndef SCHEDULER_H
#define SCHEDULER_H

struct isr_stack;

typedef void (*task_entry_t)(void);

void scheduler_schedule(isr_stack_t *stack);

void scheduler_spawn_task(task_entry_t entry);

#endif /* SCHEDULER_H */
