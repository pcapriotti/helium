#ifndef V8086_H
#define V8086_H

void enter_v8086_mode(void *tss_esp0);
int vme_supported();

#endif /* V8086_H */
