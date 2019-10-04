#ifndef V8086_H
#define V8086_H

#define V8086_STACK_BASE 0x2000
#define V8086_HEAP 0x2800

typedef struct {
  uint32_t eax, ebx, ecx, edx, edi, ebp;
  uint16_t es, ds, fs, gs;
} __attribute__((packed)) regs16_t;

int bios_int(uint32_t interrupt, regs16_t *regs);
void v8086_exit(void *stack);

typedef struct {
  uint16_t offset;
  uint16_t segment;
} __attribute__((packed)) ptr16_t;

static inline uint32_t seg_off_to_linear(uint16_t seg, uint16_t off)
{
  return (seg << 4) + off;
}

static inline uint32_t ptr16_to_linear(ptr16_t ptr)
{
  return seg_off_to_linear(ptr.segment, ptr.offset);
}

static inline void linear_to_seg_off(uint32_t p, uint16_t *seg, uint16_t *off)
{
  *seg = (p >> 4) & 0xf000;
  *off = p;
}

static inline ptr16_t linear_to_ptr16(uint32_t p)
{
  ptr16_t p16;
  linear_to_seg_off(p, &p16.segment, &p16.offset);
  return p16;
}

enum {
  EFLAGS_CF = 1 << 0,
  EFLAGS_IF = 1 << 9,
  EFLAGS_VM = 1 << 17,
};

struct isr_stack;
int v8086_manager(struct isr_stack *stack_);

#endif /* V8086_H */
