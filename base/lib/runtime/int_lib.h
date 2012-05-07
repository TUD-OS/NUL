/* -*- Mode: C -*- */
#pragma once

#define COMPILER_RT_ABI
#define CHAR_BIT        8

typedef      int si_int;
typedef unsigned su_int;

typedef          long long di_int;
typedef unsigned long long du_int;

typedef union
{
    di_int all;
    struct
    {
        su_int low;
        si_int high;
    }s;
} dwords;

typedef union
{
    du_int all;
    struct
    {
        su_int low;
        su_int high;
    }s;
} udwords;

/* EOF */
