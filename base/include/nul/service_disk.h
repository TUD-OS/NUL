/** @file
 * Client part of the disk protocol.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
 * Copyright (C) 2011-2012, Michal Sojka <sojka@os.inf.tu-dresden.de>
 * Copyright (C) 2011-2012, Alexander Boettcher <boettcher@tudos.org>
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
#pragma once

#include "parent.h"
#include "host/dma.h"
#include "sigma0/consumer.h"
#include <nul/types.h>
#include <nul/program.h>
#include <nul/message.h>

#define DISK_SERVICE_IN_S0

/**
 * Client part of the disk protocol.
 * Missing: register shared memory producer/consumer.
 */
struct DiskProtocol : public GenericNoXlateProtocol {
  enum {
    MAXDISKREQUESTS    = 32  // max number of outstanding disk requests per client
  };
  enum {
    TYPE_GET_PARAMS = ParentProtocol::TYPE_GENERIC_END,
    TYPE_GET_DISK_COUNT,
    TYPE_READ,
    TYPE_WRITE,
    TYPE_FLUSH_CACHE,
    TYPE_GET_COMPLETION,
    TYPE_GET_MEM_PORTAL,
    TYPE_DMA_BUFFER,
    TYPE_ADD_LOGICAL_DISK,
    TYPE_CHECK_NAME,
    TYPE_GET_STATS,
  };

  struct Segment {
    unsigned disknum;
    uint64 start_lba, len_lba;
    Segment() {}
    Segment(unsigned disknum, uint64 start, uint64 len) : disknum(disknum), start_lba(start), len_lba(len) {}
  };

  struct Stats {
    uint64 read, written;       ///< Statistics in bytes
    Stats() : read(0), written(0) {}
  };

  typedef Consumer<MessageDiskCommit, DiskProtocol::MAXDISKREQUESTS> DiskConsumer;
  typedef Producer<MessageDiskCommit, DiskProtocol::MAXDISKREQUESTS> DiskProducer;

  DiskConsumer *consumer;
  KernelSemaphore *sem;
  char *dma_buffer;
  size_t dma_size;

  unsigned get_params(Utcb &utcb, unsigned disk, DiskParameter *params) {
    unsigned res;
    if (!(res = call_server_keep(init_frame(utcb, TYPE_GET_PARAMS) << disk)))
      if (utcb >> *params)  res = EPROTO;
    utcb >> *params;
    utcb.drop_frame();
    return res;
  }

  unsigned get_disk_count(Utcb &utcb, unsigned &count) {
    unsigned res;
    res = call_server_keep(init_frame(utcb, TYPE_GET_DISK_COUNT));
    if (res == ENONE)
      if (utcb >> count) res = EPROTO;
    utcb.drop_frame();
    return res;
  }

  unsigned attach(Utcb &utcb, void *dma_buffer, size_t dma_size, cap_sel tmp_cap,
		  DiskConsumer *consumer, KernelSemaphore *notify_sem) {
    unsigned backup_crd = utcb.head.crd;
    unsigned res;

    assert((reinterpret_cast<unsigned long>(dma_buffer) & ((1ul<<12)-1)) == 0);
    assert((reinterpret_cast<unsigned long>(consumer) & ((1<<12)-1)) == 0);
    assert(sizeof(*consumer) < 1<<12);

    this->consumer = consumer;
    this->sem = notify_sem;

    /* Delegate sempahore and request portal capability for memory delegation */
    check2(err, call_server_drop(init_frame(utcb, TYPE_GET_MEM_PORTAL)
                                 << Utcb::TypedMapCap(notify_sem->sm()) << Crd(tmp_cap, 0, DESC_CAP_ALL)));

    /* Delegate the memory via the received portal */
    utcb.add_frame();
#ifdef DISK_SERVICE_IN_S0
    utcb << dma_size;
    utcb << Utcb::TypedTranslateMem(consumer, 0);
    res = utcb.add_mappings(reinterpret_cast<mword>(dma_buffer), dma_size, reinterpret_cast<mword>(dma_buffer), DESC_MEM_ALL, true);
    assert(res == ENONE);
#else
#warning TODO
    utcb << Utcb::TypedDelegateMem(consumer, 0);
    res = utcb.add_mappings(reinterpret_cast<mword>(dma_buffer), dma_size, hotstop | MAP_MAP, DESC_MEM_ALL, true);
    assert(res = ENONE);
#endif
    res = nova_call(tmp_cap);
    utcb.drop_frame();
    this->dma_buffer = reinterpret_cast<char*>(dma_buffer);
    this->dma_size = dma_size;

  err:
    utcb.head.crd = backup_crd;
    return res;
  }

  unsigned read_write(Utcb &utcb, bool read, unsigned disk, unsigned long usertag, unsigned long long sector,
		unsigned dmacount, DmaDescriptor *dma)
  {
    init_frame(utcb, read ? TYPE_READ : TYPE_WRITE) << disk << usertag << sector << dmacount;
    for (unsigned i=0; i < dmacount; i++)  utcb << dma[i];
    return call_server_drop(utcb);
  }

  unsigned read(Utcb &utcb, unsigned disk, unsigned long usertag, unsigned long long sector, unsigned dmacount, DmaDescriptor *dma)
  { return read_write(utcb, true, disk, usertag, sector, dmacount, dma); }

  unsigned write(Utcb &utcb, unsigned disk, unsigned long usertag, unsigned long long sector, unsigned dmacount, DmaDescriptor *dma)
  { return read_write(utcb, false, disk, usertag, sector, dmacount, dma); }

  unsigned flush_cache(Utcb &utcb, unsigned disk) {
    return call_server_drop(init_frame(utcb, TYPE_FLUSH_CACHE) << disk);
  }


  unsigned get_completion(Utcb &utcb, unsigned &tag, unsigned &status) {
    unsigned res;
    if (!(res = call_server_keep(init_frame(utcb, TYPE_GET_COMPLETION))))
      if (utcb >> tag || utcb >> status)  res = EPROTO;
    utcb.drop_frame();
    return res;
  }

  unsigned add_logical_disk(Utcb &utcb, const char* name, unsigned num_segments, Segment *segments) {
    init_frame(utcb, TYPE_ADD_LOGICAL_DISK) << 1u << Utcb::String(name);
    for (unsigned i=0; i < num_segments; i++) utcb << *segments++;
    return call_server_drop(utcb);
  }

  unsigned add_logical_disk(Utcb &utcb, const char* names[], unsigned num_segments, Segment *segments) {
    unsigned i = 0;
    while (names[i]) i++;
    init_frame(utcb, TYPE_ADD_LOGICAL_DISK) << i;
    for (i=0; names[i]; i++)
      utcb << Utcb::String(names[i]);
    for (unsigned i=0; i < num_segments; i++) utcb << *segments++;
    return call_server_drop(utcb);
  }

  /// Check whether a disk is known under a specific name. If it is,
  /// match is set to true, otherwise it is set to false.
  unsigned check_name(Utcb &utcb, unsigned disk, const char *name, bool &match) {
    unsigned res;
    if (!(res = call_server_keep(init_frame(utcb, TYPE_CHECK_NAME) << disk << Utcb::String(name)))) {
      match = utcb.msg[1];
      utcb.drop_frame();
    }
    return res;
  }

  unsigned get_stats(Utcb &utcb, unsigned disk, Stats &stats) {
    unsigned res;
    if (!(res = call_server_keep(init_frame(utcb, TYPE_GET_STATS) << disk))) {
      utcb >> stats;
      utcb.drop_frame();
    }
    return res;
  }

  DiskProtocol(CapAllocator *a, unsigned instance)
    : GenericNoXlateProtocol("disk", instance, a->alloc_cap(DiskProtocol::CAP_SERVER_PT + Global::hip.cpu_desc_count()), false,
                             a->alloc_cap(Global::hip.cpu_desc_count())) {}

  void destroy(Utcb &utcb, CapAllocator *a) {
    GenericNoXlateProtocol::destroy(utcb, DiskProtocol::CAP_SERVER_PT + Global::hip.cpu_desc_count(), a);
    a->dealloc_cap(_session_base, Global::hip.cpu_desc_count());
  }
};
