/* -*- Mode: C -*- */

#pragma once

static inline int isspace(int c)
{
  switch (c) {
  case ' ':
  case '\f':
  case '\n':
  case '\r':
  case '\t':
  case '\v':
    return 1;
  default:
    return 0;
  }
}

static inline int isdigit(int c) { return (c >= '0') && (c <= '9'); };
static inline int isupper(int c) { return (c >= 'A') && (c <= 'Z'); };
static inline int islower(int c) { return (c >= 'a') && (c <= 'z'); };
static inline int isalpha(int c) { return isupper(c) || islower(c); };
static inline int isalnum(int c) { return isalpha(c) || isdigit(c); };

static inline int tolower(int c) { return isupper(c) ? (c |  0x20) : c; }
static inline int toupper(int c) { return islower(c) ? (c & ~0x20) : c; }


/* EOF */
