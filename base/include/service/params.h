/** @file
 * Parameter handling.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#pragma once

#include <nul/compiler.h>

/**
 * Defines strings and functions for a parameter with the given
 * name. The variadic part is used to store a help text.
 *
 * PARAM_HANDLER(example, "example - this is just an example for parameter Passing",
 *       		  "Another help line...")
 * { Logging::printf("example parameter function called!\n"); }
 */
#define PARAM_HANDLER(NAME, ...)						\
  const char * __parameter_##NAME##_strings[] asm ("__parameter_" #NAME "_strings") = { #NAME, ##__VA_ARGS__, 0}; \
  asm volatile (".section .param; .long __parameter_" #NAME "_function, __parameter_" #NAME "_strings; .previous;"); \
  EXTERN_C void __parameter_##NAME##_function(Motherboard &mb, unsigned long *argv, const char *args, unsigned args_len)

/* Defined in linker script: */
extern long __param_table_start, __param_table_end;


/**
 * Define an alias for a set of parameters.
 */
#define PARAM_ALIAS(NAME, DESC, VALUE) PARAM_HANDLER(NAME, #NAME " - " DESC, "value: "  VALUE) { mb.parse_args(VALUE); }
