#include "keyboard.h"
#include "core/debug.h"
#include "core/io.h"

#include <stddef.h>

enum {
  PS2_DATA = 0x60,
};

/* modifier bit offsets */
enum {
  MOD_SHIFT,
  MOD_CTRL,
  MOD_ALT,
};

#define MOD_MASK(mod) (1 << (mod))

kb_keycode_t kb_us_layout[128] =
{
  KC_NO,  KC_ESC, KC_1,   KC_2,   KC_3,   KC_4,   KC_5,   KC_6,   KC_7,   KC_8,
  KC_9,   KC_0,   KC_DSH, KC_EQL, KC_BSP, KC_TAB, KC_Q,   KC_W,   KC_E,   KC_R,
  KC_T,   KC_Y,   KC_U,   KC_I,   KC_O,   KC_P,   KC_OBR, KC_CBR, KC_ENT, KC_CTR,
  KC_A,   KC_S,   KC_D,   KC_F,   KC_G,   KC_H,   KC_J,   KC_K,   KC_L,   KC_SEM,
  KC_QT,  KC_BQT, KC_LSH, KC_BSL, KC_Z,   KC_X,   KC_C,   KC_V,   KC_B,   KC_N,
  KC_M,   KC_CMM, KC_PRD, KC_SLH, KC_RSH, KC_AST, KC_ALT, KC_SP,  KC_CAP, KC_F1,
  KC_F2,  KC_F3,  KC_F4,  KC_F5,  KC_F6,  KC_F7,  KC_F8,  KC_F9,  KC_F10, KC_NUM,
  KC_SCR, KC_HOM, KC_UP,  KC_PGU, KC_DSH, KC_LFT, KC_NO,  KC_RHT, KC_PLS, KC_END,
  KC_DWN, KC_PGD, KC_INS, KC_DEL, KC_NO,  KC_NO,  KC_NO,  KC_F11, KC_F12, KC_NO,
  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,
  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO,  KC_NO
};

static char kb_printable_0[] = "0123456789abcdefghijklmnopqrstuvwxyz,./\\[];*-+= '`\n";
static char kb_printable_1[] = ")!\"#$%^&*(ABCDEFGHIJKLMNOPQRSTUVWXYZ<>?|{}:*_++ \"~\n";

uint8_t kb_mods = 0;

#define EVENT_BUFFER_SIZE 512
kb_event_t kb_event_buffer[EVENT_BUFFER_SIZE];
kb_event_queue_t kb_events = {
  .size = EVENT_BUFFER_SIZE,
  .events = kb_event_buffer,
  .start = 0,
  .end = 0,
};

#define MOD_SET(pressed, mod) \
  kb_mods = (kb_mods & ~MOD_MASK(mod)) | ((pressed) << (mod));

char keycode_to_char(kb_keycode_t kc)
{
  if (kc <= KC_ENT) {
    if (kb_mods & MOD_MASK(MOD_SHIFT)) {
      return kb_printable_1[kc];
    } else {
      return kb_printable_0[kc];
    }
  }

  return 0;
}

void kb_add_event(uint8_t scancode) {
  kb_keycode_t kc = kb_us_layout[scancode & 0x7f];
  unsigned int pressed = (scancode & 0x80) == 0;

  /* update modifiers */
  if (kc == KC_LSH || kc == KC_RSH) {
    MOD_SET(pressed, MOD_SHIFT);
    return;
  }
  if (kc == KC_CTR) {
    MOD_SET(pressed, MOD_CTRL);
    return;
  }
  if (kc == KC_ALT) {
    MOD_SET(pressed, MOD_ALT);
    return;
  }

  /* add event to queue */
  kb_event_t *event = &kb_event_buffer[kb_events.end];
  event->pressed = pressed;
  event->keycode = kc;
  event->printable = keycode_to_char(kc);
  event->mods = kb_mods;
  kb_events.end = (kb_events.end + 1) % kb_events.size;
}

void kb_irq(void)
{
  kb_add_event(inb(PS2_DATA));
}
