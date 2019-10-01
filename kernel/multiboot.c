#include <stdint.h>

#define MB_MAGIC 0X1badb002
#define MB_FLAGS 0x1
#define MB_CHECKSUM -(MB_MAGIC + MB_FLAGS)

typedef struct mb_header {
  uint32_t magic;
  uint32_t flags;
  uint32_t checksum;
} mb_header_t;

mb_header_t __attribute__((section(".multiboot"))) multiboot = {
  .magic = MB_MAGIC,
  .flags = MB_FLAGS,
  .checksum = MB_CHECKSUM,
};
