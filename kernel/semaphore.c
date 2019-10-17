#include "core/debug.h"
#include "scheduler.h"
#include "semaphore.h"

#define SEMAPHORE_DEBUG 0
#if SEMAPHORE_DEBUG
#define TRACE(...) serial_printf(__VA_ARGS__)
#else
#define TRACE(...) do {} while(0)
#endif

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
    TRACE("sem (%p): sleeping\n", sched_current);
    sched_current->state = TASK_WAITING;
    list_add(&sem->waiting, &sched_current->head);
    spin_unlock(&sem->lock);
    sched_disable_preemption();
    sched_yield();
  }
  else {
    spin_unlock(&sem->lock);
  }
}

void _sem_signal(semaphore_t *sem)
{
  if (sem->value++ < 0 && sem->waiting) {
    TRACE("sem (%p): waking %p\n", sched_current, sem->waiting);
    task_t *task = TASK_LIST_ENTRY(list_pop(&sem->waiting));
    task->state = TASK_RUNNING;
    list_add(&sched_runqueue, &task->head);
  }
}

void sem_signal(semaphore_t *sem)
{
  spin_lock(&sem->lock);
  _sem_signal(sem);
  spin_unlock(&sem->lock);
}
