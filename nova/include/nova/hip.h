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

struct HypervisorCpuDesc {
  uint8_t flags;
  uint8_t thread;
  uint8_t core;
  uint8_t package;
  uint32_t reserved;
} NOVA_PACKED;

struct HypervisorMemDesc {
  uint64_t address;
  uint64_t size;
  int16_t type;
  uint16_t auxiliary;
} NOVA_PACKED;

struct HypervisorInfoPage {
  uint32_t signature;
  uint16_t checksum;
  uint16_t hip_length;
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
} NOVA_PACKED;

NOVA_INLINE unsigned int cpu_desc_no(struct HypervisorInfoPage *hip)
{
  return (hip->mem_offset - hip->cpu_offset) / hip->cpu_desc_size;
}

NOVA_INLINE struct HypervisorCpuDesc *cpu_desc(struct HypervisorInfoPage *hip, unsigned int no)
{
  return NOVA_CAST(HypervisorCpuDesc *,
		   NOVA_CAST(char *, hip) + hip->cpu_offset + hip->cpu_desc_size*no);
}

NOVA_INLINE unsigned int mem_desc_no(struct HypervisorInfoPage *hip)
{
  return (hip->hip_length - hip->mem_offset) / hip->mem_desc_size;
}

NOVA_INLINE HypervisorMemDesc *mem_desc(struct HypervisorInfoPage *hip, unsigned int no)
{
  return NOVA_CAST(HypervisorMemDesc *,
		   NOVA_CAST(char *, hip) + hip->mem_offset + hip->mem_desc_size*no);
}

NOVA_END

/* EOF */
