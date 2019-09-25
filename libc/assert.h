#ifndef ASSERT_H
#define ASSERT_H

#define assert(e) do { \
    if (!(e)) { kprintf("assert: " #e "\n"); panic(); } } while(0)

#endif /* ASSERT_H */
