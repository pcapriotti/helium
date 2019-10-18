#ifndef HANDLERS_H
#define HANDLERS_H

struct isr_stack;

typedef void (*handler_t)(struct isr_stack *stack);

int irq_grab(int irq, handler_t handler);
int irq_ungrab(int irq);
void handle_interrupt(struct isr_stack *stack);

#endif /* HANDLERS_H */
