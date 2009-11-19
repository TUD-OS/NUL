/* -*- Mode: C++ -*- */

#pragma once

#include <nova/compiler.h>
#include <nova/types.h>

NOVA_BEGIN

enum {
  // Magic
  NOVA_SIGNATURE = 0x41564f4e,
  
  // Memory types
  MEM_MULTIBOOT     = -2,
  MEM_HYPERVISOR    = -1,
  MEM_AVAILABLE     =  1,
  MEM_RESERVED      =  2,
  MEM_ACPI_RECLAIM  =  3,
  MEM_ACPI_NVS      =  4,
};

typedef struct HipCpu {
  uint8_t flags;
  uint8_t thread;
  uint8_t core;
  uint8_t package;
  uint32_t reserved;
} NOVA_PACKED HipCpu;

typedef struct HipMem {
  uint64_t address;
  uint64_t size;
  int16_t type;
  uint16_t aux;
} NOVA_PACKED HipMem;

typedef struct Hip {
  uint32_t signature;
  uint16_t checksum;
  uint16_t length;
  uint16_t cpu_offset;
  uint16_t cpu_desc_size;
  uint16_t mem_offset;
  uint16_t mem_desc_size;
  uint32_t api_flags;
  uint32_t api_version;
  uint32_t sel;		// Maximum number of capabilities
  uint32_t pre;		// Predefined capabilities
  uint32_t gsi;		// number of GSIs
  uint32_t _reserved;
  uint32_t page_sizes;	// bit n set -> page size n supported
  uint32_t utcb_sizes;	// bit n set -> page size n supported
  uint32_t tsc_freq_khz;
  uint32_t bus_freq_khz;
} NOVA_PACKED Hip;

NOVA_INLINE unsigned int hip_cpu_desc_no(Hip *hip)
{
  return (hip->mem_offset - hip->cpu_offset) / hip->cpu_desc_size;
}

NOVA_INLINE HipCpu *hip_cpu_desc(Hip *hip, unsigned int no)
{
  return NOVA_CAST(HipCpu *,
		   NOVA_CAST(char *, hip) + hip->cpu_offset + hip->cpu_desc_size*no);
}

NOVA_INLINE unsigned hip_mem_desc_no(Hip *hip)
{
  return (hip->length - hip->mem_offset) / hip->mem_desc_size;
}

NOVA_INLINE HipMem *hip_mem_desc(Hip *hip, unsigned int no)
{
  return NOVA_CAST(HipMem *,
		   NOVA_CAST(char *, hip) + hip->mem_offset + hip->mem_desc_size*no);
}

NOVA_INLINE uint16_t hip_calc_checksum(Hip *hip)
{
  uint16_t res = 0;
  for (unsigned i=0; i < hip->length / sizeof(uint16_t); i++)
    res += NOVA_CAST(uint16_t *, hip)[i];
  return res;
}


NOVA_INLINE HipMem *hip_module(Hip *hip, unsigned count)
{
  unsigned max = hip_mem_desc_no(hip);

  for (unsigned i = 0; i < max; i++) {
    HipMem *cur = hip_mem_desc(hip, i);

    if (cur->type == MEM_MULTIBOOT)
      if (count-- == 0) 
	return cur;
  }

  return 0;
}

NOVA_INLINE void hip_fix_checksum(Hip *hip) { hip->checksum -= hip_calc_checksum(hip); }

NOVA_END

/* EOF */
