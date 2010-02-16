/* XXX Major hackery. */

#define STRING_INLINE

/* We don't need extern "C", because we compile the non-inline version
   from C only. For the inline version, it does not matter. */
#include <string.h>
