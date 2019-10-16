#include "ata.h"
#include "core/debug.h"
#include "core/io.h"
#include "drivers/drivers.h"
#include "pci.h"

#define ROUND64(a, i) (((uint64_t)a + (1 << (i)) - 1) >> i)

#define ATA_DEBUG 0

struct channel_struct {
  /* base IO port */
  uint16_t base;
  /* base control port */
  uint16_t ctrl;
  /* last drive selected */
  uint8_t last_drive;
};

channel_t ata_channels[2] = {0};
drive_t drives[4] = {0};
static int ata_initialised = 0;

static inline uint16_t reg_port(uint8_t channel, uint8_t reg)
{
  if (reg < ATA_REG_CTRL)
    return ata_channels[channel].base + reg;
  else
    return ata_channels[channel].ctrl + reg - ATA_REG_CTRL;
}

uint8_t ata_read(uint8_t channel, uint8_t reg)
{
  return inb(reg_port(channel, reg));
}

uint16_t ata_readw(uint8_t channel, uint8_t reg)
{
  return inw(reg_port(channel, reg));
}

void ata_write(uint8_t channel, uint8_t reg, uint8_t value)
{
  outb(reg_port(channel, reg), value);
}

void ata_reset(uint8_t channel)
{
  ata_write(channel, ATA_REG_CTRL, 4);
  io_wait();
  ata_write(channel, ATA_REG_CTRL, 0);
  io_wait();
}

void ata_wait(uint8_t channel)
{
  for (int i = 0; i < 4; i++) ata_read(channel, ATA_REG_STATUS);
}

uint8_t ata_poll_ready(uint8_t channel)
{
  uint8_t status = 0;
  while (!(status & (ATA_ST_RDY | ATA_ST_ERR))) {
    status = ata_read(channel, ATA_REG_STATUS);
  }
  return status;
}

uint8_t ata_poll_busy(uint8_t channel)
{
  uint8_t status = ATA_ST_BSY;
  while (status & ATA_ST_BSY) {
    status = ata_read(channel, ATA_REG_STATUS);
  }
  return status;
}

void *ata_read_bytes(drive_t *drive, uint64_t offset, uint32_t bytes, void *buf)
{
  /* maximum offset: 2 TB
     maximum number of bytes: 128 kB */
  uint32_t lba = offset >> 9;
  uint32_t lba_end = ROUND64(offset + bytes, 9);
  uint32_t count = lba_end - lba;
  if (count > 0xff) return 0;

  /* send read command */
  ata_write(drive->channel, ATA_REG_DRIVE_HEAD,
            0xe0 | (drive->index << 4) | ((lba >> 0x18) & 0x0f));
  ata_write(drive->channel, ATA_REG_SECTOR_COUNT, count);
  ata_write(drive->channel, ATA_REG_LBA1, lba & 0xff);
  ata_write(drive->channel, ATA_REG_LBA2, (lba >> 0x8) & 0xff);
  ata_write(drive->channel, ATA_REG_LBA3, (lba >> 0x10) & 0xff);
  /* if reading from a new drive, wait */
  if (ata_channels[drive->channel].last_drive != drive->index)
    ata_wait(drive->channel);
  if (ata_read(drive->channel, ATA_REG_STATUS) & ATA_ST_BSY)
    return 0;
  ata_write(drive->channel, ATA_REG_STATUS, ATA_CMD_READ_PIO);

  unsigned int j = 0;
  uint32_t buf_offset = offset - ((uint64_t)lba << 9);
  uint8_t *result = (uint8_t*)buf;

  for (unsigned int i = 0; i < count; i++) {
    uint8_t status = ATA_ST_BSY;
    status = ata_poll_busy(drive->channel);
    status = ata_poll_ready(drive->channel);

    if (status & ATA_ST_ERR) {
      kprintf("ata: error while reading\n");
      return 0;
    }

    for (unsigned int k = 0; k < 256; k++) {
      uint16_t val = ata_readw(drive->channel, ATA_REG_DATA);
      if (buf_offset) {
        buf_offset -= 2;
      }
      else if (j < bytes - 1) {
        result[j++] = val & 0xff;
        result[j++] = (val >> 8) & 0xff;
      }
      else if (j < bytes) {
        result[j++] = val & 0xff;
      }
    }
  }

  return result;
}

void *ata_read_lba(drive_t *drive, uint32_t lba, uint8_t count, void *buf)
{
  uint64_t offset = (uint64_t)lba << 9;
  uint32_t bytes = (uint32_t)count << 9;
  return ata_read_bytes(drive, offset, bytes, buf);
}

int ata_identify_drive(drive_t *drive)
{
  uint8_t status;
  drive->present = 0;

  /* select drive */
  ata_write(drive->channel, ATA_REG_DRIVE_HEAD, 0xe0 | (drive->index << 4));
  ata_wait(drive->channel);
  status = ata_read(drive->channel, ATA_REG_STATUS);
  if (status == 0 || status & ATA_ST_BSY) {
    return 0;
  }
  ata_channels[drive->channel].last_drive = drive->index;

  /* identify */
  ata_write(drive->channel, ATA_REG_SECTOR_COUNT, 0);
  ata_write(drive->channel, ATA_REG_LBA1, 0);
  ata_write(drive->channel, ATA_REG_LBA2, 0);
  ata_write(drive->channel, ATA_REG_LBA3, 0);
  ata_write(drive->channel, ATA_REG_STATUS, ATA_CMD_IDENTIFY);

  /* ata_wait(drive->channel); */
  status = ata_read(drive->channel, ATA_REG_STATUS);
#if ATA_DEBUG
  kprintf("ata identify: status %#x\n", status);
#endif
  if (!status || (status & ATA_ST_ERR)) return 0;

  status = ata_poll_busy(drive->channel);
#if ATA_DEBUG
  kprintf("non-busy status: %#x\n", status);
#endif

  /* check if ATAPI */
  if (ata_read(drive->channel, ATA_REG_LBA2) ||
      ata_read(drive->channel, ATA_REG_LBA3)) return 0;

  status = ata_poll_ready(drive->channel);
#if ATA_DEBUG
  kprintf("ready status: %#x\n", status);
#endif
  if (status & ATA_ST_ERR) return 0;

  /* read identify structure */
  union {
    uint16_t buf[256];
    ata_identify_t fields;
  } id;

  for (int i = 0; i < 256; i++) {
    id.buf[i] = ata_readw(drive->channel, ATA_REG_DATA);
  }

  for (int i = 0; i < 20; i++) {
    drive->model[2 * i] = id.fields.model_number[2 * i + 1];
    drive->model[2 * i + 1] = id.fields.model_number[2 * i];
  }
  drive->model[40] = '\0';
  drive->present = 1;
  drive->lba_sectors = id.fields.lba_sectors;
  return 1;
}

drive_t *ata_get_drive(uint8_t index)
{
  return &drives[index];
}

int ata_is_ide_controller(void *data, device_t *dev)
{
  return (dev->class == PCI_CLS_STORAGE &&
          dev->subclass == PCI_STORAGE_IDE);
}

void ata_list_drives(void)
{
  for (int i = 0; i < 4; i++) {
    drive_t *drive = &drives[i];
    if (drive->present) {
      kprintf("drive %d: %s ", i, drive->model);

      uint32_t kb = drives[i].lba_sectors >> 1;
      uint32_t mb = kb >> 10;
      uint32_t gb = mb >> 10;
      if (gb) {
        kprintf("%u GB", gb);
      }
      else if (mb) {
        kprintf("%u MB", mb);
      } else {
        kprintf("%u kB", kb);
      }

      kprintf("\n");
    }
  }
}

int ata_init(void *data, device_t *ide)
{
  if (!ide) return -1;
  if (ata_initialised) return 0;
  ata_initialised = 1;

#if ATA_DEBUG
  kprintf("initialising ATA driver\n");
#endif

  ata_channels[0].base = ide->bars[0] ? ide->bars[0] : ATA_PRIMARY_BASE;
  ata_channels[0].ctrl = ide->bars[1] ? ide->bars[1] : ATA_PRIMARY_CTRL;
  ata_channels[1].base = ide->bars[2] ? ide->bars[2] : ATA_SECONDARY_BASE;
  ata_channels[1].ctrl = ide->bars[3] ? ide->bars[3] : ATA_SECONDARY_CTRL;

  /* initialise drives */
  uint8_t i = 0;
  for (uint8_t channel = 0; channel < 2; channel++) {
    ata_reset(channel);

    for (uint8_t drive = 0; drive < 2; drive++) {
      drives[i].channel = channel;
      drives[i].index = drive;
      int ok = ata_identify_drive(&drives[i]);
      if (ok) {
#if ATA_DEBUG
        kprintf("drive %u: ", i);
        {
          uint32_t kb = drives[i].lba_sectors >> 1;
          uint32_t mb = kb >> 10;
          uint32_t gb = mb >> 10;
          if (gb) {
            kprintf("%u GB", gb);
          }
          else if (mb) {
            kprintf("%u MB", mb);
          } else {
            kprintf("%u kB", kb);
          }
        }
        kprintf("\n");
#endif
      }
      i++;
    }
  }

  return 0;
}

driver_t ata_driver = {
  .data = 0,
  .matches = ata_is_ide_controller,
  .init = ata_init,
};
