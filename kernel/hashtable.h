#ifndef HT_KEY_TYPE
# error "define HT_KEY_TYPE before including hashtable.h"
#else

#include <stdint.h>

#ifndef HT_NAME
# define HT_NAME HT_KEY_TYPE
#endif

#define _CAT(a, b) a##_##b
#define CAT(a, b) _CAT(a, b)

#define HT_STRUCT CAT(hashtable, HT_NAME)
#define HT_TYPE CAT(HT_STRUCT, t)

#define HT_PREFIX CAT(ht, HT_NAME)
#define P(x) CAT(HT_PREFIX, x)

typedef struct HT_STRUCT HT_TYPE;

HT_TYPE *P(new)();
void P(insert)(HT_TYPE *ht, HT_KEY_TYPE key, void *value);
void *P(get)(HT_TYPE *ht, HT_KEY_TYPE key);
void P(del)(HT_TYPE *ht);
int P(size)(HT_TYPE *ht);

#endif
