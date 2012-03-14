/**
 * @file Logical Disk Manager.
 *
 * Scans supported logical disks (e.g. partitions) and makes them
 * available to other programs as ordinary disks.
 *
 * Copyright (C) 2011, 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
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
#include <nul/types.h>
#include "crc32.cc"
#include "nul/uuid.h"
#include "nul/disk_helper.h"

class LogDiskMan;

typedef DiskHelper<LogDiskMan> MyDiskHelper;

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
    uint32 start_lba;
    uint32 num_sectors;
  } __attribute__ ((packed));

  struct mbr {
    char code[440];
    uint32 disk_signature;
    uint16 zero;
    struct partition part[4];
    uint16 mbr_signature;
  } __attribute__ ((packed));

  static_assert(sizeof(mbr) == SECTOR_SIZE, "Wrong size of MBR");

public:
  static unsigned add_extended_partitions(MyDiskHelper *disk, unsigned disknum, struct partition *e)
  {
    unsigned res;
    unsigned num = 5;
    struct mbr ebr;
    struct partition *p = NULL;
    assert(e->start_lba);
    do {
      uint64 start = e->start_lba + (p ? p->start_lba : 0);
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


  static bool find(MyDiskHelper *disk, unsigned disknum)
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
  struct header {
    char     signature[8];
    uint32   revision;
    uint32   hsize;
    uint32   crc;
    uint32   reserved;
    uint64   current_lba;
    uint64   backup_lba;
    uint64   first_lba;
    uint64   last_lba;
    uuid_t   disk_guid;
    uint64   pent_lba;
    uint32   pent_num;
    uint32   pent_size;
    uint32   crc_part;

    bool check_crc();
  } __attribute__((packed));

  typedef uint16 utf16le;

  struct pent { // Partition entry
    enum { // attributes
      SYSTEM = 1ull<<0,
      BIOS_BOOTABLE = 1ull<<2,
      READ_ONLY = 1ull<<60,
      HIDDEN = 1ull<<62,
      NO_AUTOMOUNT = 1ull<<63,
    };
    uuid_t   type;
    uuid_t   id;
    uint64   first_lba;
    uint64   last_lba;
    uint64   attr;
    utf16le  name[36];
  } __attribute__((packed));

public:

  static bool find(MyDiskHelper *disk, unsigned disknum)
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

    skip_if(gpt.crc_part != (calc_crc(~0L, reinterpret_cast<const uint8*>(disk->dma_buffer), gpt.pent_num*gpt.pent_size) ^ ~0L),
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
	part_name[5+i] = (pent.name[i] < 0x80) ? pent.name[i] : '?';

      char part_uuid[5+32+4+1];
      memset(part_uuid, 0, sizeof(part_uuid));
      Vprintf::snprintf(&part_uuid[0], sizeof(part_uuid), "uuid:");
      static_cast<Uuid>(pent.id).as_text(&part_uuid[5]);

      char part_type[5+32+4+1];
      memset(part_type, 0, sizeof(part_type));
      Vprintf::snprintf(&part_type[0], sizeof(part_type), "type:");
      static_cast<Uuid>(pent.type).as_text(&part_type[5]);

      DiskProtocol::Segment seg(disknum, pent.first_lba, pent.last_lba - pent.first_lba);
      const char *names[] = { name, part_uuid, part_type, part_name, 0 };
      check1(false, disk->add_logical_disk(*BaseProgram::myutcb(), names, 1, &seg));
    }
    return true;
  }
#undef skip_if
};

#include "lvm.cc"

class LogDiskMan : public NovaProgram, ProgramConsole
{
  MyDiskHelper *disk;

public:
  NORETURN
  int run(Utcb *utcb, Hip *hip)
  {
    init(hip);
    console_init("LogDisk", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    disk = new MyDiskHelper(this, 0);

    unsigned count = 0;
    disk->get_disk_count(*BaseProgram::myutcb(), count);
    if (count == 0)
      Logging::printf("No disks available\n");

    for (unsigned disknum = 0; disknum < count; disknum++) {
      if (Partition::find(disk, disknum)) continue;
      if (GPT::find(disk, disknum)) continue;
    }

#if 0
    disk->get_disk_count(*BaseProgram::myutcb(), count);
    for (unsigned disknum = 0; disknum < count; disknum++) {
      Lvm::find(disk, disknum);
    }
#endif

    Logging::printf("Done");
    WvTest::exit(0);
    block_forever();
  }
};

bool GPT::header::check_crc()
{
  uint32 orig_crc = crc, curr_crc;
  crc = 0;
  curr_crc = calc_crc(~0L, reinterpret_cast<const uint8*>(this), sizeof(*this)) ^ ~0L;
  crc = orig_crc;
  return curr_crc == orig_crc;
}

ASMFUNCS(LogDiskMan, WvTest)
