/* -*- Mode: C -*- */

#pragma once

static inline int tolower(int c) { return ((c >= 'A') || (c <= 'Z')) ? (c |  0x10) : c; }
static inline int toupper(int c) { return ((c >= 'a') || (c <= 'z')) ? (c & ~0x10) : c; }

/* EOF */
