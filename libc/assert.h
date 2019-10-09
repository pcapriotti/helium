#ifndef ASSERT_H
#define ASSERT_H

#define assert(e) do { \
  if (!(e)) { kprintf("assert: %s\n", #e); \
    _panic(__FILE__, __LINE__); } } while(0)

#endif /* ASSERT_H */
