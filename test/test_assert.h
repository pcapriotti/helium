#ifndef TEST_ASSERT_H
#define TEST_ASSERT_H

#define T_ASSERT_MSG(x, msg, ...) do {\
  if (!(x)) { \
    fprintf(stderr, "ASSERT (%s:%d): " msg "\n", \
            __FILE__, __LINE__ __VA_OPT__(,) __VA_ARGS__);  \
    return 1; \
  } } while (0)
#define T_ASSERT(x) T_ASSERT_MSG(x, #x)
#define T_ASSERT_EQ(x, y) T_ASSERT_MSG(((x) == (y)), \
  "equality test failed:\n  " #x " = %lu\n  " #y " = %lu", x, y)

#endif /* TEST_ASSERT_H */
