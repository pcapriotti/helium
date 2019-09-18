#ifndef ASSERT_H
#define ASSERT_H

#define assert(e) do { \
    if (!(e)) { panic(); } } while(0)

#endif /* ASSERT_H */
