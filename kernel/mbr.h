#ifndef MBR_H
#define MBR_H

typedef struct {
  uint8_t bootable;
  uint8_t start_head;
  uint8_t start_sector; /* bits 6-7 are high bits of start_cylinder */
  uint8_t start_cylinder;
  uint8_t system_id;
  uint8_t end_head;
  uint8_t end_sector; /* bits 6-7 are high bits of start_cylinder */
  uint8_t end_cylinder;
  uint32_t lba_start;
  uint32_t num_sectors;
} __attribute__((packed)) partition_table_entry_t;

typedef partition_table_entry_t partition_table_t[4];

struct drive;

int read_partition_table(struct drive *drive, partition_table_t table);

#endif /* MBR_H */
