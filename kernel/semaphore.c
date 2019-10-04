#include "core/debug.h"
#include "scheduler.h"
#include "semaphore.h"

void spin_lock(spin_lock_t *lock)
{
  sched_disable_preemption();
}

void spin_unlock(spin_lock_t *lock)
{
  sched_enable_preemption();
}

void sem_init(semaphore_t *sem, int value)
{
  sem->value = value;
  sem->waiting = 0;
}

void sem_wait(semaphore_t *sem)
{
  spin_lock(&sem->lock);

  if (--sem->value < 0) {
    serial_printf("sem (%p): sleeping\n", sched_current);
    sched_current->state = TASK_WAITING;
    task_list_add(&sem->waiting, sched_current);
    spin_unlock(&sem->lock);
    sched_disable_preemption();
    sched_yield();
  }
  else {
    spin_unlock(&sem->lock);
  }
}

void sem_signal(semaphore_t *sem)
{
  spin_lock(&sem->lock);

  if (sem->value++ < 0 && sem->waiting) {
    serial_printf("sem (%p): waking %p\n", sched_current, sem->waiting);
    task_t *task = task_list_pop(&sem->waiting);
    task->state = TASK_RUNNING;
    task_list_add(&sched_runqueue, task);
  }

  spin_unlock(&sem->lock);
}
