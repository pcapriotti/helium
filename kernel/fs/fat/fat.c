#include "core/debug.h"
#include "core/storage.h"
#include "fs/fat/fat.h"
#include "kmalloc.h"

unsigned long fat_total_sectors(fat_superblock_t *sb)
{
  if (sb->total_sectors_16)
    return sb->total_sectors_16;
  else
    return sb->total_sectors_32;
}

unsigned long fat_sectors_per_fat(fat_superblock_t *sb)
{
  if (sb->sectors_per_fat_16)
    return sb->sectors_per_fat_16;
  else
    return sb->v7.sectors_per_fat_32;
}

unsigned long fat_root_dir_sectors(fat_superblock_t *sb)
{
  return (sb->num_root_dir_entries * sizeof(fat_dir_entry_t) +
          sb->bytes_per_sector - 1) / sb->bytes_per_sector;
}

unsigned long fat_total_clusters(fat_superblock_t *sb)
{
  unsigned long start = sb->num_reserved_sectors;
  start += sb->num_fats * fat_sectors_per_fat(sb);
  start += fat_root_dir_sectors(sb);

  return (fat_total_sectors(sb) - start) /
    sb->sectors_per_cluster;
}

void fat_init(fat_t *fat, storage_t *storage)
{
  /* read superblock */
  fat->buffer = kmalloc(1 << storage->ops->alignment);
  storage_read(storage, fat->buffer, 0, 1 << storage->ops->alignment);
  fat_superblock_t *sb = fat->buffer;

  serial_printf("total clusters: %u\n", fat_total_clusters(sb));
}

void fat_cleanup(fat_t *fat)
{
  kfree(fat->buffer);
}
