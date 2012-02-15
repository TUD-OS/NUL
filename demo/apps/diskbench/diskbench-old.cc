/*
 * Disk benchmarking.
 *
 * Copyright (C) 2010, Bernhard Kauer <bk@vmmon.org>
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
#include "nul/motherboard.h"
#include "sigma0/console.h"
#include "sigma0/sigma0.h"
#include "host/dma.h"
#include "wvtest.h"

char disk_buffer[4<<20];
unsigned long blocksize = 512;
unsigned outstanding=5;
bool wvtest = false;
bool lorem_ipsum = false;
PARAM_HANDLER(blocksize,
	      "blocksize:value - override the default blocksize", "Example: 'blocksize:65536'")
{
  blocksize = argv[0];
  if (blocksize > sizeof(disk_buffer)) {
    blocksize = sizeof(disk_buffer);
    Logging::printf("limited the blocksize to %ld\n", blocksize);
  }
}

PARAM_HANDLER(outstanding,
	      "set the number of outstanding requests",
	      "Example: 'outstanding:4'")
{
  outstanding = argv[0];
  if (outstanding > DISKS_SIZE) {
    outstanding = DISKS_SIZE;
    Logging::printf("limited the number of outstanding requests to %d\n", outstanding);
  }
}

PARAM_HANDLER(wvtest) {wvtest = true;}
PARAM_HANDLER(lorem_ipsum) {lorem_ipsum = true;}

class App : public NovaProgram, ProgramConsole
{
  enum {
    FREQ    = 1000,
    TIMEOUT = FREQ,
  };
  unsigned requests;
  unsigned requests_done;
  void submit_disk() {
    DmaDescriptor dma;
    dma.byteoffset = 0;
    dma.bytecount  = blocksize;
    MessageDisk msg1(MessageDisk::DISK_READ, /*disknum*/0, /*usertag*/requests++, /*sector*/0, /*dmacount*/1, &dma,
		     reinterpret_cast<unsigned long>(disk_buffer), sizeof(disk_buffer));
    unsigned res;
    if ((res = Sigma0Base::disk(msg1)))
      Logging::panic("submit(%ld) failed: %x err %x\n", blocksize, res, msg1.error);
  }

public:
  NORETURN
  int run(Utcb *utcb, Hip *hip)
  {
    init(hip);
    console_init("Disk Benchmark", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
    Logging::printf("Benchmark up and running\n");

    Motherboard *mb = new Motherboard(new Clock(hip->freq_tsc*1000), hip);
    mb->parse_args(reinterpret_cast<const char *>(hip->get_mod(0)->aux));

    KernelSemaphore *sem = new KernelSemaphore(alloc_cap(), true);
    DiskConsumer *diskconsumer = new DiskConsumer();
    assert(diskconsumer);
    unsigned res = Sigma0Base::request_disks_attach(utcb, diskconsumer, sem->sm());
    assert(res == ENONE);

    DiskParameter params;
    MessageDisk msg0(0, &params);
    if (Sigma0Base::disk(msg0)) Logging::panic("get params failed");
    Logging::printf("DISK flags %x sectors %lld ssize %d maxreq %d name '%s'\n", params.flags, params.sectors, params.sectorsize, params.maxrequestcount, params.name);

    unsigned  req_nr = requests_done;
    unsigned print = 0;
    timevalue start = mb->clock()->clock(FREQ);

    // prefill the buffer
    while (requests - requests_done < outstanding) submit_disk();

    while (1) {
      sem->downmulti();
      while (diskconsumer->has_data()) {

	MessageDiskCommit *msg = diskconsumer->get_buffer();
	if (msg->status) Logging::panic("request %lx returned error %x\n", msg->usertag, msg->status);
	requests_done++;
	// consume the request
        diskconsumer->free_buffer();

	// check for a timeout
	timevalue now = mb->clock()->clock(FREQ);
	if (now - start > TIMEOUT) {
	  unsigned long long throughput = (requests_done-req_nr) * blocksize;
	  Math::div64(throughput, now - start);
	  unsigned request_rate = (requests_done-req_nr)*FREQ/TIMEOUT;
	  Logging::printf("Speed: %lld kb/s Request: %d/s\n", throughput, request_rate);
	  if (lorem_ipsum) {
	    char dataOnDisk[20];
	    memset(dataOnDisk, 0, sizeof(dataOnDisk));
	    memcpy(dataOnDisk, disk_buffer, 11);
	    WVPASSEQ(dataOnDisk, "Lorem ipsum");
	  }
	  if (wvtest && ++print == 2) {
	    WVPERF(throughput, "kB/s"); 
	    WVPERF(request_rate, "1/s");
	    WvTest::exit(0);
	    block_forever();
	  }
	  req_nr = requests_done;
	  start = now;
	}
	// submit the next request
	submit_disk();
      }
    }
  }
};

ASMFUNCS(App, WvTest)
