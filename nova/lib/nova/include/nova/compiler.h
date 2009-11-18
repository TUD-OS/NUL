/* -*- Mode: C++ -*- */

#pragma once

#ifdef __GNUC__
# define NOVA_PACKED    __attribute__((packed))
# define NOVA_NORETURN  __attribute__((noreturn))
# define NOVA_ALIGN(x)  __attribute__((aligned(x)))
# define NOVA_TRAP      __builtin_trap()
# define NOVA_NOTREACHED __builtin_trap()
# define NOVA_INLINE     static inline
# define NOVA_WEAK       __attribute__((weak))
# define NOVA_MEMCLOBBER asm volatile ("" ::: "memory")
#else
# error "Unknown compiler"
#endif

#ifdef __cplusplus
# define NOVA_BEGIN namespace Nova {
# define NOVA_END   }
# define NOVA_EXTERN_C extern "C"
# define NOVA_PROTO(x) x
# define NOVA_CAST(type, expr) (reinterpret_cast<type>(expr))
#else
# define NOVA_BEGIN
# define NOVA_END
# define NOVA_EXTERN_C
# define NOVA_PROTO(x) nova_##x
# define NOVA_CAST(type, expr) ((type)expr)
#endif

/* EOF */
