#ifndef FS_FAT_FAT_H
#define FS_FAT_FAT_H

#include <stdint.h>
#include <stddef.h>

struct storage;

enum {
  FAT_VERSION_INVALID,
  FAT_VERSION_FAT12,
  FAT_VERSION_FAT16,
  FAT_VERSION_FAT32,
  FAT_VERSION_UNKNOWN,
};

typedef struct fat_bpb_v4 {
  uint8_t drive_number;
  uint8_t flags;
  uint8_t signature;
  uint32_t volume_id;
  char volume_label[11];
  char fs_type[8];
} __attribute__((packed)) fat_bpb_v4_t;

typedef struct fat_bpb_v7 {
  uint32_t sectors_per_fat_32;
  uint16_t fat_flags;
  uint16_t fs_version;
  uint32_t root_cluster;
  uint16_t superblock_number;
  uint16_t superblock_backup;
  uint8_t reserved[12];
  fat_bpb_v4_t v4;
} __attribute__((packed)) fat_bpb_v7_t;

typedef struct fat_superblock {
  uint8_t jump_code[3];
  char oem[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t num_reserved_sectors;
  uint8_t num_fats;
  uint16_t num_root_dir_entries;
  uint16_t total_sectors_16;
  uint8_t media_descriptor_type;
  uint16_t sectors_per_fat_16;
  uint16_t sectors_per_track;
  uint16_t num_heads;
  uint32_t num_hidden_sectors;
  uint32_t total_sectors_32;
  union {
    fat_bpb_v4_t v4;
    fat_bpb_v7_t v7;
  } __attribute__((packed));
} __attribute__((packed)) fat_superblock_t;

typedef struct fat_dir_entry {
  char filename[11];
  uint8_t attributes;
  uint8_t reserved;
  uint8_t creation_time[3];
  uint8_t creation_date[2];
  uint8_t access_date[2];
  uint16_t cluster_hi;
  uint8_t modified_time[2];
  uint8_t modified_date[2];
  uint16_t cluster_lo;
  uint32_t size;
} __attribute__((packed)) fat_dir_entry_t;

enum {
  FAT_ENTRY_RO = 1 << 0,
  FAT_ENTRY_HIDDEN = 1 << 1,
  FAT_ENTRY_SYS = 1 << 2,
  FAT_ENTRY_VLABEL = 1 << 3,
  FAT_ENTRY_DIR = 1 << 4,
  FAT_ENTRY_ARCHIVE = 1 << 5,
};

typedef struct fat {
  void *buffer;
  unsigned cluster_size;
  size_t map_offset;
  size_t map_size;
  size_t data_offset;
  unsigned num_clusters;
  struct storage *storage;
  int version;
} fat_t;

typedef struct fat_dir_iterator {
  fat_t *fs;
  unsigned cluster;
  size_t cluster_offset;
  int index;
} fat_dir_iterator_t;

#endif /* FS_FAT_FAT_H */
