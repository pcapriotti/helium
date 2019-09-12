SECTIONS
{
  . = 0x7c00;
  .text :
  {
    src/boot.o
    . = 0x1fe;
    SHORT(0xaa55)
    src/stage1.o
    src/graphics.o
    ASSERT(. <= 0x2400, "Stage 1 should fit in the first track");
    _stage1_end = .;
    *(.text)
  }
  .data : { *(.data) }
  .bss : { *(.bss) }
}
