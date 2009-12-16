/* -*- Mode: C -*- */

#pragma once

#include <stddef.h>
#include <stdarg.h>

int printf(const char *format, ...) __attribute__ ((format(printf, 1, 2)));;
int vprintf(const char *format, va_list ap);

int snprintf(char *str, size_t size, const char *format, ...) __attribute__ ((format(printf, 3, 4)));;
int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int putchar(int c);		/* Not implemented by libstdio */
int puts(const char *s);

/* Insecure -> Disabled */
/* int sprintf(char *str, const char *format, ...); */
/* int vsprintf(char *str, const char *format, va_list ap); */

/* EOF */
