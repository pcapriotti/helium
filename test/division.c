#include <stdint.h>
#include <stdio.h>

#include "../src/math.h"
#include "test_assert.h"

int test_div64(uint64_t a, uint64_t b)
{
  uint64_t q1 = a / b;
  uint64_t q2 = div64sd(a, b);

  T_ASSERT_EQ(q1, q2);
  return 0;
}

int main(int argc, char **argv)
{
  int ret = 0;
  ret = test_div64(39287ULL, 1098) || ret;
  ret = test_div64(29732032987ULL, 2891) || ret;
  ret = test_div64(6823184706356444491ULL, 109) || ret;
  ret = test_div64(4946386074277337033ULL, 42008) || ret;

  return ret;
}
