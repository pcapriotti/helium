#include "ata.h"
#include "core/debug.h"
#include "core/io.h"
#include "core/storage.h"
#include "core/util.h"
#include "drivers/drivers.h"
#include "handlers.h"
#include "pci.h"

#define MAX_BUSY_ATTEMPTS 50000

#define ATA_DEBUG 1

typedef struct ata_sector {
  uint16_t data[256];
} __attribute__((packed)) ata_sector_t;

enum {
  IDE_PROGIF_PCI = 0x05,
  IDE_PROGIF_SWITCHABLE = 0x0a,
  IDE_PROGIF_BM = 0x80,
};

/* offsets into identify structure */
enum {
  IDENTIFY_MODEL_NUMBER = 27,
  IDENTIFY_LBA28_SECTORS = 60,
  IDENTIFY_VERSION_MAJOR = 80,
  IDENTIFY_SUPPORTED_COMMANDS = 82,
  IDENTIFY_LBA48_SECTORS = 100,
};

/* bits of IDENTIFY_SUPPORTED_COMMANDS */
enum {
  SUPPORTED_LBA48 = 1 << 26,
};

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
static uint8_t ata_irq_number = 0;

/* bus master */
static uint16_t ata_bmi;

static inline uint16_t reg_port(uint8_t channel, uint8_t reg)
{
  if (reg < ATA_REG_CTRL)
    return ata_channels[channel].base + reg;
  else
    return ata_channels[channel].ctrl + reg - ATA_REG_CTRL;
}

uint8_t ata_read(uint8_t channel, uint8_t reg)
{
  uint16_t port = reg_port(channel, reg);
  return inb(port);
}

uint16_t ata_readw(uint8_t channel, uint8_t reg)
{
  return inw(reg_port(channel, reg));
}

void ata_write(uint8_t channel, uint8_t reg, uint8_t value)
{
  uint16_t port = reg_port(channel, reg);
  outb(port, value);
}

void ata_writew(uint8_t channel, uint8_t reg, uint16_t value)
{
  uint16_t port = reg_port(channel, reg);
  outw(port, value);
}

void ata_reset(uint8_t channel)
{
  ata_write(channel, ATA_REG_CTRL, ATA_CTRL_SRST);
  io_wait();
  ata_write(channel, ATA_REG_CTRL, ATA_CTRL_NIEN);
  io_wait();

#if ATA_DEBUG
  {
    int col = serial_set_colour(0);
    serial_printf("[ata] reset, ctrl: %#02x\n",
                  ata_read(channel, ATA_REG_CTRL));
    serial_set_colour(col);
  }
#endif
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

uint8_t ata_poll_busy(uint8_t channel, unsigned max)
{
  uint8_t status = ATA_ST_BSY;
  unsigned num = 0;
  while ((status & ATA_ST_BSY) && num < max) {
    status = ata_read(channel, ATA_REG_STATUS);
    num++;
  }
#if ATA_DEBUG
  int col = serial_set_colour(0);
  serial_printf("[ata] num busy poll attempts: %u\n", num);
  serial_set_colour(col);
#endif
  return status;
}

static int ata_prepare_read_write(drive_t *drive, uint64_t lba, uint32_t count)
{
  ata_write(drive->channel, ATA_REG_DRIVE_HEAD,
            0xe0 | (drive->index << 4) |
            (drive->lba48 ? 0 : ((lba >> 0x18) & 0x0f)));
  if (drive->lba48) {
    /* send high bytes */
    ata_write(drive->channel, ATA_REG_SECTOR_COUNT, count >> 8);
    ata_write(drive->channel, ATA_REG_LBA1, (lba >> 0x18) & 0xff);
    ata_write(drive->channel, ATA_REG_LBA2, (lba >> 0x20) & 0xff);
    ata_write(drive->channel, ATA_REG_LBA3, (lba >> 0x28) & 0xff);
  }
  ata_write(drive->channel, ATA_REG_SECTOR_COUNT, count);
  ata_write(drive->channel, ATA_REG_LBA1, lba & 0xff);
  ata_write(drive->channel, ATA_REG_LBA2, (lba >> 0x8) & 0xff);
  ata_write(drive->channel, ATA_REG_LBA3, (lba >> 0x10) & 0xff);

  /* if reading from a new drive, wait */
  if (ata_channels[drive->channel].last_drive != drive->index)
    ata_wait(drive->channel);
  if (ata_read(drive->channel, ATA_REG_STATUS) & ATA_ST_BSY)
    return -1;

  return 0;
}

int ata_write_lba(drive_t *drive, uint64_t lba, void *buf, uint32_t count)
{
  if (count > 0xffff) count = 0xffff;

  /* send lba and count */
  if (ata_prepare_read_write(drive, lba, count) == -1)
    return -1;

  /* send write command */
  ata_write(drive->channel, ATA_REG_STATUS, ATA_CMD_WRITE_PIO);

  ata_sector_t *sector = buf;
  for (unsigned i = 0; i < count; i++) {
    uint8_t status = ATA_ST_BSY;
    status = ata_poll_busy(drive->channel, MAX_BUSY_ATTEMPTS);
    status = ata_poll_ready(drive->channel);

    if (status & ATA_ST_ERR) {
      int col = serial_set_colour(SERIAL_COLOUR_ERR);
      serial_printf("[ata] error while writing\n");
      serial_set_colour(col);
      return -1;
    }

    for (unsigned k = 0; k < 256; k++) {
      ata_writew(drive->channel, ATA_REG_DATA, sector->data[k]);
    }

    sector++;
  }

  /* flush cache */
  ata_write(drive->channel, ATA_REG_STATUS, ATA_CMD_FLUSH_CACHE);
  ata_wait(drive->channel);

  return 0;
}

int ata_write_bytes(drive_t *drive, uint64_t offset, uint32_t bytes, void *buf)
{
  return -1;
}

void *ata_read_bytes(drive_t *drive, uint64_t offset, uint32_t bytes, void *buf)
{
  uint64_t lba = offset >> 9;
  uint64_t lba_end = ROUND64(offset + bytes, 9);
  uint32_t count = lba_end - lba;

  if (count > 0xffff) count = 0xffff;

  /* send lba and count */
  if (ata_prepare_read_write(drive, lba, count) == -1)
    return 0;

  /* send read command */
  ata_write(drive->channel, ATA_REG_STATUS, ATA_CMD_READ_PIO);

  unsigned int j = 0;
  uint32_t buf_offset = offset - ((uint64_t)lba << 9);
  uint8_t *result = (uint8_t*)buf;

  /* read data */
  for (unsigned int i = 0; i < count; i++) {
    uint8_t status = ATA_ST_BSY;
    status = ata_poll_busy(drive->channel, MAX_BUSY_ATTEMPTS);
    status = ata_poll_ready(drive->channel);

    if (status & ATA_ST_ERR) {
      int col = serial_set_colour(SERIAL_COLOUR_ERR);
      serial_printf("[ata] error while writing\n");
      serial_set_colour(col);
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

void ata_controller_type(device_t *ide,
                         int (*print)(const char *, ...))
{
  int pci = ide->prog_if & IDE_PROGIF_PCI;
  int dual = ide->prog_if & IDE_PROGIF_SWITCHABLE;
  int bm = ide->prog_if & IDE_PROGIF_BM;

  print("%s%s%s",
        pci ? "PCI mode" : "ISA mode",
        dual ? ", switchable" : "",
        bm ? ", bus mastering" : "");
}

static inline uint32_t identify_readl(uint16_t *id, int offset)
{
  return id[offset] | (id[offset + 1] << 16);
}

static inline uint64_t identify_readll(uint16_t *id, int offset)
{
  return (uint64_t) id[offset] |
    ((uint64_t) id[offset + 1] << 16) |
    ((uint64_t) id[offset + 2] << 32) |
    ((uint64_t) id[offset + 3] << 48);
}

int ata_identify_drive(drive_t *drive)
{
  uint8_t status;
  drive->present = 0;

  /* select drive */
  ata_write(drive->channel, ATA_REG_DRIVE_HEAD, 0xe0 | (drive->index << 4));
  ata_wait(drive->channel);

  status = ata_poll_busy(drive->channel, MAX_BUSY_ATTEMPTS);
  if (status & ATA_ST_BSY) {
    return 0;
  }

  ata_channels[drive->channel].last_drive = drive->index;

  /* identify */
  ata_write(drive->channel, ATA_REG_SECTOR_COUNT, 0);
  ata_write(drive->channel, ATA_REG_LBA1, 0);
  ata_write(drive->channel, ATA_REG_LBA2, 0);
  ata_write(drive->channel, ATA_REG_LBA3, 0);
  ata_write(drive->channel, ATA_REG_STATUS, ATA_CMD_IDENTIFY);

  status = ata_read(drive->channel, ATA_REG_STATUS);
  if (!status || (status & ATA_ST_ERR)) {
    return 0;
  }

  status = ata_poll_busy(drive->channel, MAX_BUSY_ATTEMPTS);

  /* check if ATAPI */
  if (ata_read(drive->channel, ATA_REG_LBA2) ||
      ata_read(drive->channel, ATA_REG_LBA3)) {
    return 0;
  }

  status = ata_poll_ready(drive->channel);
  if (status & ATA_ST_ERR) {
    return 0;
  }

  /* read identify structure */
  uint16_t id[256];
  for (int i = 0; i < 256; i++) {
    id[i] = ata_readw(drive->channel, ATA_REG_DATA);
  }

  for (int i = 0; i < 20; i++) {
    uint16_t x = id[IDENTIFY_MODEL_NUMBER + i];
    drive->model[2 * i] = (x >> 8) & 0xff;
    drive->model[2 * i + 1] = x & 0xff;
  }
  drive->model[40] = '\0';

  uint32_t supported_commands =
    identify_readl(id, IDENTIFY_SUPPORTED_COMMANDS);

  drive->lba48 = supported_commands & SUPPORTED_LBA48;
  drive->lba_sectors = drive->lba48
    ? identify_readll(id, IDENTIFY_LBA48_SECTORS)
    : identify_readl(id, IDENTIFY_LBA28_SECTORS);

#if ATA_DEBUG
  serial_printf("[ata] major version: %#04x\n",
                id[IDENTIFY_VERSION_MAJOR]);
  serial_printf("[ata] supported commands: %#08x\n",
                identify_readl(id, IDENTIFY_SUPPORTED_COMMANDS));
#endif

  drive->present = 1;
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

      uint64_t kb = drives[i].lba_sectors >> 1;
      uint64_t mb = kb >> 10;
      uint64_t gb = mb >> 10;
      uint64_t tb = gb >> 10;
      if (tb) {
        kprintf("%llu TB", tb);
      }
      else if (gb) {
        kprintf("%llu GB", gb);
      }
      else if (mb) {
        kprintf("%llu MB", mb);
      } else {
        kprintf("%llu kB", kb);
      }

      kprintf("\n");
    }
  }
}

void ata_irq(struct isr_stack *stack)
{
  int serviced = 0;
  for (int channel = 0; channel < 2; channel++) {
    uint8_t status = ata_read(channel, ATA_REG_STATUS);
    serviced = serviced || (status != 0);
    if (status & ATA_ST_ERR) {
      int col = serial_set_colour(SERIAL_COLOUR_ERR);
      serial_printf("[ata] error: %#02x\n",
                    ata_read(channel, ATA_REG_ERROR));
      serial_set_colour(col);
    }
  }
  if (serviced) pic_eoi(ata_irq_number);
}
HANDLER_STATIC(ata_irq_handler, ata_irq);

int ata_init(void *data, device_t *ide)
{
  if (!ide) return -1;
  if (ata_initialised) return 0;

#if ATA_DEBUG
  {
    serial_printf("[ata] initialising: ");
    ata_controller_type(ide, serial_printf);
    serial_printf("\n");
  }
#endif

  ata_channels[0].base = (ide->bars[0] && (ide->prog_if & IDE_PROGIF_PCI)) ?
    (ide->bars[0] & ~3) : ATA_PRIMARY_BASE;
  ata_channels[0].ctrl = (ide->bars[1] && (ide->prog_if & IDE_PROGIF_PCI)) ?
    (ide->bars[1] & ~3) : ATA_PRIMARY_CTRL;
  ata_channels[1].base = (ide->bars[2] && (ide->prog_if & IDE_PROGIF_PCI)) ?
    (ide->bars[2] & ~3) : ATA_SECONDARY_BASE;
  ata_channels[1].ctrl = (ide->bars[3] && (ide->prog_if & IDE_PROGIF_PCI)) ?
    (ide->bars[3] & ~3) : ATA_SECONDARY_CTRL;
  ata_bmi = ide->bars[4];

  ata_irq_number = ide->irq & 0xff;
  if (!ata_irq_number) ata_irq_number = ATA_SECONDARY_IRQ;

#if ATA_DEBUG
  serial_printf("[ata] irq number = %u\n", ata_irq_number);
#endif
  if (ata_irq_number) {
    irq_grab(ata_irq_number, &ata_irq_handler);
  }

  /* initialise drives */
  uint8_t i = 0;
  for (uint8_t channel = 0; channel < 2; channel++) {
    ata_reset(channel);

    for (uint8_t drive = 0; drive < 2; drive++) {
#if ATA_DEBUG
      {
        int col = serial_set_colour(0);
        serial_printf("[ata] identify drive %u channel %u\n", drive, channel);
        serial_set_colour(col);
      }
#endif
      drives[i].channel = channel;
      drives[i].index = drive;
      int ok = ata_identify_drive(&drives[i]);
      if (ok) {
#if ATA_DEBUG
        serial_printf("[ata] drive %u channel %u: ", i);
        {
          uint64_t kb = drives[i].lba_sectors >> 1;
          uint64_t mb = kb >> 10;
          uint64_t gb = mb >> 10;
          uint64_t tb = gb >> 10;
          if (tb) {
            serial_printf("%llu TB", tb);
          }
          else if (gb) {
            serial_printf("%llu GB", gb);
          }
          else if (mb) {
            serial_printf("%llu MB", mb);
          }
          else {
            serial_printf("%llu kB", kb);
          }
        }
        serial_printf("\n");
#endif
        ata_initialised = 1;
      }
      i++;
    }
  }

  return 0;
}

typedef struct {
  drive_t *drive;
  uint32_t part_offset;
} ata_ops_data_t;

static int ata_ops_write_unaligned(void *_data, void *buf, void *scratch,
                                   uint64_t offset,
                                   uint32_t bytes)
{
  return -1;
}


static int ata_ops_write(void *_data, void *buf,
                         uint64_t offset,
                         uint32_t bytes)
{
  return -1;
}

static int ata_ops_read(void *_data, void *buf,
                          uint64_t offset,
                          uint32_t bytes)
{
  ata_ops_data_t *data = _data;
#if ATA_CLOSURE_DEBUG
  serial_printf("reading at %#x from drive %u:%u with part offset %#x\n",
                offset, data->drive->channel,
                data->drive->index,
                data->part_offset);
#endif
  void *ret = ata_read_bytes(data->drive,
                             offset + ((uint64_t) data->part_offset << 9),
                             bytes, buf);
  return ret ? 0 : -1;
}

static int ata_ops_read_unaligned(void *data, void *buf, void *scratch,
                                    uint64_t offset, uint32_t bytes)
{
  return ata_ops_read(data, buf, offset, bytes);
}

driver_t ata_driver = {
  .data = 0,
  .matches = ata_is_ide_controller,
  .init = ata_init,
};

storage_ops_t ata_storage_ops = {
  .read = ata_ops_read,
  .read_unaligned = ata_ops_read_unaligned,
  .write = ata_ops_write,
  .write_unaligned = ata_ops_write_unaligned,
};
