#include "console/console.h"
#include "core/debug.h"
#include "core/x86.h"
#include "core/v8086.h"
#include "drivers/ata/ata.h"
#include "drivers/keyboard/keyboard.h"
#include "frames.h"
#include "kmalloc.h"
#include "memory.h"
#include "semaphore.h"
#include "timer.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INPUT_BUF_SIZE 1024
#define ERROR_COLOUR 0x0090232a
#define OK_COLOUR 0x007c9a59

typedef struct shell {
  semaphore_t lock;

  char input[INPUT_BUF_SIZE + 1];
  size_t input_len;
} shell_t;

void shell_process_command(shell_t *shell)
{
  if (shell->input_len == 0) return;

  char *saveptr = 0;
  char *cmd = strtok_r(shell->input, " ", &saveptr);

  if (!cmd || strlen(cmd) == 0) return;

  if (!strcmp("reboot", cmd)) {
    kb_reset_system();
  }
  else if (!strcmp("poweroff", cmd)) {
    bios_shutdown();
  }
  else if (!strcmp("ticks", cmd)) {
    uint32_t tick = timer_get_tick();
    kprintf("%u\n", tick);
  }
  else if (!strcmp("drives", cmd)) {
    ata_list_drives();
  }
  else if (!strcmp("memory", cmd)) {
    const char *ty = strtok_r(0, " ", &saveptr);
    if (!ty || strlen(ty) == 0 || !strcmp(ty, "kernel")) {
      frames_dump_diagnostics(&kernel_frames);
    }
    else if (!strcmp(ty, "user")) {
      frames_dump_diagnostics(&user_frames);
    }
    else if (!strcmp(ty, "dma")) {
      frames_dump_diagnostics(&dma_frames);
    }
    else {
      console_set_fg(ERROR_COLOUR);
      kprintf("unknown allocator `%s'\n", ty);
      console_reset_fg();
    }
  }
  else if (!strcmp("cpuid", cmd)) {
    if (cpuid_is_supported()) {
      char vendor[20];
      cpuid_vendor(vendor);
      uint64_t features = cpuid_features();
      kprintf("cpu \"%s\", features: %#08x\n", vendor, features);
    }
    else {
      console_set_fg(ERROR_COLOUR);
      kprintf("cpuid not supported\n");
      console_reset_fg();
    }
  }
  else {
    console_set_fg(ERROR_COLOUR);
    kprintf("unknown command: %s\n", cmd);
    console_reset_fg();
  }
}

void shell_draw_prompt(shell_t *shell)
{
  kprintf("> ");
}

void on_kb_event(kb_event_t *event, void *data)
{
  if (!event->pressed) return;

  shell_t *shell = data;
  sem_wait(&shell->lock);
  if (event->keycode == KC_ENT) {
    kprintf("\n");
    shell_process_command(shell);
    shell->input_len = 0;
    shell->input[shell->input_len] = '\0';
    shell_draw_prompt(shell);
  }
  else if (event->keycode == KC_BSP) {
    if (shell->input_len > 0) {
      if (console.cur.x > 0) {
        console_set_cursor((point_t) { console.cur.x - 1, console.cur.y });
        console_delete_char(console.cur);
      }

      shell->input_len--;
      shell->input[shell->input_len] = '\0';
    }
  }
  else if (event->printable) {
    if (shell->input_len < INPUT_BUF_SIZE - 1) {
      shell->input[shell->input_len++] = event->printable;
      shell->input[shell->input_len] = '\0';
      /* echo */
      kprintf("%c", event->printable);
    }
  }
  sem_signal(&shell->lock);
}

void shell_init(shell_t *shell)
{
  shell->input_len = 0;
  sem_init(&shell->lock, 1);

  kb_grab(on_kb_event, shell);
  console_set_fg(OK_COLOUR);
  kprintf("Helium debug shell\n");
  console_reset_fg();
  shell_draw_prompt(shell);
}

void shell_main(void *data)
{
  shell_t *shell = kmalloc(sizeof(shell_t));
  shell_init(shell);
}
