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

#include "sigma0/consumer.h"
#include "service/helper.h"
#include "nul/message.h"
#include "nul/baseprogram.h"

/**
 * This class defines the call interface to sigma0 services.
 */
class Sigma0Base : public BaseProgram
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
    REQUEST_PCICFG,
    REQUEST_ACPI
  };
 protected:

  template <unsigned OP>
    static unsigned request_attach(Utcb *utcb, void *buffer, unsigned sem_nq)
    {
      TemporarySave<Utcb::HEADER_SIZE + 5> save(utcb);

      utcb->msg[0] = OP;
      utcb->msg[1] = reinterpret_cast<unsigned long>(buffer);
      utcb->head.mtr    = Mtd(2, 0);
      add_mappings(utcb, false, sem_nq << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, 0, 0x18 | DESC_TYPE_CAP);
      check1(1, nova_call(14, utcb->head.mtr));
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
    if (nova_call(14, Mtd(1 + words, 0)) && OP != REQUEST_PUTS)
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
  static unsigned  request_stdin         (Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_STDIN_ATTACH>(utcb, buffer, sem_nq); }
  static unsigned  request_disks_attach  (Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_DISKS_ATTACH>(utcb, buffer, sem_nq); }
  static unsigned  request_timer_attach  (Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_TIMER_ATTACH>(utcb, buffer, sem_nq); }
  static unsigned  request_network_attach(Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_NETWORK_ATTACH>(utcb, buffer, sem_nq); }
  static bool disk   (MessageDisk &msg)     { return sigma0_message<MessageDisk,      REQUEST_DISK>(msg); }
  static bool console(MessageConsole &msg)  { return sigma0_message<MessageConsole,   REQUEST_CONSOLE>(msg); }
  static bool hostop (MessageHostOp &msg)   { return sigma0_message<MessageHostOp,    REQUEST_HOSTOP>(msg); }
  static bool timer  (MessageTimer &msg)    { return sigma0_message<MessageTimer,     REQUEST_TIMER>(msg); }
  static bool network(MessageNetwork &msg)  { return sigma0_message<MessageNetwork,   REQUEST_NETWORK>(msg); }
  static bool time   (MessageTime &msg)     { return sigma0_message<MessageTime,      REQUEST_TIME>(msg); }
  static bool pcicfg (MessagePciConfig &msg){ return sigma0_message<MessagePciConfig, REQUEST_PCICFG>(msg); }
  static bool acpi   (MessageAcpi &msg)     { return sigma0_message<MessageAcpi,      REQUEST_ACPI>(msg); }

};


/**
 * Push interface sizes.
 */
enum {
  STDIN_SIZE = 32,
  DISKS_SIZE = 32,
  TIMER_SIZE = 32,
  NETWORK_SIZE = 1 << (20 - 2)
};

/**
 * Stdin push interface.
 */
typedef Consumer<MessageInput, STDIN_SIZE> StdinConsumer;
typedef Producer<MessageInput, STDIN_SIZE> StdinProducer;


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
