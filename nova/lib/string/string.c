#include <nova/compiler.h>

/* XXX Major hackery. */

#undef NOVA_INLINE
#define NOVA_INLINE

/* We don't need NOVA_EXTERN_C, because we compile the non-inline
   version from C only. For the inline version, it does not matter. */
#include <string.h>
