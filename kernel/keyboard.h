#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

typedef enum {
  /* numbers */
  KC_0, KC_1, KC_2, KC_3, KC_4, KC_5, KC_6, KC_7, KC_8, KC_9,
  /* letters */
  KC_A, KC_B, KC_C, KC_D, KC_E, KC_F, KC_G, KC_H, KC_I, KC_J,
  KC_K, KC_L, KC_M, KC_N, KC_O, KC_P, KC_Q, KC_R, KC_S, KC_T,
  KC_U, KC_V, KC_W, KC_X, KC_Y, KC_Z,
  /* symbols */
  KC_CMM, KC_PRD, KC_SLH, KC_BSL, KC_OBR, KC_CBR, KC_SEM, KC_AST,
  KC_DSH, KC_PLS, KC_EQL, KC_SP, KC_QT, KC_BQT, KC_ENT,
  /* function keys */
  KC_F0, KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6,
  KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12,
  /* movements */
  KC_LFT, KC_RHT, KC_UP, KC_DWN, KC_PGU, KC_PGD, KC_HOM, KC_END,
  /* modifiers */
  KC_ALT, KC_CTR, KC_LSH, KC_RSH,
  /* special */
  KC_NO, KC_CAP, KC_NUM, KC_SCR, KC_INS, KC_DEL, KC_TAB, KC_ESC, KC_BSP
} kb_keycode_t;

int kb_init(void);
void kb_irq(void);

typedef struct {
  int pressed;
  kb_keycode_t keycode;
  uint8_t mods;
  uint8_t scancode;
  char printable;
  unsigned long timestamp;
} kb_event_t;

/* circular array */
typedef struct {
  unsigned int size;
  unsigned int start;
  unsigned int end;
  void *items;
} queue_t;

extern queue_t kb_events;

/* add a process to the keyboard waiting queue */
void kb_grab(void (*on_event)(kb_event_t *event, void *data), void *data);

#endif /* KEYBOARD_H */
