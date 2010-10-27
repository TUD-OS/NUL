/**
 * Sigma0 request interface defintions.
 *
 * Copyright (C) 2008-2010, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
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
#include "nul/error.h"

/**
 * This class defines the call interface to sigma0 services.
 */
class Sigma0Base : public BaseProgram
{
 public:
  enum {
    REQUEST_STDIN_ATTACH = 0x1001,
    REQUEST_DISKS_ATTACH,
    REQUEST_TIMER_ATTACH,
    REQUEST_NETWORK_ATTACH,
    REQUEST_VNET_ATTACH,
    REQUEST_DISK,
    REQUEST_TIMER,
    REQUEST_TIME,
    REQUEST_CONSOLE,
    REQUEST_HOSTOP,
    REQUEST_NETWORK,
    REQUEST_PCICFG,
    REQUEST_ACPI,
    REQUEST_VNET,
  };
 protected:

  template <unsigned OP>
    static unsigned request_attach(Utcb *utcb, void *buffer, unsigned sem_nq)
    {
      TemporarySave<Utcb::HEADER_SIZE + 5> save(utcb);

      utcb->msg[0] = OP;
      utcb->msg[1] = reinterpret_cast<unsigned long>(buffer);
      utcb->set_header(2, 0);
      add_mappings(utcb, sem_nq << Utcb::MINSHIFT, 1 << Utcb::MINSHIFT, MAP_MAP, 0x14 | DESC_TYPE_CAP);
      check1(1, nova_call(14));
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
    utcb->set_header(1 + words, 0);
    if (nova_call(14))
      Logging::printf("sigma0 request failed %x\n", utcb->msg[0]);
    memcpy(&msg,  utcb->msg + 1, words*sizeof(unsigned));
    return utcb->msg[0];
  }
 public:
  struct PutsRequest
  {
    char *buffer;
    PutsRequest(char *_buffer) : buffer(_buffer) {}
  };
  static unsigned  request_stdin         (Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_STDIN_ATTACH>(utcb, buffer, sem_nq); }
  static unsigned  request_disks_attach  (Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_DISKS_ATTACH>(utcb, buffer, sem_nq); }
  static unsigned  request_timer_attach  (Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_TIMER_ATTACH>(utcb, buffer, sem_nq); }
  static unsigned  request_network_attach(Utcb *utcb, void *buffer, unsigned sem_nq) { return request_attach<REQUEST_NETWORK_ATTACH>(utcb, buffer, sem_nq); }
  static unsigned  request_vnet_attach   (Utcb *utcb, unsigned sem_nq) { return request_attach<REQUEST_VNET_ATTACH>(utcb, NULL, sem_nq); }
  static unsigned disk(MessageDisk &msg)     { return sigma0_message<MessageDisk,      REQUEST_DISK>(msg); }
  static unsigned console(MessageConsole &msg)  { return sigma0_message<MessageConsole,   REQUEST_CONSOLE>(msg); }
  static unsigned hostop (MessageHostOp &msg)   { return sigma0_message<MessageHostOp,    REQUEST_HOSTOP>(msg); }
  static unsigned timer  (MessageTimer &msg)    { return sigma0_message<MessageTimer,     REQUEST_TIMER>(msg); }
  static unsigned network(MessageNetwork &msg)  { return sigma0_message<MessageNetwork,   REQUEST_NETWORK>(msg); }
  static unsigned time   (MessageTime &msg)     { return sigma0_message<MessageTime,      REQUEST_TIME>(msg); }
  static unsigned pcicfg (MessagePciConfig &msg){ return sigma0_message<MessagePciConfig, REQUEST_PCICFG>(msg); }
  static unsigned acpi   (MessageAcpi &msg)     { return sigma0_message<MessageAcpi,      REQUEST_ACPI>(msg); }
  static unsigned vnetop (MessageVirtualNet &msg){ return sigma0_message<MessageVirtualNet,REQUEST_VNET>(msg); }
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
struct TimerItem {
  timevalue time;
  TimerItem(timevalue _time = 0) : time(_time) {}
};
typedef Consumer<TimerItem, DISKS_SIZE> TimerConsumer;
typedef Producer<TimerItem, DISKS_SIZE> TimerProducer;

/**
 * Network push interface.
 */
typedef PacketConsumer<NETWORK_SIZE> NetworkConsumer;
typedef PacketProducer<NETWORK_SIZE> NetworkProducer;
