#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#ifndef SMP
typedef int spin_lock_t[0];
#endif

void spin_lock(spin_lock_t *lock);
void spin_unlock(spin_lock_t *lock);

struct task;

typedef struct semaphore {
  spin_lock_t lock;
  int value;
  struct task *waiting;
} semaphore_t;

#define SEM_INIT(val) ((semaphore_t) { SPIN_LOCK_INIT, (val), 0 })

void sem_init(semaphore_t *sem, int value);
void sem_wait(semaphore_t *sem);
void sem_signal(semaphore_t *sem);

#endif /* SEMAPHORE_H */
