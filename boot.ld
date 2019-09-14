SECTIONS
{
  . = 0x7c00;
  .text0 :
  {
    src/boot.o
    . = 0x1fe;
    SHORT(0xaa55)
    src/stage1.o
    *(.text1)
    ASSERT(. <= 0x2400, "Stage 1 should fit in the first track");
    _stage1_end = .;
  }

  . = 0x100000;
  .text : AT((_stage1_end + 511) & ~511)
  {
    *(.text)
    _kernel_end = .;
  }

  .data : { *(.data) }
  .bss : { *(.bss) }
}
