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

/* offsets from control IO port */
enum {
  ATA_STATUS_ALT = 0,
  ATA_ADDRESS
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
  ATA_CMD_IDENTIFY = 0xec
};

typedef struct {
  uint16_t flags;
  uint16_t num_cyls;
  uint16_t reserved;
  uint16_t num_heads;
  uint16_t unformatted_bytes_per_track;
  uint16_t unformatted_bytes_per_sector;
  uint16_t num_sectors;
  uint16_t vendor[3];
  char serial_no[20];
  uint16_t buf_type;
  uint16_t buf_size; /* in units of 512 bytes */
  uint16_t ecc_bytes;
  char fw_revision[8];
  char model_number[40];
  uint16_t vendor_unique;
  uint16_t doubleword;
  uint16_t capabilities;
  uint16_t reserved2;
  uint16_t pio_timing;
  uint16_t dma_timing;
  uint16_t reserved3;
  uint16_t num_cur_cyl;
  uint16_t num_cur_heads;
  uint16_t num_cur_sectors;
  uint32_t capacity;
  uint16_t multiple_sectors;
  uint32_t lba_sectors;
} __attribute__((packed)) ata_identify_t;

typedef struct drive {
  uint8_t present;
  uint8_t channel;
  uint8_t index;
  uint32_t lba_sectors;
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
