/**
 * Common BIOS code.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

#define VB_UNIMPLEMENTED    Logging::panic("\t%s():%d eax %x ebx %x edx %x eip %x:%x\n", __func__, __LINE__, cpu->eax, cpu->ebx, cpu->edx, cpu->cs.base, cpu->eip)
#define DEBUG            Logging::printf("\t%s eax %x ebx %x ecx %x edx %x eip %x efl %x\n", __func__, cpu->eax, cpu->ebx, cpu->ecx, cpu->edx, cpu->eip, cpu->efl)

class BiosCommon
{
protected:
  Motherboard &_mb;
#include "model/simplemem.h"

  /**
   * Special BIOS vectors.
   */
  enum {
    RESET_VECTOR = 0x100,
    WAIT_DISK_VECTOR,
    MAX_VECTOR,
  };


  /**
   * Write bios data helper.
   */
  void write_bda(unsigned short offset, unsigned value, unsigned len)
  {
    assert(len <= sizeof(value));
    copy_out(0x400 + offset, &value, len);
  }


  /**
   * Read bios data helper.
   */
  unsigned read_bda(unsigned offset)
  {
    unsigned res;
    copy_in(0x400 + offset, &res, sizeof(res));
    return res;
  }

  /**
   * Jump to another realmode INT handler.
   */
  bool jmp_int(CpuState *cpu, unsigned char number)
  {
    // we build a new iret fram of the stack, to return to
    unsigned short idt_frames[6];

    // the destination
    copy_in(number*4, idt_frames, 4);
    // the old frame
    copy_in(cpu->ss.base + cpu->esp, idt_frames + 3, 6);
    // flags are the same
    idt_frames[2] = idt_frames[5];
    // space for the new frame
    cpu->esp -= 6;
    copy_out(cpu->ss.base + cpu->esp, idt_frames, 12);
    return true;
  }

  /**
   * Set the usual error indication.
   */
  void error(CpuState *cpu, unsigned char errorcode)
  {
    cpu->efl |= 1;
    cpu->ah = errorcode;
  }

  BiosCommon(Motherboard &mb) : _mb(mb), _bus_memregion(&mb.bus_memregion), _bus_mem(&mb.bus_mem) {}
};
