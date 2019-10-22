#ifndef STACKTRACE_H
#define STACKTRACE_H

typedef struct stacktrace {
  struct stacktrace *next;
  void *eip;
} __attribute__((packed)) stacktrace_t;

#define stacktrace_head() ({ \
  stacktrace_t ret; \
  __asm__ volatile \
    ("mov %%ebp, %0\n" \
     "call 1f\n" \
     "1: pop %1\n" \
     : "=r"(ret.next), "=r"(ret.eip)); \
  ret; })

static inline void stacktrace_print(stacktrace_t *st)
{
  int i = 0;
  while (st->next) {
    serial_printf("%d: %p stack: %p\n", i, st->eip, st);
    st = st->next;
    i++;
  }
}

#define stacktrace_print_current() do { \
  stacktrace_t st = stacktrace_head(); \
  stacktrace_print(&st); \
  } while(0)

#endif /* STACKTRACE_H */
