#include "core/debug.h"
#include "keyboard.h"
#include "semaphore.h"
#include "kmalloc.h"

#include <stddef.h>
#include <stdint.h>

#define INPUT_BUF_SIZE 1024

typedef struct shell {
  semaphore_t lock;

  char input[INPUT_BUF_SIZE + 1];
  size_t input_len;
} shell_t;

void shell_process_command(shell_t *shell, const char *cmd)
{
  kprintf("unknown command: %s\n", shell->input);
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
    shell_draw_prompt(shell);
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
}
