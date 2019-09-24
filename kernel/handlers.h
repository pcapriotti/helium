#ifndef HANDLERS_H
#define HANDLERS_H

struct isr_stack;

void handle_interrupt(struct isr_stack *stack);

#endif /* HANDLERS_H */
