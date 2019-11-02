#ifndef DRIVERS_REALTEK_COMMON_H
#define DRIVERS_REALTEK_COMMON_H

#include <stdint.h>

struct device;

enum {
  REG_MAC = 0x00,
  REG_TSD = 0x10,
  REG_TSAD = 0x20,
  REG_RBSTART = 0x30,
  REG_CMD = 0x37,
  REG_CAPR = 0x38,
  REG_CBR = 0x3a,
  REG_INT_MASK = 0x3c,
  REG_INT_STATUS = 0x3e,
  REG_RX_CONF = 0x44,
  REG_CONFIG_1 = 0x52,
};

enum {
  CMD_BUFE = 1 << 0,
  CMD_TE = 1 << 2,
  CMD_RE = 1 << 3,
  CMD_RST = 1 << 4,
};

enum {
  INT_MASK_ROK = 1 << 0,
  INT_MASK_RER = 1 << 1,
  INT_MASK_TOK = 1 << 2,
  INT_MASK_TER = 1 << 3,
  INT_MASK_RXOVW = 1 << 4,
  INT_MASK_PUN = 1 << 5,
  INT_MASK_FOVW = 1 << 6,
  INT_MASK_LEN_CHG = 1 << 13,
  INT_MASK_TIMEOUT = 1 << 14,
  INT_MASK_SERR = 1 << 15,
};

enum {
  RX_CONF_AAP = 1 << 0, /* all packets */
  RX_CONF_APM = 1 << 1, /* physical match */
  RX_CONF_AM = 1 << 2, /* multicast */
  RX_CONF_AB = 1 << 3, /* broadcast */
  RX_CONF_WRAP = 1 << 7,
};

uint16_t rtl_find_iobase(struct device *dev);

#endif /* DRIVERS_REALTEK_COMMON_H */
