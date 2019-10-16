#ifndef DRIVERS_H
#define DRIVERS_H

struct device;

typedef struct driver {
  void *data;
  int (*matches)(void *data, struct device *dev);
  int (*init)(void *data, struct device *dev);
} driver_t;

void drivers_init(void);

#endif /* DRIVERS_H */
