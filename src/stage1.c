static volatile unsigned short *vga = (unsigned short *)0xb8000;

void _stage1()
{
  vga[0] = 'O' | 0xf00;
  vga[1] = 'k' | 0xf00;
  vga[2] = '.' | 0xf00;

  __asm__ volatile("hlt");
  while (1);
}
