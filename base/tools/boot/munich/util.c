/*
 * \brief   Utility functions for a bootloader
 * \date    2006-03-28
 * \author  Bernhard Kauer <kauer@tudos.org>
 */
/*
 * Copyright (C) 2006,2007,2010  Bernhard Kauer <kauer@tudos.org>
 * Technische Universitaet Dresden, Operating Systems Research Group
 *
 * This file is part of the OSLO package, which is distributed under
 * the  terms  of the  GNU General Public Licence 2.  Please see the
 * COPYING file for details.
 */


#include <stdarg.h>
#include "util.h"

/**
 * Wait roughly a given number of milliseconds.
 *
 * We use the PIT for this.
 */
void
wait(int ms)
{
  /* the PIT counts with 1.193 Mhz */
  ms*=1193;

  /* initalize the PIT, let counter0 count from 256 backwards */
  outb(0x43, 0x34);
  outb(0x40, 0);
  outb(0x40, 0);

  unsigned short state;
  unsigned short old = 0;
  while (ms>0)
    {
      outb(0x43, 0);
      state = inb(0x40);
      state |= inb(0x40) << 8;
      ms -= (unsigned short)(old - state);
      old = state;
    }
}

/**
 * Print the exit status and reboot the machine.
 */
void
__exit(unsigned status)
{
  out_char('\n');
  out_description("exit()", status);
  for (unsigned i=0; i<16;i++)
    {
      wait(1000);
      out_char('.');
    }
  out_string("-> OK, reboot now!\n");
  reboot();
}

#ifndef NDEBUG
static unsigned int serial_initialized;
#define SERIAL_BASE 0x3f8

void
serial_init()
{
  serial_initialized = 1;
  // enable DLAB and set baudrate 115200
  outb(SERIAL_BASE+0x3, 0x80);
  outb(SERIAL_BASE+0x0, 0x01);
  outb(SERIAL_BASE+0x1, 0x00);
  // disable DLAB and set 8N1
  outb(SERIAL_BASE+0x3, 0x03);
  // reset IRQ register
  outb(SERIAL_BASE+0x1, 0x00);
  // enable fifo, flush buffer, enable fifo
  outb(SERIAL_BASE+0x2, 0x01);
  outb(SERIAL_BASE+0x2, 0x07);
  outb(SERIAL_BASE+0x2, 0x01);
  // set RTS,DTR
  outb(SERIAL_BASE+0x4, 0x03);
}


static
void
serial_send(unsigned value)
{
  if (!serial_initialized)
    return;

  while (!(inb(SERIAL_BASE+0x5) & 0x20))
    ;
  outb(SERIAL_BASE, value);
}
#endif


/**
 * Output a single char.
 * Note: We allow only to put a char on the last line.
 */
int
out_char(unsigned value)
{
#define BASE(ROW) ((unsigned short *) (0xb8000+ROW*160))
  static unsigned int col;
  if (value!='\n')
    {
      unsigned short *p = BASE(24)+col;
      *p = 0x0f00 | value;
      col++;
    }
#ifndef NDEBUG
  else
    serial_send('\r');
#endif

  if (col>=80 || value == '\n')
    {
      col=0;
      unsigned short *p=BASE(0);
      memcpy(p, p+80, 24*160);
      memset(BASE(24), 0, 160);
    }

#ifndef NDEBUG
  serial_send(value);
#endif

  return value;
}





/**
 * Output a string.
 */
void
out_string(const char *value)
{
  for(; *value; value++)
    out_char(*value);
}


/**
 * Output a single hex value.
 */
void
out_hex(unsigned value, unsigned bitlen)
{
  int i;
  for (i=bsr(value | 1<<bitlen) &0xfc; i>=0; i-=4)
    {
      unsigned a = (value >> i) & 0xf;
      if (a>=10)
	a += 7;
      a+=0x30;

      out_char(a);
    }
}

/**
 * Output a string followed by a single hex value, prefixed with a
 * message label.
 */
void
out_description(const char *prefix, unsigned int value)
{
  out_string(message_label);
  out_string(prefix);
  out_char(' ');
  out_hex(value, 0);
  out_char('\n');
}

/**
 * Output a string, prefixed with a message label.
 */
void
out_info(const char *msg)
{
  out_string(message_label);
  out_string(msg);
  out_char('\n');
}

int
strcmp (char const *s1, char const *s2)
{
    while (*s1 && *s1 == *s2) 
        s1++, s2++;
 
    return *s1 - *s2;
}

int
isspace (int c)
{
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

unsigned long
strtoul (char const *ptr, char const **end, int base)
{  
    while (isspace (*ptr))
        ptr++;

    if (!base) {   
        if (*ptr != '0')
            base = 10;
        else if (*(ptr + 1) == 'x')
            ptr += 2, base = 16;
        else
            ptr += 1, base = 8;
    }

    unsigned long val = 0;
    unsigned char c;

    while (c = *ptr) {

        int x = (c >= 'a') ? c - 'a' + 10 :
                (c >= 'A') ? c - 'A' + 10 :
                (c >= '0') ? c - '0' : 0xff;
 
        if (x >= base)
            break;

        val = val * base + x;

        ptr++;
    }

    if (end)
        *end = ptr;

    return val;
}

char *
get_arg (char **line, char delim)
{
    for (; isspace (**line); ++*line) ;

    if (!**line)
        return 0;

    char *arg = *line;

    for (; delim == ' ' ? !isspace (**line) : **line != delim; ++*line)
        if (!**line)
            return arg;

    *(*line)++ = 0;

    return arg;
}
