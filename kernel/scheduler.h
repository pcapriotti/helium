#ifndef SCHEDULER_H
#define SCHEDULER_H

struct isr_stack;

typedef void (*task_entry_t)(void);

void scheduler_schedule(struct isr_stack *stack);

void scheduler_spawn_task(task_entry_t entry);

void scheduler_yield(struct isr_stack *stack);

#endif /* SCHEDULER_H */
