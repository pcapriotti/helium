#include "debug.h"
#include "v8086.h"
#include "interrupts.h"
#include "io.h"

#include <stddef.h>
#include <string.h>

int v8086_tracing = 0;

typedef struct {
  uint32_t eip, cs, eflags, sp, ss;
  uint32_t es, ds, fs, gs;
} __attribute__((packed)) v8086_stack_t;

uint32_t v8086_enter(regs16_t *regs, v8086_stack_t stack);
uint32_t v8086_exit(void *stack);

void v8086_debug_dump(const char *s, v8086_isr_stack_t *stack)
{
  debug_str("[v8086] ");
  debug_str(s);
  debug_str(" at ");
  debug_byte(stack->cs >> 8);
  debug_byte(stack->cs);
  debug_str(":");
  debug_byte(stack->eip >> 8);
  debug_byte(stack->eip);
  debug_str(" sp ");
  debug_byte(stack->ss >> 8);
  debug_byte(stack->ss);
  debug_str(":");
  debug_byte(stack->esp >> 8);
  debug_byte(stack->esp);
  debug_str("\n ");
  uint8_t *addr = (uint8_t *)seg_off_to_linear(stack->cs, stack->eip);
  for (int i = 0; i < 15; i++) {
    debug_str(" ");
    debug_byte(addr[i]);
  }
  uint16_t *st = (uint16_t *)seg_off_to_linear(stack->ss, stack->esp);
  debug_str("\n");
  debug_str("  stack:");
  for (int i = 0; i < 8; i++) {
    debug_str(" ");
    debug_byte(st[i] >> 8);
    debug_byte(st[i]);
  }
  debug_str("\n");
}

void v8086_gpf_handler(v8086_isr_stack_t *stack)
{
  uint8_t *addr = (uint8_t *)seg_off_to_linear(stack->cs, stack->eip);
  int op32 = 0;

  if (*addr == 0x66) {
    if (v8086_tracing) debug_str("[v8086] opsize\n");
    addr++;
    stack->eip++;
    op32 = 1;
  }
  else if (*addr == 0x67) {
    if (v8086_tracing) debug_str("[v8086] adsize\n");
    addr++;
    stack->eip++;
  }

  switch (*addr) {
  case 0x9c: /* pushf */
    if (v8086_tracing) v8086_debug_dump("pushf", stack);
    if (op32) {
      stack->esp -= 4;
      uint32_t *st = (uint32_t *) stack->esp;
      st[0] = stack->eflags & 0xdff;
      stack->eip += 1;
    }
    else {
      stack->esp -= 2;
      uint16_t *st = (uint16_t *) stack->esp;
      st[0] = stack->eflags & 0xffff;
      stack->eip += 1;
    }
    return;
  case 0x9d: /* popf */
    if (v8086_tracing) v8086_debug_dump("popf", stack);
    if (op32) {
      uint32_t *st = (uint32_t *)stack->esp;
      stack->esp += 4;
      stack->eflags = st[0] | EFLAGS_VM;
      stack->eip += 1;
    }
    else {
      uint16_t *st = (uint16_t *)stack->esp;
      stack->esp += 2;
      stack->eflags = st[0] | EFLAGS_VM;
      stack->eip += 1;
    }
    return;
  case 0xcf: /* iret */
    if (v8086_tracing) v8086_debug_dump("iret", stack);
    {
      uint16_t *st = (uint16_t *)stack->esp;

      /* iret in the stack guard */
      if (stack->eip == V8086_STACK_BASE + 4 &&
          stack->cs == 0) {
        v8086_exit(stack);
        return;
      }

      /* final iret from v8086 */
      if (stack->esp == V8086_STACK_BASE) {
        v8086_exit(stack);
        return;
      }

      stack->esp += 6;

      stack->eip = st[0];
      stack->cs = st[1];
      stack->eflags = (stack->eflags & 0xffff0000) | st[2];
      return;
    }
  case 0xfa: /* cli */
    if (v8086_tracing) v8086_debug_dump("cli", stack);
    stack->eip += 1;
    stack->eflags &= ~EFLAGS_IF;
    return;
  case 0xfb: /* sti */
    if (v8086_tracing) v8086_debug_dump("sti", stack);
    stack->eip += 1;
    stack->eflags |= EFLAGS_IF;
    return;
  case 0xcd: /* int */
    if (v8086_tracing) v8086_debug_dump("int", stack);
    {
      ptr16_t *handlers = 0;
      uint8_t int_num = addr[1];

      /* special case: copy extended memory */
      if (int_num == 0x15 && (stack->eax & 0xff00) == 0x8700) {
        stack->eip += 2;
        stack->eflags &= ~EFLAGS_CF;
        stack->eax = 0;

        uint8_t *p = (uint8_t *)seg_off_to_linear
          (stack->es, stack->esi);
        void *src = (void *) ((*(uint32_t *)(p + 0x12) & 0xffffff) |
                              (p[0x17] << 24));
        void *dst = (void *) ((*(uint32_t *)(p + 0x1a) & 0xffffff) |
                              (p[0x1f] << 24));
        size_t len = (stack->ecx & 0xffff) << 1;
        memcpy(dst, src, len);

        return;
      }

      ptr16_t iv = handlers[int_num];

      stack->esp -= 6;
      uint16_t *st = (uint16_t *)stack->esp;
      st[0] = stack->eip + 2;
      st[1] = stack->cs;
      st[2] = stack->eflags;

      stack->cs = iv.segment;
      stack->eip = iv.offset;
      return;
    }
  case 0xe6: /* outb imm */
    if (v8086_tracing) v8086_debug_dump("outb imm", stack);
    {
      uint8_t *port = (uint8_t *)stack->eip + 1;
      uint8_t value = stack->eax & 0xff;
      /* kprintf("outb %#x, %#x\n", (uint32_t) port, (uint32_t) value); */
      outb(*port, value);
      stack->eip += 2;
      return;
    }
  case 0xe7: /* outw/l imm */
    if (v8086_tracing) v8086_debug_dump("outw/l imm", stack);
    {
      uint8_t *port = (uint8_t *)stack->eip + 1;
      if (op32) {
        uint32_t value = stack->eax;
        /* kprintf("outl %#x, %#x\n", (uint32_t) port, value); */
        outl(*port, value);
      }
      else {
        uint16_t value = stack->eax & 0xffff;
        /* kprintf("outw %#x, %#x\n", (uint32_t) port, (uint32_t) value); */
        outw(*port, value);
      }
      stack->eip += 2;
      return;
    }
  case 0xee: /* outb */
    if (v8086_tracing) v8086_debug_dump("outb", stack);
    {
      uint16_t port = stack->edx & 0xffff;
      uint8_t value = stack->eax & 0xff;
      /* kprintf("outb %#x, %#x\n", (uint32_t) port, (uint32_t) value); */
      outb(port, value);
      stack->eip++;
      return;
    }
  case 0xef: /* outw */
    if (v8086_tracing) v8086_debug_dump("outw", stack);
    {
      uint16_t port = stack->edx & 0xffff;
      if (op32) {
        uint32_t value = stack->eax;
        outl(port, value);
      }
      else {
        uint16_t value = stack->eax & 0xffff;
        outw(port, value);
      }
      stack->eip++;
      return;
    }
  case 0xe4: /* inb imm */
    if (v8086_tracing) v8086_debug_dump("inb imm", stack);
    {
      uint8_t *port = (uint8_t *)stack->eip + 1;
      uint8_t value = inb(*port);
      /* kprintf("inb %#x\n", (uint32_t) (*port)); */
      stack->eax = (stack->eax & 0xffffff00) | value;
      stack->eip += 2;
      return;
    }
  case 0xe5: /* inw/l imm */
    if (v8086_tracing) v8086_debug_dump("inw/l imm", stack);
    {
      uint8_t *port = (uint8_t *)stack->eip + 1;
      /* kprintf("in%c %#x\n", op32 ? 'l' : 'w', (uint32_t) (*port)); */
      if (op32) {
        stack->eax = inl(*port);
      }
      else {
        uint16_t value = inw(*port);
        stack->eax = (stack->eax & 0xffff0000) | value;
      }
      stack->eip += 2;
      return;
    }
  case 0xec: /* inb */
    if (v8086_tracing) v8086_debug_dump("inb", stack);
    {
      uint16_t port = stack->edx & 0xffff;
      /* kprintf("inb %#x\n", (uint32_t) port); */
      uint8_t value = inb(port);
      stack->eax = (stack->eax & 0xffffff00) | value;
      stack->eip++;
      return;
    }
  case 0xed: /* inw */
    if (v8086_tracing) v8086_debug_dump("inw", stack);
    {
      uint16_t port = stack->edx & 0xffff;
      if (op32) {
        stack->eax = inl(port);
      }
      else {
        uint16_t value = inw(port);
        stack->eax = (stack->eax & 0xffff0000) | value;
      }
      stack->eip++;
      return;
    }
  default:
    debug_str("Unimplemented v8086 opcode ");
    debug_byte(*addr);
    debug_str("\n");
    panic();
  }
}

int v8086_manager(isr_stack_t *stack_)
{
  v8086_isr_stack_t *stack = (void *)stack_;

  if (!(stack->eflags & EFLAGS_VM)) return 0;

  switch (stack->int_num) {
  case IDT_GP:
    v8086_gpf_handler(stack);
    return 1;
  }

  if (stack->int_num >= IDT_IRQ) {
    int irq = stack->int_num - IDT_IRQ;

    /* let the BIOS handle this interrupt */
    stack->esp -= 6;
    uint16_t *st = (uint16_t *)stack->esp;
    st[0] = stack->eip;
    st[1] = stack->cs;
    st[2] = stack->eflags;

    int mapped = irq < 8 ? irq + 8 : irq + 0x68;
    struct { uint16_t off; uint16_t seg; } *iv = 0;
    stack->eip = iv[mapped].off;
    stack->cs = iv[mapped].seg;

    return 1;
  }

  return 0;
}

void v8086_interrupt_handler(isr_stack_t *stack)
{
  int done = v8086_manager(stack);
  if (!done) {
    debug_str("Unhandled exception: ");
    debug_byte(stack->int_num);
    debug_str("\n");

    debug_str("flags: ");
    debug_byte(stack->eflags >> 24);
    debug_byte(stack->eflags >> 16);
    debug_byte(stack->eflags >> 8);
    debug_byte(stack->eflags);
    debug_str(" eip: ");
    debug_byte(stack->eip >> 24);
    debug_byte(stack->eip >> 16);
    debug_byte(stack->eip >> 8);
    debug_byte(stack->eip);
    debug_str(" cs: ");
    debug_byte(stack->cs >> 8);
    debug_byte(stack->cs);
    debug_str("\n");
    panic();
  }
}


/* The stack parameter of v8086_enter must be initialised by the
caller to prevent undefined behaviour, so we have to use this little
wrapper that just sets up the stack and passes it by value to
v8086_enter */
uint32_t v8086_set_stack_and_enter(regs16_t *regs, ptr16_t entry)
{
  /* set up the stack for iret */
  v8086_stack_t stack;

  stack.es = regs->es;
  stack.ds = regs->ds;
  stack.fs = regs->fs;
  stack.gs = regs->gs;
  stack.eflags = EFLAGS_VM;
  stack.ss = 0;
  stack.sp = V8086_STACK_BASE;
  stack.eip = entry.offset;
  stack.cs = entry.segment;

  /* set up a stack guard, in case some BIOS code attempts to return
  with a far return instead of an iret */
  {
    uint16_t *st = (uint16_t *) V8086_STACK_BASE;
    st[0] = (uint32_t) (st + 2); /* eip = addr of iret instruction later */
    st[1] = 0; /* cs = 0 */
    st[2] = 0xcf; /* iret */
  }

  int paging_enabled = paging_disable();
  uint32_t flags = v8086_enter(regs, stack);
  if (paging_enabled) paging_enable();
  return flags;
}

void v8086_save_esp(uint32_t esp)
{
  kernel_tss.tss.esp0 = esp;
}

uint32_t v8086_restore_esp()
{
  return kernel_tss.tss.esp0;
}

uint32_t v8086_save_ctx(regs16_t *regs, v8086_isr_stack_t *ctx)
{
  regs->eax = ctx->eax;
  regs->ebx = ctx->ebx;
  regs->ecx = ctx->ecx;
  regs->edx = ctx->edx;
  regs->edi = ctx->edi;
  regs->ebp = ctx->ebp;
  regs->es = ctx->es;
  regs->ds = ctx->ds;
  regs->fs = ctx->fs;
  regs->gs = ctx->gs;

  return ctx->eflags;
}

int bios_int(uint32_t interrupt, regs16_t *regs)
{
  ptr16_t *handlers = 0; /* bios IVT */
  return v8086_set_stack_and_enter(regs, handlers[interrupt]);
}

void bios_shutdown(void)
{
  regs16_t regs;
  regs.eax = 0x5307;
  regs.ebx = 1;
  regs.ecx = 3;
  bios_int(0x15, &regs);
}
