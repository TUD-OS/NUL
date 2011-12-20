/**
 * @file Logical Disk Manager.
 *
 * Scans supported logical disks (e.g. partitions) and makes them
 * available to other programs as ordinary disks.
 *
 * Copyright (C) 2011, Michal Sojka <sojka@os.inf.tu-dresden.de>
 *
 * This file is part of Vancouver.nova.
 *
 * Vancouver.nova is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * Vancouver.nova is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "service/logging.h"
#include "nul/program.h"
#include "sigma0/console.h"
#include "nul/service_disk.h"
#include "host/dma.h"
#include "wvtest.h"
#include <stdint.h>

#include "crc32.cc"

class DiskHelper : public DiskProtocol {
  static char disk_buffer[4<<20];
public:
  DiskHelper(unsigned cap_base, unsigned cap_num, unsigned instance) : DiskProtocol(cap_base, instance) {
    unsigned res;
    cap_sel sem_cap = cap_base + cap_num;
    cap_sel tmp_cap = cap_base + cap_num + 1;

    KernelSemaphore *sem = new KernelSemaphore(sem_cap, true);
    DiskProtocol::DiskConsumer *diskconsumer = new (1<<12) DiskProtocol::DiskConsumer();
    assert(diskconsumer);

    res = attach(*BaseProgram::myutcb(), disk_buffer, sizeof(disk_buffer), tmp_cap,
		 diskconsumer, sem);
    if (res) Logging::panic("DiskProtocol::attach failed: %d\n", res);
  }

  unsigned read_synch(unsigned disknum, uint64_t start_secotr, size_t size) {
    DmaDescriptor dma;
    unsigned res;

    dma.byteoffset = 0;
    dma.bytecount  = size;
    assert(size <= sizeof(disk_buffer));
    res = read(*BaseProgram::myutcb(), disknum, /*usertag*/0, start_secotr, /*dmacount*/1, &dma);
    if (res)
      return res;

    sem->downmulti();
    assert(consumer->has_data());
    MessageDiskCommit *msg = consumer->get_buffer();
    assert(msg->usertag == 0);
    consumer->free_buffer();
    return ENONE;
  }
};

class Partition {
  enum {
    SECTOR_SHIFT = 9L,
    SECTOR_SIZE = 1L << SECTOR_SHIFT,
  };

  struct partition {
    char status;
    char start_chs[3];
    unsigned char type;
    char end_chs[3];
    uint32_t start_lba;
    uint32_t num_sectors;
  } __attribute__ ((packed));

  struct mbr {
    char code[440];
    uint32_t disk_signature;
    uint16_t zero;
    struct partition part[4];
    uint16_t mbr_signature;
  } __attribute__ ((packed));

  static_assert(sizeof(mbr) == SECTOR_SIZE, "Wrong size of MBR");

public:
  static unsigned add_extended_partitions(DiskHelper *disk, unsigned disknum, struct partition *e)
  {
    unsigned res;
    unsigned num = 5;
    struct mbr ebr;
    struct partition *p = NULL;
    assert(e->start_lba);
    do {
      uint64_t start = e->start_lba + (p ? p->start_lba : 0);
      disk->read_synch(disknum, start, SECTOR_SIZE);
      ebr = *reinterpret_cast<struct mbr*>(disk->dma_buffer);
      if (ebr.mbr_signature != 0xaa55)
	return EPROTO;
      p = ebr.part;
      switch (p->type) {
      case 0: continue;
      default: {
	if (p->start_lba && p->num_sectors) {
	  char name[20];
	  Vprintf::snprintf(name, sizeof(name), "%dp%d", disknum, num++);
	  DiskProtocol::Segment seg(disknum, start + p->start_lba, p->num_sectors);
	  check1(res, res = disk->add_logical_disk(*BaseProgram::myutcb(), name, 1, &seg));
	} else
	  Logging::printf("Non-LBA partition %d on disk %d? Skipping.\n", num, disknum);
      }
      }
      p++;
    } while (p->start_lba > 0);
    return ENONE;
  }


  static bool find(DiskHelper *disk, unsigned disknum)
  {
    bool found = false;
    disk->read_synch(disknum, 0, SECTOR_SIZE);

    struct mbr mbr = *reinterpret_cast<struct mbr*>(disk->dma_buffer);
    if (mbr.mbr_signature != 0xaa55)
      return false;
    for (unsigned i=0; i<4; i++) {
      struct partition *p = &mbr.part[i];
      if (p->type) {
	if (p->start_lba && p->num_sectors) {
	  char name[20];
	  Vprintf::snprintf(name, sizeof(name), "%dp%d", disknum, i+1);
	  DiskProtocol::Segment seg(disknum, p->start_lba, p->num_sectors);

	  switch (p->type) {
	  case 0xee: break;// Do not add GUID partition table
	  default:
	    check1(false, disk->add_logical_disk(*BaseProgram::myutcb(), name, 1, &seg));
	    found = true;
	  }

	  switch (p->type) {
	  case 0x5: 		// Extended CHS
	  case 0xE:		// Extended LBA
	    check1(false, add_extended_partitions(disk, disknum, p));
	  }
	} else
	  Logging::printf("Non-LBA partition %d on disk %d? Skipping.\n", i, disknum);
      }
    }
    return found;
  }
};

class GPT {
  typedef struct guid {
    unsigned char id[16];
    void as_text(char *dest);
  } guid_t;

  struct header {
    char     signature[8];
    uint32_t revision;
    uint32_t hsize;
    uint32_t crc;
    uint32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_lba;
    uint64_t last_lba;
    guid_t   disk_guid;
    uint64_t pent_lba;
    uint32_t pent_num;
    uint32_t pent_size;
    uint32_t crc_part;

    bool check_crc();
  } __attribute__((packed));

  typedef uint16_t utf16le;

  struct pent { // Partition entry
    enum { // attributes
      SYSTEM = 1ull<<0,
      BIOS_BOOTABLE = 1ull<<2,
      READ_ONLY = 1ull<<60,
      HIDDEN = 1ull<<62,
      NO_AUTOMOUNT = 1ull<<63,
    };
    guid_t   type;
    guid_t   id;
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attr;
    utf16le  name[36];
  } __attribute__((packed));

public:

  static bool find(DiskHelper *disk, unsigned disknum)
  {
#define skip_if(cond, msg, ...) if (cond) { Logging::printf(msg " on disk %d - skiping\n", ##__VA_ARGS__, disknum); return false; }
    unsigned res;

    disk->read_synch(disknum, 1, 512);
    struct header gpt = *reinterpret_cast<struct header*>(disk->dma_buffer);

    if (strncmp(gpt.signature, "EFI PART", sizeof(gpt.signature)) != 0)
      return false;
    skip_if(gpt.revision < 0x00000100, "Incompatible GPT revision");
    skip_if(gpt.hsize < sizeof(gpt), "GPT too small");
    skip_if(gpt.current_lba != 1, "GPT LBA mismatch");
    skip_if(!gpt.check_crc(), "GPT CRC mismatch");

    res = disk->read_synch(disknum, gpt.backup_lba, 512);
    skip_if(res != ENONE, "Cannot read backup GPT");
    struct header backup_gpt = *reinterpret_cast<struct header*>(disk->dma_buffer);

    skip_if(!backup_gpt.check_crc(), "Backup GPT CRC mismatch");
    skip_if(backup_gpt.current_lba != gpt.backup_lba, "Backup GPT LBA mismatch");

    disk->read_synch(disknum, 2, gpt.pent_num*gpt.pent_size);

    skip_if(gpt.crc_part != (calc_crc(~0L, reinterpret_cast<const uint8_t*>(disk->dma_buffer), gpt.pent_num*gpt.pent_size) ^ ~0L),
	    "GPT partition entries CRC mismatch");

    for (unsigned p = 0; p < gpt.pent_num; p++) {
      struct pent pent = *reinterpret_cast<struct pent*>(reinterpret_cast<char*>(disk->dma_buffer) + p*gpt.pent_size);
      if (pent.first_lba >= pent.last_lba)
	continue;

      char name[20];
      Vprintf::snprintf(name, sizeof(name), "%dp%d", disknum, p+1);

      char part_name[5+36+1];
      memset(part_name, 0, sizeof(part_name));
      Vprintf::snprintf(&part_name[0], sizeof(part_name), "name:");
      for (unsigned i=0; i<36; i++)
	part_name[i] = (pent.name[i] < 0x80) ? pent.name[i] : '?';

      char part_uuid[5+32+4+1];
      memset(part_uuid, 0, sizeof(part_uuid));
      Vprintf::snprintf(&part_uuid[0], sizeof(part_uuid), "uuid:");
      pent.id.as_text(&part_uuid[5]);

      char part_type[5+32+4+1];
      memset(part_type, 0, sizeof(part_type));
      Vprintf::snprintf(&part_type[0], sizeof(part_type), "type:");
      pent.id.as_text(&part_type[5]);

      DiskProtocol::Segment seg(disknum, pent.first_lba, pent.last_lba - pent.first_lba);
      check1(false, disk->add_logical_disk(*BaseProgram::myutcb(), name, 1, &seg));
    }
    return true;
  }
};

class LogDiskMan : public NovaProgram, ProgramConsole
{
  DiskHelper *disk;

public:
  NORETURN
  int run(Utcb *utcb, Hip *hip)
  {
    init(hip);
    console_init("LogDisk", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    disk = new DiskHelper(alloc_cap(DiskProtocol::CAP_SERVER_PT + hip->cpu_desc_count() + 2),
			  DiskProtocol::CAP_SERVER_PT + hip->cpu_desc_count(), 0);

    unsigned count = 0;
    disk->get_disk_count(*BaseProgram::myutcb(), count);
    if (count == 0)
      Logging::printf("No disks available\n");

    for (unsigned disknum = 0; disknum < count; disknum++) {
      if (Partition::find(disk, disknum)) continue;
      if (GPT::find(disk, disknum)) continue;
    }

    Logging::printf("Done");
    WvTest::exit(0);
    block_forever();
  }
};

char DiskHelper::disk_buffer[4<<20] ALIGNED(0x1000);

bool GPT::header::check_crc()
{
  uint32_t orig_crc = crc, curr_crc;
  crc = 0;
  curr_crc = calc_crc(~0L, reinterpret_cast<const uint8_t*>(this), sizeof(*this)) ^ ~0L;
  crc = orig_crc;
  return curr_crc == orig_crc;
}

void GPT::guid::as_text(char *dest)
{
  const int chunks[] = { -4, -2, -2, 2, 6 };
  const int *chunk = chunks;
  unsigned start = 0, len = 4;
  for (unsigned i = 0; i < 16; i++) {
    if (i == start + len) {
      *dest++ = '-';
      chunk++;
      start = i;
      len = (*chunk > 0) ? *chunk : -*chunk;
    }
    unsigned char ch = (*chunk > 0) ? id[i] : id[start+len - (i - start) - 1];
    Vprintf::snprintf(dest, 3, "%02x", ch);
    dest += 2;
  }
  *dest = '\0';
}

ASMFUNCS(LogDiskMan, WvTest)
