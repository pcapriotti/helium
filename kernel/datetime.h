#ifndef DATETIME_H
#define DATETIME_H

typedef struct date {
  unsigned day;
  unsigned month;
  unsigned long year;
} date_t;

typedef struct time {
  unsigned seconds;
  unsigned minutes;
  unsigned hours;
} time_t;

typedef struct datetime {
  date_t date;
  time_t time;
} datetime_t;

void datetime_debug(datetime_t *dt);

#endif /* DATETIME_H */
