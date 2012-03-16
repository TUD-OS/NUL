/**
 * @file 
 * Disk access helper class.
 *
 * Copyright (C) 2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NUL (NOVA user land).
 *
 * NUL is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * NUL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "nul/service_disk.h"

/**
 * Disk access helper class.
 *
 * It encapsulates disk DMA buffer and provides synchronous read
 * operation.
 */
template <class C, unsigned S = 4<<20 >
class DiskHelper : public DiskProtocol {
public:
  static char disk_buffer[S];
  DiskHelper(CapAllocator* a, unsigned instance) : DiskProtocol(a, instance) {
    unsigned res;
    cap_sel sem_cap = a->alloc_cap();
    cap_sel tmp_cap = a->alloc_cap();

    KernelSemaphore *sem = new KernelSemaphore(sem_cap, true);
    DiskProtocol::DiskConsumer *diskconsumer = new (1<<12) DiskProtocol::DiskConsumer();
    assert(diskconsumer);

    res = attach(*BaseProgram::myutcb(), disk_buffer, sizeof(disk_buffer), tmp_cap,
		 diskconsumer, sem);
    if (res) Logging::panic("DiskProtocol::attach failed: %d\n", res);
  }

  unsigned read_write_synch(bool read, unsigned disknum, uint64 start_sector, size_t size) {
    DmaDescriptor dma;
    unsigned res;

    dma.byteoffset = 0;
    dma.bytecount  = size;
    assert(size <= sizeof(disk_buffer));
    res = read_write(*BaseProgram::myutcb(), read, disknum, /*usertag*/0, start_sector, /*dmacount*/1, &dma);
    if (res)
      return res;

    sem->downmulti();
    assert(consumer->has_data());
    MessageDiskCommit *msg = consumer->get_buffer();
    assert(msg->usertag == 0);
    consumer->free_buffer();
    return ENONE;
  }

  unsigned read_synch(unsigned disknum, uint64 start_secotr, size_t size) {
    return read_write_synch(true, disknum, start_secotr, size);
  }

  unsigned write_synch(unsigned disknum, uint64 start_secotr, size_t size) {
    return read_write_synch(false, disknum, start_secotr, size);
  }
};

// Static member "definition"
template <class C, unsigned S>
char DiskHelper<C,S>::disk_buffer[S] ALIGNED(0x1000);
