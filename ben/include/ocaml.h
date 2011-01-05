extern "C" {
#include "callback.h"
#include "memory.h"
#include "mlvalues.h"
}

#define REGPARM(X)      __attribute__ ((regparm(X)))
#define NORETURN        __attribute__ ((noreturn))
#define PACKED          __attribute__ ((__packed__))
#define UNREACHED       __builtin_trap()
#define EXTERN_C        extern "C"

EXTERN_C REGPARM(0)
void caml_startup (char ** argv);

EXTERN_C REGPARM(0)
value * caml_named_value (char const * name);

EXTERN_C REGPARM(0)
value caml_callback (value closure, value arg);

EXTERN_C REGPARM(0)
value caml_copy_string (char const *);

void start_ocaml ();
