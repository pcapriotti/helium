#ifndef CONSOLE_FBCON_H
#define CONSOLE_FBCON_H

#include <stdint.h>

typedef struct fbcon {
  uint32_t *fb;
  int needs_repaint;
  int pitch;

  semaphore_t paint_mutex;
  semaphore_t paint_sem;
} fbcon_t;

int fbcon_init(fbcon_t *fbcon);
fbcon_t *fbcon_get(void);
console_backend_t *fbcon_backend_get(void);

#endif /* CONSOLE_FBCON_H */
