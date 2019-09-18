#ifndef ASSERT_H
#define ASSERT_H

#define assert(e) do { \
    if (!(e)) { text_panic("assert: " #e); } } while(0)

#endif /* ASSERT_H */
