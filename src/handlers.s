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

.macro isr0 number
  .globl isr\number
isr\number:
  pushl $0xff /* fake error code */
  pushl $\number
  isr_preamble
  push %esp
  call interrupt_handler
  add $0x4, %esp /* clean the stack */
  isr_epilogue 8
.endm

.macro isr1 number
  .globl isr\number
isr\number:
  pushl $\number
  isr_preamble
  push %esp
  call interrupt_handler
  add $0x4, %esp /* clean the stack */
  isr_epilogue 8
.endm

.macro irq number
  .globl irq\number
irq\number:
  isr_preamble
  pushw $\number
  call irq_handler
  add $0x2, %esp /* clean the stack */
  isr_epilogue 0
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
isr0 128

irq 0
irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15
