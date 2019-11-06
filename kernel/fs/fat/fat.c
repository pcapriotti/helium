#include "core/debug.h"
#include "core/storage.h"
#include "fs/fat/fat.h"
#include "kmalloc.h"
#include "memory.h"

#include <assert.h>
#include <string.h>

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

static int fat_match_signature(fat_bpb_v4_t *v4)
{
  if (v4->signature != 0x28 && v4->signature != 0x29)
    return FAT_VERSION_INVALID;

  if (v4->fs_type[0] != 'F') return FAT_VERSION_INVALID;
  if (v4->fs_type[1] != 'A') return FAT_VERSION_INVALID;
  if (v4->fs_type[2] != 'T') return FAT_VERSION_INVALID;

  if (!memcmp(&v4->fs_type[3], "12   ", 5)) return FAT_VERSION_FAT12;
  if (!memcmp(&v4->fs_type[3], "16   ", 5)) return FAT_VERSION_FAT16;
  if (!memcmp(&v4->fs_type[3], "32   ", 5)) return FAT_VERSION_FAT32;
  if (!memcmp(&v4->fs_type[3], "     ", 5)) return FAT_VERSION_UNKNOWN;

  return FAT_VERSION_INVALID;
}

static int fat_declared_version(fat_superblock_t *sb)
{
  int version = fat_match_signature(&sb->v7.v4);
  if (version == FAT_VERSION_INVALID) {
    version = fat_match_signature(&sb->v4);
  }
  /* TODO check for a BPB 3.4 */
  return version;
}

static int fat_version(fat_t *fat, fat_superblock_t *sb)
{
  int version = fat_declared_version(sb);
  if (version != FAT_VERSION_UNKNOWN)
    return version;

  if (fat->num_clusters <= 4086) return FAT_VERSION_FAT12;
  if (fat->num_clusters <= 65526) return FAT_VERSION_FAT16;

  return FAT_VERSION_FAT32;
}

size_t fat_cluster_offset(fat_t *fat, unsigned cluster)
{
  assert(cluster >= 2);
  return fat->data_offset + (cluster - 2) * fat->cluster_size;
}

void fat_init(fat_t *fat, storage_t *storage)
{
  /* read superblock */
  fat->buffer = kmalloc(1 << storage->ops->alignment);
  storage_read(storage, fat->buffer, 0, 1 << storage->ops->alignment);
  fat_superblock_t *sb = fat->buffer;

  /* determine width */

  /* save superblock info */
  fat->cluster_size = sb->bytes_per_sector * sb->sectors_per_cluster;
  fat->map_offset = sb->bytes_per_sector * sb->num_reserved_sectors;
  fat->map_size = sb->bytes_per_sector * fat_sectors_per_fat(sb);
  size_t dir_offset = fat->map_offset + fat->map_size * sb->num_fats;
  size_t dir_size = sb->num_root_dir_entries * sizeof(fat_dir_entry_t);
  fat->data_offset = dir_offset +
    (dir_size + sb->bytes_per_sector - dir_size % sb->bytes_per_sector);
  fat->num_clusters = (fat_total_sectors(sb) * sb->bytes_per_sector - fat->data_offset) /
    fat->cluster_size;
  fat->version = fat_version(fat, sb);

  kfree(fat->buffer);

  /* allocate a cluster-size buffer */
  fat->buffer = falloc(fat->cluster_size);
}

void fat_cleanup(fat_t *fat)
{
  ffree(fat->buffer);
}

int fat_read_cluster(fat_t *fat, void *buffer, unsigned cluster)
{
  return 0;
}

int fat_dir_iterator_init(fat_dir_iterator_t *it, fat_t *fs, unsigned cluster)
{
  it->fs = fs;
  it->cluster = cluster;
  it->cluster_offset = 0;
  it->index = 0;

  return 0;
}
