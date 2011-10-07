/*
 * Copyright (C) 2011, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
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

#include <nul/program.h>
#include <sigma0/sigma0.h>
#include <sigma0/console.h>

#include <nul/generic_service.h>
#include <util/capalloc_partition.h>
#include <nul/service_bridge.h>

enum {
  CONST_CAP_RANGE = 16U,
};

class BridgeService : public CapAllocatorAtomicPartition<1 << CONST_CAP_RANGE>  {
  enum {
    MAX_CLIENTS      = (1 << Config::MAX_CLIENTS_ORDER),
  };

  ALIGNED(4096) struct ClientData : public GenericClientData {
    uint8                                            rx_ring[BridgeProtocol::RING_BUFFER_SIZE];
    PacketProducer<BridgeProtocol::RING_BUFFER_SIZE> rx_prod;
  };

  typedef ClientDataStorage<ClientData, BridgeService> CData;
  ALIGNED(4096) CData _storage;

  bool enable_verbose;

  void check_clients(Utcb &utcb) {
    CData::Guard guard_c(&_storage, utcb, this);
    ClientData volatile * data = _storage.get_invalid_client(utcb, this);
    while (data) {
      Logging::printf("br: found dead client - freeing datastructure\n");
      _storage.free_client_data(utcb, data, this);
      data = _storage.get_invalid_client(utcb, this, data);
    }
  }

public:
  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap)
  {
    unsigned op, res;
    check1(EPROTO, input.get_word(op));

    switch (op) {
    case ParentProtocol::TYPE_OPEN:
      {
        unsigned idx = input.received_cap();

        if (enable_verbose && !idx) Logging::printf("  open - invalid cap received\n");
        if (!idx) return EPROTO;

        ClientData *data = 0;
        res = _storage.alloc_client_data(utcb, data, idx, this);
        if (enable_verbose && res) Logging::printf("  alloc_client - res %x\n", res);
        if (res == ERESOURCE) { check_clients(utcb); return ERETRY; } //force garbage collection run
        else if (res != ENONE) return res;

        PacketConsumer<BridgeProtocol::RING_BUFFER_SIZE> *rx_con = reinterpret_cast<PacketConsumer<BridgeProtocol::RING_BUFFER_SIZE> *>(data->rx_ring);
        data->rx_prod = PacketProducer<BridgeProtocol::RING_BUFFER_SIZE>(rx_con, data->identity);

        data->rx_prod.produce(reinterpret_cast<const unsigned char *>("HELLO"), 6);

        free_cap = false;
        if (enable_verbose) Logging::printf("**** created bridge client 0x%x 0x%x\n", data->pseudonym, data->identity);
        utcb << Utcb::TypedMapCap(data->identity);
        return res;
      }
    case ParentProtocol::TYPE_CLOSE:
      {
        ClientData *data = 0;
        CData::Guard guard_c(&_storage, utcb, this);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
        return _storage.free_client_data(utcb, data, this);
      }

    case BridgeProtocol::TYPE_GET_RING:
      {
        ClientData *data = 0;
        CData::Guard guard_c(&_storage, utcb, this);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
        if (enable_verbose) Logging::printf("**** mapping ring 0x%x 0x%x 0x%x\n", data->pseudonym, data->identity, data->rx_ring);

        unsigned s = BaseProgram::add_mappings(&utcb, reinterpret_cast<mword>(data->rx_ring), BridgeProtocol::RING_BUFFER_SIZE, 0, DESC_MEM_ALL);
        // Make this a normal error?
        assert(s == 0);

        return ENONE;
      }
    default:
      Logging::printf("unknown proto\n");
      return EPROTO;
    }
      
  }

  unsigned alloc_cap(unsigned num = 1, unsigned cpu = ~0U) {
    unsigned cap, cap_last, first_cap;

    first_cap = CapAllocatorAtomicPartition::alloc_cap(1, cpu);
    if (!first_cap) return 0;
    cap_last = first_cap;

    while (--num) { //XXX fix me 
      cap = CapAllocatorAtomicPartition::alloc_cap(1, cpu);
      assert(cap);
      assert(cap_last + 1 == cap);
      cap_last = cap;
    } while (--num);

    return first_cap;
  }

  unsigned alloc_crd() { return Crd(alloc_cap(), 0, DESC_CAP_ALL).value(); }

  template <class C>
    unsigned start_service (Utcb *utcb, Hip * hip, const char *service_name, C * c)
  {
    unsigned res;
    unsigned exc_base_wo;
    unsigned pt_wo;
    unsigned service_cap = alloc_cap();
    Utcb *utcb_wo;
      
    for (unsigned cpunr = 0; cpunr < hip->cpu_desc_count(); cpunr++) {
      Hip_cpu const *cpu = &hip->cpus()[cpunr];
      if (not cpu->enabled()) continue;

      exc_base_wo = alloc_cap(16);
      if (!exc_base_wo) return ERESOURCE;
      pt_wo       = alloc_cap();

      unsigned cap_ec = c->create_ec4pt(this, cpunr, exc_base_wo, &utcb_wo, alloc_cap());
      if (!cap_ec) return ERESOURCE;;

      utcb_wo->head.crd = alloc_crd();
      utcb_wo->head.crd_translate = Crd(_cap_base, CONST_CAP_RANGE, DESC_CAP_ALL).value();

      unsigned long portal_func = reinterpret_cast<unsigned long>(StaticPortalFunc<BridgeService>::portal_func);
      res = nova_create_pt(pt_wo, cap_ec, portal_func, 0);
      if (res) return res;

      Logging::printf("Announcing service on CPU%u", cpunr);
      res = ParentProtocol::register_service(*utcb, service_name, cpunr, pt_wo, service_cap, NULL);
      Logging::printf(" %u\n", res);
      if (res != ENONE) return res;
    }

    return ENONE;
  }

    
 public:
 
  BridgeService() : CapAllocatorAtomicPartition<1 << CONST_CAP_RANGE>(1), enable_verbose(true) {
    unsigned long long base = alloc_cap_region(1 << CONST_CAP_RANGE, 12);
    assert(base && !(base & 0xFFFULL));
    _cap_base = base;


  }


 };

class Bridge : public NovaProgram, public ProgramConsole
{
  friend class BridgeService;
public:

  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("Bridge", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    Logging::printf("booting - bridge ...\n");

    BridgeService *b = new(4096) BridgeService();
    unsigned res = b->start_service(utcb, hip, "/bridge", this);
    switch (res) {
    case ENONE:
      Logging::printf("Service registered.\n");
      break;
    case EPERM:
      Logging::printf("EPERM - check your config file.\n");
      break;
    default:
      Logging::printf("Error %u\n", res);
      break;
    };

  }
};

ASMFUNCS(Bridge, NovaProgram)

// EOF
