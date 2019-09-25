#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>

#define DEBUG_EAX(x) __asm__ volatile("" : : "a"(x))
#define DEBUG_REGS(a, b, c, d) __asm__ volatile("" : : "a"(a), "b"(b), "c"(c), "d"(d))

extern volatile uint16_t *vga_text;

void hang_system(void);

typedef struct {
  int x, y;
  volatile uint16_t *p;
} debug_console_t;
extern debug_console_t debug_console;

extern volatile int debug_key_pressed;
extern int debug_paging;

void panic(void);

void debug_str(const char *msg);
void debug_byte(uint8_t x);

int kprintf(const char *fmt, ...);

void (*print_char_function)(char c);
void (*flush_output_function)(void);

#endif /* DEBUG_H */
