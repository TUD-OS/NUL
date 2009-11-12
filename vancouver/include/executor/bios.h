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
#define DEBUG            Logging::printf("\t%s eax %x ebx %x edx %x eip %x efl %x\n", __func__, cpu->eax, cpu->ebx, cpu->edx, cpu->eip, cpu->efl)

class BiosCommon 
{
protected:
  Motherboard &_mb;


  /**
   * Special BIOS vectors.
   */
  enum {
    RESET_VECTOR = 0x100,
    WAIT_DISK_VECTOR,
    MAX_VECTOR,
  };

  /**
   * Copy to the user.
   */
  void copy_out(unsigned long address, void *ptr, unsigned count)
  {
    /**
     * No need to check segment limit and write rights here, as a
     * pmode BIOS that reloads the ES segment would also crash this
     * value!
     *
     * Note: we currently ignore the possible 64k wrap-around of a realmode segment here.
     */
    MessageMemWrite msg(address, ptr, count);
    if (!_mb.bus_memwrite.send(msg))
      Logging::printf("%s() could not copy out %x bytes to %lx\n", __PRETTY_FUNCTION__, count, address);
  }


  /**
   * Copy from the user.
   */
  void copy_in(unsigned long address, void *ptr, unsigned count)
  {    /**
     * No need to check segment limit and access rights here, as a
     * pmode BIOS that reloads the ES segment would also crash this
     * value!
     *
     * Note: we currently ignore the possible 64k wrap-around of a realmode segment here.
     */
    MessageMemRead msg(address, ptr, count);
    if (!_mb.bus_memread.send(msg))
      Logging::printf("%s() could not copy in %x bytes from %lx\n", __PRETTY_FUNCTION__, count, address);
  }


  /**
   * Write bios data helper.
   */
  inline void write_bda(unsigned short offset, unsigned value, unsigned len)
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


  BiosCommon(Motherboard &mb) : _mb(mb) {}
};
