#include "debug.h"
#include "v8086.h"
#include "interrupts.h"

int v8086_tracing = 0;

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
      ptr16_t iv = handlers[addr[1]];

      stack->esp -= 6;
      uint16_t *st = (uint16_t *)stack->esp;
      st[0] = stack->eip + 2;
      st[1] = stack->cs;
      st[2] = stack->eflags;

      stack->cs = iv.segment;
      stack->eip = iv.offset;
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

typedef struct {
  uint32_t eip, cs, eflags, sp, ss;
  uint32_t es, ds, fs, gs;
} __attribute__((packed)) v8086_stack_t;

/* The following function enters virtual-8086 mode with the given 16
bit register configuration. It works by taking a stack parameter,
populating it with values that would be pushed before an interrupt
from v8086 to protected mode, and issuing an iret instruction, which
will jump to the given entry point. */
uint32_t v8086_enter(regs16_t *regs, v8086_stack_t stack)
{
  v8086_isr_stack_t *ctx;
  uint32_t eflags;

  /* set up a stack guard, in case some BIOS code attempts to return
  with a far return instead of an iret */
  {
    uint16_t *st = (uint16_t *) V8086_STACK_BASE;
    st[0] = (uint32_t) (st + 2); /* eip = addr of iret instruction later */
    st[1] = 0; /* cs = 0 */
    st[2] = 0xcf; /* iret */
  }

  /* save flags */
  __asm__ volatile
    (
     "pushf\n"
     "pop %0\n"
     : "=r"(eflags));

  __asm__ volatile
    (/* save registers used here */
     "push %3\n"

     /* save current stack pointer in tss */
     "mov %%esp, %0\n"

     /* adjust the stack */
     "mov %2, %%esp\n"

     /* /\* set up registers *\/ */
     "mov 0x0(%3), %%eax\n"
     "mov 0x4(%3), %%ebx\n"
     "mov 0xc(%3), %%edx\n"
     "mov 0x10(%3), %%edi\n"
     "mov 0x14(%3), %%ebp\n"
     "mov 0x8(%3), %%ecx\n" /* this has to be last */

     /* jump to v8086 mode */
     "iret\n"

     /* control will return here from the v8086 #GP handler after the
     v8086 call stack has underflowed */
     "v8086_exit:\n"

     /* save context */
     "mov 4(%%esp), %1\n"

     /* restore stack */
     "mov %0, %%esp\n"

     /* restore registers */
     "pop %3\n"

     : "=m"(kernel_tss.tss.esp0), "=a"(ctx)
     : "a"(&stack), "c"(regs)
     : "%ebx", "%edx", "%edi", "%ebp", "%esi");

  /* update regs structure */
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

  /* restore flags */
  __asm__ volatile
    ("push %0\n"
     "popf\n"
     :
     : "r"(eflags));

  return ctx->eflags;
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

  return v8086_enter(regs, stack);
}

int bios_int(uint32_t interrupt, regs16_t *regs)
{
  ptr16_t *handlers = 0; /* bios IVT */
  return v8086_set_stack_and_enter(regs, handlers[interrupt]);
}
