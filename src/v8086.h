#ifndef V8086_H
#define V8086_H

void v8086_enter(uint32_t *tss_esp0, uint32_t *tss_eip);
void v8086_exit();
void v8086_restore_segments();
void v8086_int(uint16_t irq);
int vme_supported();

#endif /* V8086_H */
