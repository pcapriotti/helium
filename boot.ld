SECTIONS
{
  . = 0x7c00;
  _stage0_start = .;
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

  . = 0x100000;
  .text : AT(_stage1_end)
  {
    _kernel_start = .;
    *(.text)
    _kernel_end = .;
  }

  .data : { *(.data) }
  .bss : { *(.bss) }
}
