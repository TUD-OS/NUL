/**
 * Generic HW register implementation.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
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
#include <string.h>


/**
 * A general device register, use e.g. for the PCI config space and
 * AHCI registers.
 *
 * The mask defines which bits of the register are readonly.  The rw1s
 * and rw1c masks define whether external writes to read-write bits
 * behave as set-on-one or clear-on-one respectively.
 */
class HwRegister {
 public:
  const char *name;
  unsigned bitoffset;
  unsigned char  bitlen;
  unsigned value;
  unsigned mask;
  unsigned rw1s;
  unsigned rw1c;
  unsigned callback;
  short saveindex;
};

#define REGISTER(name, bitoffset, bitlen, value, mask, rw1s, rw1c, callback) { name, bitoffset, bitlen, value, mask, rw1s, rw1c, callback, 0}
#define REGISTER_RW(name, bitoffset, bitlen, value, mask)    REGISTER(name, bitoffset, bitlen, value, mask, 0, 0, 0)
// a private register, RO to external, but RW to internal callers
#define REGISTER_PR(name, bitoffset, bitlen, value)          REGISTER(name, bitoffset, bitlen, value,    0, 0, 1, 0)
#define REGISTER_RO(name, bitoffset, bitlen, value)          REGISTER_RW(name, bitoffset, bitlen, value, 0)
#define REGISTERSET(name, ...) template <> HwRegister HwRegisterSet< name >::_hw_regs[] = {  __VA_ARGS__ , REGISTER_RO(0, 0, 0, 0) }


template <typename Z>
class HwRegisterSet {
  static HwRegister _hw_regs[];
  unsigned *_regs_data;
  int _reg_count;


 protected:

  /**
   * Return the mask of a register.
   */
  unsigned get_reg_mask(int index)
  {
    if (index >=0 && index < _reg_count) return _hw_regs[index].mask;
    return 0;
  } 

  /**
   * Default empty callback function.
   */
  void write_callback(unsigned flags, unsigned old_value, unsigned new_value) {};


  /**
   * Modify a register. This is an internal function used to update values.
   */
  void modify_reg(int index, unsigned delete_bits, unsigned set_bits)
  {
    unsigned old_value;
    if (read_reg(index, old_value) && ~_hw_regs[index].saveindex)
      _regs_data[_hw_regs[index].saveindex] = old_value & ~delete_bits | set_bits;
    else
      Logging::panic("can not modify read-only register '%s'!\n", _hw_regs[index].name);
  }

  /**
   * Reset a register to its default state.
   */
  void reset_reg(int index)
  {
    if (~_hw_regs[index].saveindex)  _regs_data[_hw_regs[index].saveindex] = _hw_regs[index].value;
  }

 public:
  /**
   * Find a register by name.
   */
  int find_reg(const char *name) const
  {
    for (unsigned i=0; _hw_regs[i].name; i++) 
      if (!strcmp(name, _hw_regs[i].name)) return i;
    return -1;
  }


  /**
   * Read a register.
   */
  bool read_reg(int index, unsigned &value)
  {
    if (index >=0 && index < _reg_count)
      {
	if (~_hw_regs[index].saveindex)
	  value = _regs_data[_hw_regs[index].saveindex];
	else
	  value = _hw_regs[index].value;
	return true;
      }
    return false;
  }


  /**
   * Write a single register.  External writes respect the different
   * masks.
   */
  void write_reg(int index, unsigned value, bool external, Z *obj = 0)
  {
    unsigned old_value;
    if (read_reg(index, old_value) && ~_hw_regs[index].saveindex)
      {
	if (external)
	  {
	    value = value & ~_hw_regs[index].rw1s | ( value | old_value) & _hw_regs[index].rw1s;
	    value = value & ~_hw_regs[index].rw1c | (~value & old_value) & _hw_regs[index].rw1c;
	    unsigned new_value = old_value & ~_hw_regs[index].mask | value & _hw_regs[index].mask;

	    _regs_data[_hw_regs[index].saveindex] = new_value;
	    if (obj && _hw_regs[index].callback)  obj->write_callback(_hw_regs[index].callback, old_value, new_value);
	  }
	else
	  _regs_data[_hw_regs[index].saveindex] = value;
      }
  }


  /**
   * Read all register that fall in a given address range.
   */
  bool read_all_regs(unsigned address, unsigned &value, unsigned size)
  {    
    assert(size <= 4 && !(size & size -1));
    bool res = false;
    unsigned tmp = 0;
    unsigned tmp2;
    for (int i = 0; i < _reg_count; i++) 
      if (in_range(address, (_hw_regs[i].bitoffset / 8) & ~(size-1), size) && read_reg(i, tmp2)) 
	{
	  tmp |= tmp2 << (_hw_regs[i].bitoffset & (size*8-1));
	  res = true;
	}
    if (res)  value = tmp;
    return res;
  }


  /**
   * Write all register that fall in a given address range.
   */
  bool write_all_regs(unsigned address, unsigned value, unsigned size, Z *obj = 0)
  {    
    assert(size <= 4 && !(size & size -1));
    bool res = false;
    for (int i = 0; i < _reg_count; i++) 
      if (in_range(address, (_hw_regs[i].bitoffset / 8) & ~(size-1), size)) 
	{
	  write_reg(i, value >> (_hw_regs[i].bitoffset & (size*8-1)), true, obj);
	  res = true;
	}
    return res;
  }

 protected:

  HwRegisterSet() 
    {
      // count the bytes we need per-object storage for
      int save_index = 0;
      _reg_count = 0;
      for (unsigned i=0; _hw_regs[i].name; i++) 
	{
	  //Logging::printf("\treg %s pos %x-%d value %x mask %x\n",_hw_regs[i].name, _hw_regs[i].bitoffset / 8, _hw_regs[i].bitlen, _hw_regs[i].value, _hw_regs[i].mask);
	  _reg_count++;
	  if (_hw_regs[i].mask | _hw_regs[i].rw1s | _hw_regs[i].rw1c) 
	    {
	      assert(_hw_regs[i].bitlen <= sizeof(_hw_regs[i].value)*8);
	      assert(_hw_regs[i].bitlen == 32 || !(_hw_regs[i].mask >> _hw_regs[i].bitlen));
	      _hw_regs[i].saveindex = save_index++;;
	    }
	  else
	    _hw_regs[i].saveindex = ~0;
	}
      // allocate the backing store
      _regs_data = reinterpret_cast<unsigned *>(malloc(sizeof(*_regs_data) * save_index));
      
      // initialize the backing store from the default values
      for (unsigned i=0; _hw_regs[i].name; i++)
	reset_reg(i);
    }
};
