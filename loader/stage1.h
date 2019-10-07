#ifndef STAGE1_H
#define STAGE1_H

#include <stdint.h>

#define LOADER_MAGIC 0x1fc9b736

extern uint8_t _loader0_start[];
extern uint8_t _loader0_end[];
extern uint8_t _loader_start[];
extern uint8_t _loader_end[];
extern uint8_t _kernel_start[];
extern uint8_t _bss_start[];
extern uint8_t _bss_end[];

#endif /* STAGE1_H */
