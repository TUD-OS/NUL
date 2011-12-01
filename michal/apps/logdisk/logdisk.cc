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

class Partition {
  enum {
    SECTOR_SHIFT = 9L,
    SECTOR_SIZE = 1L << SECTOR_SHIFT,
  };

  struct partition {
    char status;
    char start_chs[3];
    char type;
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
  static unsigned add_extended_partitions(DiskProtocol *disk, unsigned disknum, struct partition *e)
  {
    DmaDescriptor dma;
    unsigned res;
    unsigned num = 5;
    struct mbr ebr;
    struct partition *p = NULL;
    assert(e->start_lba);
    do {
      dma.byteoffset = 0;
      dma.bytecount  = SECTOR_SIZE;
      uint64_t start = e->start_lba + (p ? p->start_lba : 0);
      res = disk->read(*BaseProgram::myutcb(), disknum, /*usertag*/0, /*sector*/start, /*dmacount*/1, &dma);
      if (res) return res;
      
      disk->sem->downmulti();
      assert(disk->consumer->has_data());
      MessageDiskCommit *msg = disk->consumer->get_buffer();
      assert(msg->usertag == 0);
      disk->consumer->free_buffer();

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


  static unsigned find(DiskProtocol *disk)
  {
    unsigned count = 0;
    disk->get_disk_count(*BaseProgram::myutcb(), count);
    for (unsigned disknum = 0; disknum < count; disknum++) {
      DmaDescriptor dma;
      unsigned res;
      dma.byteoffset = 0;
      dma.bytecount  = SECTOR_SIZE;
      res = disk->read(*BaseProgram::myutcb(), disknum, /*usertag*/0, /*sector*/0, /*dmacount*/1, &dma);
      if (res) return res;
      
      disk->sem->downmulti();
      assert(disk->consumer->has_data());
      MessageDiskCommit *msg = disk->consumer->get_buffer();
      assert(msg->usertag == 0);
      disk->consumer->free_buffer();

      struct mbr mbr = *reinterpret_cast<struct mbr*>(disk->dma_buffer);
      if (mbr.mbr_signature != 0xaa55)
	continue;
      for (unsigned i=0; i<4; i++) {
	struct partition *p = &mbr.part[i];
	if (p->type) {
	  if (p->start_lba && p->num_sectors) {
	    char name[20];
	    Vprintf::snprintf(name, sizeof(name), "%dp%d", disknum, i+1);
	    DiskProtocol::Segment seg(disknum, p->start_lba, p->num_sectors);
	    check1(res, res = disk->add_logical_disk(*BaseProgram::myutcb(), name, 1, &seg));

	    switch (p->type) {
	    case 0x5: 		// Extended CHS
	    case 0xE:		// Extended LBA
	      check1(res, res = add_extended_partitions(disk, disknum, p));
	    }
	  } else
	    Logging::printf("Non-LBA partition %d on disk %d? Skipping.\n", i, disknum);
	}
      }
    }
    return ENONE;
  }
};

class LogDiskMan : public NovaProgram, ProgramConsole
{
  static char disk_buffer[4<<20];
  DiskProtocol *disk;

public:
  NORETURN
  int run(Utcb *utcb, Hip *hip)
  {
    unsigned res;
    init(hip);
    console_init("LogDisk", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    KernelSemaphore *sem = new KernelSemaphore(alloc_cap(), true);
    DiskProtocol::DiskConsumer *diskconsumer = new (1<<12) DiskProtocol::DiskConsumer();
    assert(diskconsumer);

    disk = new DiskProtocol(alloc_cap(DiskProtocol::CAP_SERVER_PT + hip->cpu_desc_count()), 0);
    res = disk->attach(*utcb, disk_buffer, sizeof(disk_buffer), alloc_cap(),
		       diskconsumer, sem);
    if (res) Logging::panic("disk->attach failed: %d\n", res);

    Partition::find(disk);

    WvTest::exit(0);
    block_forever();
  }
};

char LogDiskMan::disk_buffer[4<<20] ALIGNED(0x1000);

ASMFUNCS(LogDiskMan, WvTest)
