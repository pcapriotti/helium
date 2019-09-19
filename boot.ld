SECTIONS
{

  _stack_base = 0x7b00;
  . = 0x7c00;
  _kernel_low_start = .;
  .text0 :
  {
    src/boot.o
    . = 0x1fe;
    SHORT(0xaa55)
    src/stage1.o
    *(.text1)
    _stage1_end = ALIGN(512);
    ASSERT(_stage1_end <= 0x2400, "Stage 1 should fit in the first track");
  }

  /* put bss in low memory, so that it can be used by stage 1 code */
  . = _stage1_end;
  .bss : { *(.bss) }

  _kernel_low_end = .;

  /* the kernel goes in high memory */
  . = 0x100000;
  .text : AT(_stage1_end)
  {
    _kernel_start = .;
    *(.text) *(.text.*)
  }

  .data : { *(.data) *(.rodata) }
  _kernel_end = .;

}
