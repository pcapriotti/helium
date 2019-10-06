.section .text

.globl kmain
kmain:
  /* set main kernel stack */
  mov $kernel_stack, %esp

  /* read arguments from multiboot */
  push %eax
  push %ebx
  call kernel_start
1: hlt
  jmp 1b

.section .multiboot

#define MB_MAGIC 0X1badb002
#define MB_FLAGS 0x3
#define MB_CHECKSUM -(MB_MAGIC + MB_FLAGS)

multiboot_header:
  .long MB_MAGIC
  .long MB_FLAGS
  .long MB_CHECKSUM

.section .bss

.balign 16
kernel_stack_base:
  .skip 0x1000
kernel_stack:
