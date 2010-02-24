/*
 * Hypervisor Information Page (HIP)
 *
 * Copyright (C) 2008, Udo Steinberg <udo@hypervisor.org>
 * Changed for Vancouver: 2008, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of the NOVA microhypervisor.
 *
 * NOVA is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NOVA is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License version 2 for more details.
 */

#pragma once
#include "vmm/cpu.h"

typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned long long uint64;

class Hip_cpu
{
    public:
        uint8   flags;
        uint8   thread;
        uint8   core;
        uint8   package;
	uint32  reserved;
};

class Hip_mem
{
    public:
        uint64  addr;
        uint64  size;
        int     type;
        uint32  aux;
};

class Hip
{
    public:
        uint32  signature;
        uint16  checksum;       // HIP checksum
        uint16  length;         // HIP length
        uint16  cpu_offs;       // Offset of first CPU descriptor
        uint16  cpu_size;
        uint16  mem_offs;       // Offset of first MEM descriptor
        uint16  mem_size;
        uint32  api_flg;        // API feature flags
        uint32  api_ver;        // API version
        uint32  cfg_cap;        // Number of CAPs
        uint32  cfg_exc;        // Number of Exception portals
        uint32  cfg_vm;         // Number of VM portals
        uint32  cfg_gsi;        // Number of GSIs
        uint32  cfg_page;       // PAGE sizes
        uint32  cfg_utcb;       // UTCB sizes
        uint32  freq_tsc;       // TSC freq in khz
        uint32  freq_bus;       // BUS freq in khz


	uint16 calc_checksum()
	{
	  uint16 res = 0;
	  for (unsigned i=0; i < length / sizeof(uint16); i++)
	    res += reinterpret_cast<uint16 *>(this)[i];
	  return res;
	}
	void fix_checksum() { checksum -= calc_checksum(); }

	Hip_mem *get_mod(unsigned nr) {
	  for (int i=0; mem_size && i < (length - mem_offs) / mem_size; i++)
	    {
	      Hip_mem *hmem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(this) + mem_offs + i * mem_size);
	      if (hmem->type != -2) continue;
	      if (!nr--) return hmem;
	    }
	  return 0;
	}

	void append_mem(uint64 addr, uint64 size, int type, uint32 aux)
	{
	  Hip_mem * mem = reinterpret_cast<Hip_mem *>(reinterpret_cast<char *>(this) + length);
	  length += mem_size;
	  mem->addr = addr;
	  mem->size = size;
	  mem->type = type;
	  mem->aux = aux;
	}

	bool has_vmx() {  return api_flg &  (1 << 1); }
	bool has_svm() {  return api_flg &  (1 << 2); }
};
