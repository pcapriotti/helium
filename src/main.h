#ifndef MAIN_H
#define MAIN_H

extern uint8_t _stage0_start[];
extern uint8_t _stage1_end[];
extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

extern uint8_t *_low_mem_buffer;

void main(void);

#endif /* MAIN_H */
