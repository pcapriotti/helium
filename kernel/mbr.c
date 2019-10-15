#include "drivers/ata/ata.h"
#include "mbr.h"

#include <stdint.h>

#define MBR_PART_TABLE 0x01be

int read_partition_table(drive_t *drive, partition_table_t table)
{
  return ata_read_bytes(drive, MBR_PART_TABLE,
                        sizeof(partition_table_t), table) != 0;
}
