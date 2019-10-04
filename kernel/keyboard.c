#include "atomic.h"
#include "keyboard.h"
#include "core/gdt.h"
#include "core/debug.h"
#include "core/interrupts.h"
#include "core/io.h"
#include "core/v8086.h"
#include "scheduler.h"
#include "timer.h"

#include <stddef.h>

#define KB_TASKLET 1

static uint32_t kb_tasklet_stack[256];
static task_t kb_tasklet = {
  .state = TASK_STOPPED,
};
/* whether the tasklet is already processing a keyboard event */
static int kb_tasklet_running = 0;

void (*kb_on_event)(kb_event_t *event) = 0;
static kb_event_t last_event;

enum {
  PS2_DATA = 0x60,
  PS2_STATUS = 0x64,
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
uint8_t kb_scancode_buffer[EVENT_BUFFER_SIZE];
queue_t kb_scancodes = {
  .size = EVENT_BUFFER_SIZE,
  .start = 0,
  .end = 0,
  .items = (void *)kb_scancode_buffer,
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

kb_event_t *kb_generate_event(uint8_t scancode)
{
  kb_keycode_t kc = kb_us_layout[scancode & 0x7f];
  unsigned int pressed = (scancode & 0x80) == 0;

  /* update modifiers */
  if (kc == KC_LSH || kc == KC_RSH) {
    MOD_SET(pressed, MOD_SHIFT);
    return 0;
  }
  if (kc == KC_CTR) {
    MOD_SET(pressed, MOD_CTRL);
    return 0;
  }
  if (kc == KC_ALT) {
    MOD_SET(pressed, MOD_ALT);
    return 0;
  }

  /* create event structure */
  kb_event_t *event = &last_event;
  event->pressed = pressed;
  event->keycode = kc;
  event->printable = keycode_to_char(kc);
  event->mods = kb_mods;
  event->timestamp = timer_get_tick();

  return event;
}

void kb_propagate_event(uint8_t scancode)
{
  if (!kb_on_event) return;
  kb_event_t *event = kb_generate_event(scancode);
  if (event) kb_on_event(event);
}

void kb_process_scancodes()
{
  while (1) {
    sched_disable_preemption();
    pic_mask(IRQ_KEYBOARD);
    while (kb_scancodes.start != kb_scancodes.end) {
      uint8_t scancode = kb_scancode_buffer[kb_scancodes.start];
      kb_scancodes.start = (kb_scancodes.start + 1) % kb_scancodes.size;
      pic_unmask(IRQ_KEYBOARD);
      sched_enable_preemption();
      kb_propagate_event(scancode);
      sched_disable_preemption();
      pic_mask(IRQ_KEYBOARD);
    }

    sched_current->state = TASK_WAITING;
    kb_tasklet_running = 0;
    pic_unmask(IRQ_KEYBOARD);
    sched_yield();
  }
}

int kb_init(void)
{
  isr_stack_t *stack = (void *)kb_tasklet_stack + sizeof(kb_tasklet_stack) - sizeof(isr_stack_t);
  stack->eip = (uint32_t) kb_process_scancodes;
  stack->eflags = EFLAGS_IF;
  stack->cs = GDT_SEL(GDT_CODE);
  kb_tasklet.stack = stack;
  kb_tasklet.state = TASK_WAITING;

  return 0;
}

void kb_irq(void)
{
  /* wait for keyboard input */
  uint8_t status = inb(PS2_STATUS);
  while (status & 0x1) {
    if ((status & (1 << 5)) == 0) break;
    status = inb(PS2_STATUS);
  }
  if ((status & 0x1) == 0) return;

  /* get scancode and save it */
  uint8_t scancode = inb(PS2_DATA);

#if KB_TASKLET
  kb_scancode_buffer[kb_scancodes.end] = scancode;
  kb_scancodes.end = (kb_scancodes.end + 1) % kb_scancodes.size;
  if (kb_scancodes.end == kb_scancodes.start) {
    /* drop old scancodes */
    kb_scancodes.start++;
  }

  /* add tasklet to scheduler */
  if (!kb_tasklet_running) {
    /* note that we can't just check the task state to deterine if the
    tasklet is running, because it may be blocked on a semaphore, in
    which case we don't want to wake it prematurely */
    kb_tasklet.state = TASK_RUNNING;
    task_list_add(&sched_runqueue, &kb_tasklet);
    kb_tasklet_running = 1;
  }
#else
  kb_event_t *event = kb_generate_event(scancode);
  if (event->pressed && event->printable) {
    kprintf("%c", event->printable);
  }
#endif

  pic_eoi(IRQ_KEYBOARD);
}

void kb_grab(void (*on_event)(kb_event_t *event))
{
  kb_on_event = on_event;
}
