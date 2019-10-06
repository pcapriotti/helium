#ifndef ASSERT_H
#define ASSERT_H

#define assert(e) do { \
    if (!(e)) { kprintf("assert: %s (%s:%d)\n", \
                        #e, __FILE__, __LINE__); \
      panic(); } } while(0)

#endif /* ASSERT_H */
