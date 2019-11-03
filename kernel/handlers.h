#ifndef HANDLERS_H
#define HANDLERS_H

#include "list.h"

struct isr_stack;

typedef struct handler {
  list_t head;
  void (*handle)(struct isr_stack *stack);
} handler_t;

#define HANDLER_STATIC(name, handler) \
  static handler_t name = { \
    .handle = handler, \
  }

int irq_grab(int irq, handler_t *handler);
int irq_ungrab(int irq);
void handle_interrupt(struct isr_stack *stack);

#endif /* HANDLERS_H */
