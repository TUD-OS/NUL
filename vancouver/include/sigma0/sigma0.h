/*
 * Sigma0 request interface defintions.
 *
 * Copyright (C) 2008, Bernhard Kauer <bk@vmmon.org>
 *
 * This file is part of Vancouver.
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
#pragma once

#include "sys/program.h"
#include "sys/consumer.h"
#include "models/dma.h"
#include "vmm/message.h"


/**
 * This class defines the call interface to sigma0 services.
 */
class Sigma0Base : public NovaProgram
{
 public:
  enum {
    REQUEST_PUTS   = 0x1001,
    REQUEST_STDIN_ATTACH,
    REQUEST_DISKS_ATTACH,
    REQUEST_TIMER_ATTACH,
    REQUEST_NETWORK_ATTACH,
    REQUEST_DISK,
    REQUEST_TIMER,
    REQUEST_TIME,
    REQUEST_CONSOLE,
    REQUEST_HOSTOP,
    REQUEST_NETWORK,
    REQUEST_IOIO,
    REQUEST_IOMEM,
    REQUEST_IRQ,
  };
 protected:

  template <unsigned OP> 
    static unsigned request_attach(void *buffer, unsigned sem_nq)
    {
      Utcb *utcb = myutcb();
      TemporarySave<Utcb::HEADER_SIZE + 5> save(utcb);
      
      utcb->msg[0] = OP;
      utcb->msg[1] = reinterpret_cast<unsigned long>(buffer);
      utcb->head.mtr    = Mtd(3, 0);
      utcb->add_mappings(false, sem_nq << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, 0, 3);
      check(idc_call(14, utcb->head.mtr));
      return utcb->msg[0];
    }

  template <class MESSAGE, unsigned OP>
  static unsigned sigma0_message(MESSAGE &msg)
  {
    Utcb *utcb = myutcb();
    assert(utcb);
    const unsigned words = (sizeof(msg) +  sizeof(unsigned) - 1) / sizeof(unsigned);
    TemporarySave<Utcb::HEADER_SIZE + 1 + words> save(utcb);
    utcb->msg[0] = OP;
    memcpy(utcb->msg + 1, &msg,  words*sizeof(unsigned));
    if (idc_call(14, Mtd(1 + words, 0)))
      Logging::printf("sigma0 request failed %x\n", utcb->msg[0]);
    memcpy(&msg,  utcb->msg + 1, words*sizeof(unsigned));
    return !utcb->msg[0];
  }
 public:
  struct PutsRequest
  {
    char *buffer;
    PutsRequest(char *_buffer) : buffer(_buffer) {}
  };
  static unsigned  puts(char *buffer) {  PutsRequest req(buffer); return sigma0_message<PutsRequest, REQUEST_PUTS>(req); }
  static unsigned  request_stdin         (void *buffer, unsigned sem_nq) { return request_attach<REQUEST_STDIN_ATTACH>(buffer, sem_nq); }
  static unsigned  request_disks_attach  (void *buffer, unsigned sem_nq) { return request_attach<REQUEST_DISKS_ATTACH>(buffer, sem_nq); }
  static unsigned  request_timer_attach  (void *buffer, unsigned sem_nq) { return request_attach<REQUEST_TIMER_ATTACH>(buffer, sem_nq); }
  static unsigned  request_network_attach(void *buffer, unsigned sem_nq) { return request_attach<REQUEST_NETWORK_ATTACH>(buffer, sem_nq); }


  static unsigned request_irq(unsigned irq)
  {
    Utcb *utcb = myutcb();
    TemporarySave<Utcb::HEADER_SIZE + 5> save(utcb);

    utcb->head.mtr = Mtd(1, 0);
    utcb->msg[0] = REQUEST_IRQ;
    utcb->add_mappings(false, irq << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, 0, 3);
    utcb->head.crd = utcb->msg[2];
    return idc_call(14, Mtd(3, 0));
  }

  static unsigned request_io(unsigned long base, unsigned long size, bool io, unsigned long &iomem_start)
  {
    Utcb *utcb = myutcb();
    TemporarySave<Utcb::HEADER_SIZE + 5> save(utcb);
    utcb->head.mtr = Mtd(1, 0);
    if (io)
      {
	utcb->msg[0] = REQUEST_IOIO;
	size <<= Utcb::MINSHIFT;
	base <<= Utcb::MINSHIFT;
      }
    else
      {
	if (size < 0x1000)  size = 0x1000;
	utcb->msg[0] = REQUEST_IOMEM;
      }
    Logging::printf("request_io(%lx, %lx, utcb %p)\n", base, size, utcb);
    utcb->add_mappings(false, base, size, 0, io ? 2 : 1);
    utcb->head.crd = io ? utcb->msg[2] : (iomem_start | (Cpu::bsr(size) << 5) | 1);
    
    unsigned res = idc_call(14, Mtd(3,0));
    if (!res && !io)
      {
	res = iomem_start;
	iomem_start += size;
	return res;
      }
    else
      return 0;
  }
  static bool disk(MessageDisk &msg)       { return sigma0_message<MessageDisk,    REQUEST_DISK>(msg); }
  static bool console(MessageConsole &msg) { return sigma0_message<MessageConsole, REQUEST_CONSOLE>(msg); }
  static bool hostop(MessageHostOp &msg)   { return sigma0_message<MessageHostOp,  REQUEST_HOSTOP>(msg); }
  static bool timer(MessageTimer &msg)     { return sigma0_message<MessageTimer,   REQUEST_TIMER>(msg); }
  static bool network(MessageNetwork &msg) { return sigma0_message<MessageNetwork, REQUEST_NETWORK>(msg); }
  static bool time(MessageTime &msg)       { return sigma0_message<MessageTime,    REQUEST_TIME>(msg); }
};


/**
 * Push interface sizes.
 */
enum {
  STDIN_SIZE = 32,
  DISKS_SIZE = 32,
  TIMER_SIZE = 32,
  NETWORK_SIZE = 1 << (16 - 2),
};

/**
 * Stdin push interface.
 */
typedef Consumer<MessageKeycode, STDIN_SIZE> StdinConsumer;
typedef Producer<MessageKeycode, STDIN_SIZE> StdinProducer;


/**
 * Disk push interface.
 */
typedef Consumer<MessageDiskCommit, DISKS_SIZE> DiskConsumer;
typedef Producer<MessageDiskCommit, DISKS_SIZE> DiskProducer;


/**
 * Timer push interface.
 */
struct TimerItem{
  unsigned long long time;
  TimerItem(unsigned long long _time = 0) : time(_time) {}
};
typedef Consumer<TimerItem, DISKS_SIZE> TimerConsumer;
typedef Producer<TimerItem, DISKS_SIZE> TimerProducer;

/**
 * Network push interface.
 */
typedef PacketConsumer<NETWORK_SIZE> NetworkConsumer;
typedef PacketProducer<NETWORK_SIZE> NetworkProducer;
