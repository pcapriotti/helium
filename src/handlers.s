.section .text1

.macro isr_preamble
  cli
  pusha
  push %ds
  push %es
  push %fs
  push %gs
.endm

.macro isr_epilogue slots
  pop %gs
  pop %fs
  pop %es
  pop %ds
  popa
  add $\slots, %esp
  iret
.endm

isr_generic:
  isr_preamble
  call interrupt_handler
  isr_epilogue 8

/* Make sure that all isr stubs have the same size.
  This way I can set them all in the IDT with a simple loop */

.macro isr0 number
  .globl isr\number
isr\number:
  sub $4, %esp
  pushl $\number
  mov $isr_generic, %eax
  jmp *%eax
.endm

.macro isr1 number
  .globl isr\number
isr\number:
  /* fake push so that all isrs have the same code size */
  sub $0, %esp
  pushl $\number
  mov $isr_generic, %eax
  jmp *%eax
.endm

isr0 0
isr0 1
isr0 2
isr0 3
isr0 4
isr0 5
isr0 6
isr0 7
isr1 8
isr0 9
isr1 10
isr1 11
isr1 12
isr1 13
isr1 14
isr0 15
isr0 16
isr1 17
isr0 18
isr0 19

isr0 32
isr0 33
isr0 34
isr0 35
isr0 36
isr0 37
isr0 38
isr0 39
isr0 40
isr0 41
isr0 42
isr0 43
isr0 44
isr0 45
isr0 46
isr0 47
