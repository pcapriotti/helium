static volatile unsigned short *vga = (unsigned short *)0xb8000;

extern volatile unsigned char *vesa_framebuffer;
extern unsigned short vesa_pitch;

void _stage1()
{
  for (int j = 0; j < 10; j++) {
    for (int i = 0; i < 10; i++) {
      vesa_framebuffer[i + j * vesa_pitch] = 4;
    }
  }

  __asm__ volatile("hlt");
  while (1);
}
