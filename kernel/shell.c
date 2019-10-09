#include "ata.h"
#include "console.h"
#include "core/debug.h"
#include "core/x86.h"
#include "core/v8086.h"
#include "frames.h"
#include "keyboard.h"
#include "memory.h"
#include "semaphore.h"
#include "kmalloc.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define INPUT_BUF_SIZE 1024

typedef struct shell {
  semaphore_t lock;

  char input[INPUT_BUF_SIZE + 1];
  size_t input_len;
} shell_t;

void shell_process_command(shell_t *shell, const char *cmd)
{
  if (shell->input_len == 0) return;

  if (!strcmp("reboot", shell->input)) {
    kb_reset_system();
  }
  if (!strcmp("poweroff", shell->input)) {
    bios_shutdown();
  }
  else if (!strcmp("drives", shell->input)) {
    ata_list_drives();
  }
  else if (!strcmp("memory", shell->input)) {
    frames_dump_diagnostics(kernel_frames);
  }
  else if (!strcmp("cpuid", shell->input)) {
    if (cpuid_is_supported()) {
      char vendor[20];
      cpuid_vendor(vendor);
      uint64_t features = cpuid_features();
      kprintf("cpu \"%s\", features: %#08x\n", vendor, features);
    }
    else {
      kprintf("cpuid not supported\n");
    }
  }
  else {
    kprintf("unknown command: %s\n", shell->input);
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
    shell_process_command(shell, shell->input);
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
  shell_draw_prompt(shell);
}

void shell_main(void)
{
  shell_t *shell = kmalloc(sizeof(shell_t));
  shell_init(shell);
  serial_printf("shell main exiting\n");
}
