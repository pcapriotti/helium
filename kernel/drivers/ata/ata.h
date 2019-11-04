#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_PRIMARY_BASE 0x1F0
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_PRIMARY_IRQ 14

#define ATA_SECONDARY_BASE 0x170
#define ATA_SECONDARY_CTRL 0x376
#define ATA_SECONDARY_IRQ 15

/* registers */
enum {
  ATA_REG_DATA = 0,
  ATA_REG_ERROR,
  ATA_REG_SECTOR_COUNT,
  ATA_REG_LBA1,
  ATA_REG_LBA2,
  ATA_REG_LBA3,
  ATA_REG_DRIVE_HEAD,
  ATA_REG_STATUS,

  ATA_REG_CTRL,
  ATA_REG_DRIVE_ADDR,
};

/* error bits */
enum {
  /* Address mark not found */
  ATA_ERR_AMNF = 0,
  /* Track zero not found */
  ATA_ERR_TKZNF,
  /* Aborted command */
  ATA_ERR_ABRT,
  /* Media change request */
  ATA_ERR_MCR,
  /* ID not found */
  ATA_ERR_IDNF,
  /* Media changed */
  ATA_ERR_MC,
  /* Uncorrectable data error */
  UNC,
  /* Bad block detected */
  BBK
};

/* status masks */
enum {
  /* an error occurred */
  ATA_ST_ERR = 1 << 0,
  /* always set to zero */
  ATA_ST_IDX = 1 << 1,
  /* corrected data, always zero */
  ATA_ST_CORR = 1 << 2,
  /* data available or can accept data */
  ATA_ST_DRQ = 1 << 3,
  /* overlapped mode service request */
  ATA_ST_SRV = 1 << 4,
  /* drive fault */
  ATA_ST_DR = 1 << 5,
  /* drive ready */
  ATA_ST_RDY = 1 << 6,
  /* preparing */
  ATA_ST_BSY = 1 << 7
};

/* ATA commands */
enum {
  ATA_CMD_READ_PIO = 0x20,
  ATA_CMD_WRITE_PIO = 0x30,
  ATA_CMD_FLUSH_CACHE = 0xe7,
  ATA_CMD_IDENTIFY = 0xec,
};

/* ATA control register */
enum {
  ATA_CTRL_NIEN = 1 << 1,
  ATA_CTRL_SRST = 1 << 2,
};

typedef struct drive {
  uint8_t present;
  uint8_t channel;
  uint8_t index;
  uint8_t lba48;
  uint64_t lba_sectors;
  char model[41];
} drive_t;

typedef struct channel_struct channel_t;

struct device;

drive_t *ata_get_drive(uint8_t drive);
void *ata_read_bytes(drive_t *drive, uint64_t offset, uint32_t bytes, void *buf);
void *ata_read_lba(drive_t *drive, uint32_t lba, uint8_t count, void *buf);
void ata_list_drives(void);

extern struct driver ata_driver;

#endif /* ATA_H */
