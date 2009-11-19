/* -*- Mode: C++ -*- */

#pragma once

#ifdef __GNUC__
# define NOVA_ALIGN(x)   __attribute__((aligned(x)))
# define NOVA_INLINE     static inline
# define NOVA_MEMCLOBBER asm volatile ("" ::: "memory")
# define NOVA_NORETURN   __attribute__((noreturn))
# define NOVA_NOTREACHED __builtin_trap()
# define NOVA_PACKED     __attribute__((packed))
# define NOVA_REGPARM(x) __attribute__((regparm(x)))
# define NOVA_TRAP       __builtin_trap()
# define NOVA_WEAK       __attribute__((weak))
#else
# error "Unknown compiler"
#endif

#ifdef __cplusplus
# define NOVA_CAST(type, expr) (reinterpret_cast<type>(expr))
# define NOVA_BEGIN namespace Nova {
# define NOVA_END   }
# define CPU_BEGIN  namespace Cpu {
# define CPU_END    }
# define NOVA_EXTERN_C extern "C"
# define NOVA_PROTO(x) x
#else
# define NOVA_CAST(type, expr) ((type)expr)
# define NOVA_BEGIN
# define NOVA_END
# define CPU_BEGIN
# define CPU_END
# define NOVA_EXTERN_C
# define NOVA_PROTO(x) nova_##x
#endif

/* EOF */
