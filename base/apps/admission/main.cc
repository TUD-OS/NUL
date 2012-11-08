/*
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

#include <nul/program.h>
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

#include <nul/compiler.h>
#include <nul/service_admission.h>
#include <nul/timer.h>
#include <nul/service_timer.h>
#include <service/math.h>
#include <service/cmdline.h>

#include <host/keyboard.h>

#include "util/capalloc_partition.h"

#define CONST_CAP_RANGE 16U

namespace ab {

class AdmissionService : public CapAllocatorAtomicPartition<1 << CONST_CAP_RANGE>, public ProgramConsole, public NovaProgram
{
  static char * flag_revoke;

private:

  struct ClientData : public GenericClientData {
    cap_sel statistics;
    char name[32];
    struct {
      cap_sel  idx;
      unsigned cpu;
      unsigned prio;
      unsigned quantum;
      PACKED timevalue last[10];
      char name[32];
    } scs[Config::MAX_CPUS + 32]; //still guessing

    void* operator new(size_t size) {
      assert(size == sizeof(struct ClientData));
      for (unsigned j=0; j < 4; j++) { //XXX
        for (unsigned long i=0; i < MemoryClientData::max; i++) {
          assert(sizeof(MemoryClientData::items[i].used) == 4);
          if (MemoryClientData::items[i].used) continue;
          if (0 != Cpu::cmpxchg4b(&MemoryClientData::items[i].used, 0, 1)) continue;
          //Logging::printf("memalloc - %lu - %p - used %p\n", i, &MemoryClientData::items[i].client, &MemoryClientData::items[i].used);
          return &MemoryClientData::items[i].client;
        }
      }
      return 0;
    }

    void operator delete(void* ptr) {
      unsigned long item = (reinterpret_cast<unsigned long>(ptr) - reinterpret_cast<unsigned long>(MemoryClientData::items)) / sizeof(struct MemoryClientData);
      assert( ptr >= MemoryClientData::items);
      assert(item < MemoryClientData::max);
      assert((reinterpret_cast<unsigned long>(ptr) - reinterpret_cast<unsigned long>(MemoryClientData::items)) % sizeof(struct MemoryClientData) == 0);

      //free kernel resources (SCs) of client
      struct ClientData * data = &MemoryClientData::items[item].client;
      for (unsigned i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
        if (!data->scs[i].idx) continue;

        nova_revoke(Crd(data->scs[i].idx, 0, DESC_CAP_ALL), true); //revoke SC
      }
      nova_revoke(Crd(data->statistics, 0, DESC_CAP_ALL), true); //revoke statistic cap

      //free data structure of client
      MemoryClientData::items[item].used = 0;
      //Logging::printf("memfree - %lu < %lu - %p - used %p\n", item, MemoryClientData::max, &MemoryClientData::items[item].client, &MemoryClientData::items[item].used);
    }
  } PACKED;

  struct MemoryClientData {
    static struct MemoryClientData * items;
    static unsigned long max;
    struct ClientData client;
    unsigned long used;
  } PACKED;

  struct ClientData idle_scs;
  struct ClientData own_scs;

  timevalue global_sum[Config::MAX_CPUS];
  timevalue global_prio[Config::MAX_CPUS][256]; //XXX MAX_PRIO

  ALIGNED(8) ClientDataStorage<ClientData, AdmissionService> _storage;

  bool enable_top;
  bool enable_measure;
  bool enable_log;
  bool enable_verbose;
  unsigned cpu_start, cpu_end;

  unsigned interval;

public:

  AdmissionService() : CapAllocatorAtomicPartition<1 << CONST_CAP_RANGE>(1) {}

  void init_service(Hip * hip) {
    unsigned long long base = alloc_cap_region(1 << CONST_CAP_RANGE, CONST_CAP_RANGE);
    assert(base && !(base & 0xFFFFULL));
    _divider  = hip->cpu_count();
    _cap_base = base;
    enable_verbose = enable_top = enable_measure = enable_log = false;
    cpu_start = 0, cpu_end = ~0U;
  }

  inline unsigned alloc_cap(unsigned num = 1, unsigned cpu = ~0U) { //XXX quirk as long as CapAllocatorAtomic can not handle num > 1
    if (num > 1) return CapAllocator::alloc_cap(num);
    else return CapAllocatorAtomicPartition<1 << CONST_CAP_RANGE>::alloc_cap(num, cpu);
  }
  inline void dealloc_cap(unsigned cap, unsigned count = 1) {
    assert(count == 1); CapAllocatorAtomicPartition<1 << CONST_CAP_RANGE>::dealloc_cap(cap, count);
  }

  inline unsigned alloc_crd() { return Crd(alloc_cap(1, BaseProgram::mycpu()), 0, DESC_CAP_ALL).value(); } //XXX physical cpu number is here used, but logical should be used! XXX//

  #include "top.h"

  void check_clients(Utcb &utcb) {
    ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
    ClientData * data = 0;
    while (data = _storage.get_invalid_client(utcb, this, data)) {
      if (enable_verbose) Logging::printf("ad: found dead client - freeing datastructure\n");
      _storage.free_client_data(utcb, data, this);
    }
  }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap, cap_sel pid)
    {
      unsigned op, res;
      check1(EPROTO, input.get_word(op));

      switch (op) {
      case ParentProtocol::TYPE_OPEN:
      {
        unsigned idx = input.received_cap();
        unsigned cap_session = 0;

        if (enable_verbose && !idx) Logging::printf("  open - invalid cap received\n");
        if (!idx) return EPROTO;

        //check whether we have already a session with this client
        res = ParentProtocol::check_singleton(utcb, idx, cap_session);
        if (!res && cap_session)
        {
         //XXX check whether pseudo is really invalid otherwise unnecessary to do that
          ClientData *data = 0;
          ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
          while (data = _storage.next(data)) {
            if (data->get_identity() == cap_session) {
              dealloc_cap(data->pseudonym); //replace old pseudonym, first pseudonym we got via parent and gets obsolete as soon as client becomes running
              if (enable_verbose) Logging::printf("  open - session rebind pseudo=%x->%x\n", data->pseudonym, idx);
              data->pseudonym = idx;
              utcb << Utcb::TypedMapCap(data->get_identity());
              free_cap = false;
              return ENONE;
            }
          }
        }

        ClientData *data = 0;
        res = _storage.alloc_client_data(utcb, data, idx, this);
        if (enable_verbose && res) Logging::printf("  alloc_client - res %x\n", res);
        if (res == ERESOURCE) { check_clients(utcb); Logging::printf("oh oh - got out of oom - retry\n"); return ERETRY; } //force garbage collection run
        else if (res) return res;
        if (*flag_revoke) { check_clients(utcb); *flag_revoke = 0; }

        res = ParentProtocol::set_singleton(utcb, data->pseudonym, data->get_identity());
        assert(!res);

        free_cap = false;
        if (enable_verbose) Logging::printf("**** created admission client 0x%x 0x%x\n", data->pseudonym, data->get_identity());
        utcb << Utcb::TypedMapCap(data->get_identity());
        return res;
      }
      case ParentProtocol::TYPE_CLOSE:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
        check1(res, res = _storage.get_client_data(utcb, data, input.identity()));
        return _storage.free_client_data(utcb, data, this);
      }
      case AdmissionProtocol::TYPE_SET_NAME:
      {
        ClientData *data = 0;
        ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
        res = _storage.get_client_data(utcb, data, input.identity());
        if (res) return res;

        unsigned len;
        char const * name = input.get_zero_string(len);
        if (!name || !len) return EPROTO;

        memcpy(data->name, name, len > sizeof(data->name) - 1 ? sizeof(data->name) - 1 : len);
        data->name[sizeof(data->name) - 1] = 0;
        return ENONE;
      }
      case AdmissionProtocol::TYPE_SC_ALLOC:
      case AdmissionProtocol::TYPE_SC_PUSH:
        {
          AdmissionProtocol::sched sched;
          unsigned i, cpu = _divider, len, idx = input.received_cap();
          bool self = false;

          check1(EPROTO, !idx);
          check1(EPROTO, input.get_word(sched));
          check1(EPROTO, input.get_word(cpu) && cpu < Global::hip.cpu_desc_count() && Global::hip.cpus()[cpu].enabled()); //check that cpu < number of cpus and that cpu is enabled
          check1(NOVA_ECPU, !(cpu_start <= cpu && cpu <= cpu_end)); //check that we only handle cpus we should

          char const * name = input.get_zero_string(len);
          if (!name) return EPROTO;

          if (op == AdmissionProtocol::TYPE_SC_PUSH)
            check1(EPROTO, input.get_word(self));

          ClientData *data = 0;
          ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
          if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

          if (op == AdmissionProtocol::TYPE_SC_PUSH) {
            timevalue computetime;
            //check for sigma0 - only sigma0 is allowed to push
            if (NOVA_ESUCCESS != nova_ctl_sc(idx, computetime)) return EPROTO;
            if (self) data = &own_scs;
          }

          again:

          for (i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
            if (data->scs[i].idx) continue;
            if (0 != Cpu::cmpxchg4b(&data->scs[i].idx, 0, 0xaffe)) goto again;
            break;
          }
          if (i >= sizeof(data->scs) / sizeof(data->scs[0])) return ERESOURCE;

          if (sched.type < sched.TYPE_APERIODIC || sched.type > sched.TYPE_SYSTEM) sched.type = sched.TYPE_APERIODIC; //sanity check
          data->scs[i].prio    = (op == AdmissionProtocol::TYPE_SC_PUSH) ? sched.type : (sched.type > sched.TYPE_PERIODIC ? sched.TYPE_PERIODIC : sched.type);
          data->scs[i].quantum = Config::DEFAULT_QUANTUM;
          data->scs[i].cpu = cpu;
          memset(data->scs[i].last, 0, sizeof(data->scs[i].last));
          memcpy(data->scs[i].name, name, len > sizeof(data->scs[i].name) - 1 ? sizeof(data->scs[i].name) - 1 : len);
          data->scs[i].name[sizeof(data->scs[i].name) - 1] = 0;

          if (op == AdmissionProtocol::TYPE_SC_PUSH) {
            data->scs[i].idx = idx; //got from outside
            free_cap = false;
          } else {
            unsigned idx_sc = alloc_cap(1, cpu); //XXX physical cpu number is here used, but logical should be used! XXX//
            unsigned char res;
            res = nova_create_sc(idx_sc, idx, Qpd(data->scs[i].prio, data->scs[i].quantum), data->scs[i].cpu);
            if (res != NOVA_ESUCCESS) {
              memset(&data->scs[i], 0, sizeof(data->scs[i]));
              dealloc_cap(idx_sc);
              Logging::printf("create_sc failed %x\n", res);
              return EPROTO;
            }
            data->scs[i].idx = idx_sc;
          }
          if (enable_verbose) Logging::printf("created sc - prio=%u quantum=%u cpu=%u %s.%s\n", data->scs[i].prio, data->scs[i].quantum, data->scs[i].cpu, data->name, data->scs[i].name);
          return ENONE;
        }
        break;
      case AdmissionProtocol::TYPE_GET_USAGE_CAP:
        {
          ClientData *client;
          ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
          if (res = _storage.get_client_data(utcb, client, input.identity())) return res;

          if (!(client->statistics = alloc_cap(1, BaseProgram::mycpu()))) return ERESOURCE; //XXX physical cpu number is here used, but logical should be used! XXX//
          if (NOVA_ESUCCESS != nova_create_sm(client->statistics)) {
            dealloc_cap(client->statistics); client->statistics = 0; return ERESOURCE;
          }

          utcb << Utcb::TypedMapCap(client->statistics);
          return ENONE;
        }
        break;
      case AdmissionProtocol::TYPE_REBIND_USAGE_CAP:
      case AdmissionProtocol::TYPE_SC_USAGE:
        {
          ClientData *caller;
          cap_sel stats;
          unsigned len;

          ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
          if (res = _storage.get_client_data(utcb, caller, input.identity())) return res; //caller
          if (!(stats = input.identity(1))) return EPROTO; //client statistic cap
          char const * name = input.get_zero_string(len);

          //input.dump_typed_items();
          //check whether provided stat cap match to one of our client
          ClientData *client = 0;
          for (client = _storage.next(client); client; client = _storage.next(client))
            if (client->statistics == stats) break;
          if (!client) return EPERM;

          if (op == AdmissionProtocol::TYPE_SC_USAGE) {
            timevalue time_con = 0;
            unsigned res = get_usage(client, time_con, ((!name || (len==0)) ? NULL : name), true);
            if (res == ENONE) utcb << time_con;
            return res;
          } else if (op == AdmissionProtocol::TYPE_REBIND_USAGE_CAP) {
            nova_revoke(Crd(client->statistics, 0, DESC_CAP_ALL), false); //rebind - revoke old mapping
            utcb << Utcb::TypedMapCap(client->statistics);
            return ENONE;
          } else return EPROTO;
        }
        break;
      default:
        return EPROTO;
      }
    }

  /*
   * Calculates subsumed time of all SCs of a client on all CPUs
   */
  unsigned get_usage(ClientData * data, timevalue &time_con, const char * name, bool fresh) {
    unsigned i;

    for (i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
      if (!data->scs[i].idx) continue;
      if (name && strcmp(data->scs[i].name, name)) continue;

      if (!fresh) {
        time_con += data->scs[i].last[0];
        continue;
      }

      timevalue computetime;
      unsigned res = nova_ctl_sc(data->scs[i].idx, computetime);
      if (res != NOVA_ESUCCESS) return EPERM;
      time_con += computetime;
    }
    return ENONE;
  }

  /*
   * Calculates subsumed time of all SCs of a client per CPU.
   * Result is passed on UTCB.
   */
  unsigned get_usage(Utcb & utcb, ClientData volatile * data) {
    unsigned i;

    for (phy_cpu_no cpunr=0; cpunr < Global::hip.cpu_desc_count(); cpunr++) {
      Hip_cpu const *cpu = &Global::hip.cpus()[cpunr];
      if (not cpu->enabled()) continue;

      timevalue time_con = 0;
      bool avail = false;

      for (i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
        if (!data->scs[i].idx || data->scs[i].cpu != cpunr) continue;
        time_con += data->scs[i].last[0] - data->scs[i].last[1];
        avail = true;
      }
      if (!avail) continue;

      timevalue rest;
      splitfloat(time_con, rest, cpunr);

      unsigned _util = time_con;
      unsigned _rest = rest;
      utcb << cpunr << _util << _rest;
    }

    utcb << ~0UL;
    return ENONE;
  }

  bool start_service (Utcb *utcb, Hip * hip)
    {
      flag_revoke = new (0x1000) char[0x1000];
      if (!flag_revoke) return false;

      const char * service_name = "/admission";
      unsigned res;
      unsigned service_cap = alloc_cap();
      Utcb *utcb_wo, *utcb_pf;
      
      memset(&idle_scs, 0, sizeof(idle_scs));
      memset(&own_scs , 0, sizeof(idle_scs));
      memcpy(&own_scs.name, "admission", 9);

      for (unsigned cpunr = cpu_start; cpunr <= cpu_end; cpunr++) {
        Hip_cpu const *cpu = &hip->cpus()[cpunr];
        if (not cpu->enabled()) continue;

        assert(cpunr < sizeof(idle_scs.scs) / sizeof(idle_scs.scs[0]));
        assert(!idle_scs.scs[cpunr].idx);
        idle_scs.scs[cpunr].idx = ParentProtocol::CAP_PT_PERCPU + Config::MAX_CPUS + cpunr;
        idle_scs.scs[cpunr].cpu = cpunr;

        timevalue computetime;
        if (NOVA_ESUCCESS != nova_ctl_sc(idle_scs.scs[cpunr].idx, computetime))
          Logging::panic("Couldn't get idle sc cap - cpu %u, idx %#x\n", idle_scs.scs[cpunr].cpu, idle_scs.scs[cpunr].idx);

        unsigned s0_exc_base = Config::EXC_PORTALS * cpunr;
        unsigned cap_ec = create_ec4pt(this, cpunr, s0_exc_base, &utcb_wo, alloc_cap(1, cpunr)); //XXX physical cpu number is here used, but logical should be used! XXX//
        if (!cap_ec) return false;
        unsigned cap_pf = create_ec4pt(this, cpunr, s0_exc_base, &utcb_pf, alloc_cap(1, cpunr)); //XXX physical cpu number is here used, but logical should be used! XXX//
        if (!cap_pf) return false;

        utcb_wo->head.crd = alloc_crd();
        utcb_wo->head.crd_translate = Crd(_cap_base, CONST_CAP_RANGE, DESC_CAP_ALL).value();
        assert(!(_cap_base & ((1UL << CONST_CAP_RANGE)-1)));
        utcb_pf->head.crd = 0;

        unsigned pt_wo            = alloc_cap(1, cpunr); //XXX physical cpu number is here used, but logical should be used! XXX//
        unsigned long portal_func = reinterpret_cast<unsigned long>(StaticPortalFunc<AdmissionService>::portal_func);
        res = nova_create_pt(pt_wo, cap_ec, portal_func, 0);
        if (res) return false;

        res = ParentProtocol::register_service(*utcb, service_name, cpunr, pt_wo, service_cap, flag_revoke);
        if (res) return !res;
      }

      return true;
    }

  bool run_statistics(Utcb * utcb, Hip * hip) {
    assert(enable_measure);

    Clock * _clock = new Clock(hip->freq_tsc);
    if (!_clock) return false;

    TimerProtocol * timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
    unsigned res = timer_service->timer(*utcb, _clock->abstime(0, 1000));
    if (res) return false;

    KernelSemaphore sem = KernelSemaphore(timer_service->get_notify_sm());

    StdinConsumer stdinconsumer;
    if (enable_top)
      Sigma0Base::request_stdin(utcb, &stdinconsumer, sem.sm());

    {
      ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, *utcb, this);
      measure_scs(hip);
    }

    unsigned update = true;
    unsigned show = 0, client_num = 0, sc_num = 0, num_cpu_shift = 0;

    while (true) {
      if (enable_top) {
        while (stdinconsumer.has_data()) {
          MessageInput *kmsg = stdinconsumer.get_buffer();
          if (!(kmsg->data & KBFLAG_RELEASE)) {
            bool _update = true;
            if ((kmsg->data & 0x7f) == 77) { show = show == 2 ? 0 : 2; } //"p"
            else if ((kmsg->data & 0x7f) == 44) show = 0; //"t"
            else if ((kmsg->data & 0x3ff) == KBCODE_DOWN) {
              if (show == 1) {
                if (sc_num < (sizeof(own_scs.scs) / sizeof(own_scs.scs[0]) - 1)) sc_num ++;
              } else {
                if (show == 0 && client_num) client_num--;
                show = 0;
              }
            }
            else if ((kmsg->data & 0x3ff) == KBCODE_UP) {
              if (show == 1) {
                if (sc_num) sc_num--;
              } else {
                if (show == 0) client_num++;
                show = 0;
              }
            }
            else if ((kmsg->data & 0x3ff) == KBCODE_LEFT) {
              if ((show == 0 || show == 2) && num_cpu_shift) num_cpu_shift--;
            }
            else if ((kmsg->data & 0x3ff) == KBCODE_RIGHT) {
              if ((show == 0 || show == 2) && (num_cpu_shift < hip->cpu_count() - 1)) num_cpu_shift++;
            }
            else if ((kmsg->data & 0x7f) == KBCODE_ENTER) { show = show == 1 ? 0 : 1; if (show == 0) sc_num = 0;}
            else _update = false;
            update = update ? update : _update;
          }
          stdinconsumer.free_buffer();
        }
      }

      {
        ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, *utcb, this);
        unsigned tcount = 0;

        if (!timer_service->triggered_timeouts(*utcb, tcount) && tcount) measure_scs(hip);

        if (enable_top && (update || tcount)) {
          if (show == 0)
            top_dump_scs(*utcb, hip, client_num, num_cpu_shift);
          else if (show == 1)
            top_dump_client(client_num, interval, hip, sc_num);
          else
            top_dump_prio(hip, num_cpu_shift);
        }
      }

      if (timer_service->timer(*utcb, _clock->abstime(interval, 1)))
        Logging::printf("failure - programming timer\n");

      sem.downmulti();
      update = false;
    }
  }

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {
    init(hip);
    init_mem(hip);
    init_service(hip);

    Semaphore sem(alloc_cap(), true);
    LogProtocol log(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    console_init("admission service", &sem);

    interval = 2000; //2s

    char const *cmdline = reinterpret_cast<char *>(hip->get_mod(0)->aux);
    char const *args[16];
    unsigned argv = Cmdline::parse(cmdline, args, sizeof(args)/sizeof(char *));
    for (unsigned i=1; i < argv; i++) {
      if (!memcmp("top", args[i], 3)) enable_top = true;
      if (!memcmp("measure", args[i], 7)) enable_measure = true;
      if (!memcmp("log", args[i], 3)) enable_log = true;
      if (!memcmp("verbose", args[i], 7)) enable_verbose = true;
      if (!memcmp("cpu", args[i], 3)) {
        if (args[i][3] != ':') continue;
        cpu_start = strtoul(&args[i][4], 0, 0);
        unsigned len = strcspn(&args[i][4], ":");
        if (len) cpu_end = strtoul(&args[i][4+len+1], 0, 0);
      }
    }
    enable_measure = enable_measure || enable_top;

    if (enable_log) _console_data.log = &log;

    if (cpu_end >= hip->cpu_desc_count()) cpu_end = hip->cpu_desc_count() - 1;
    if (cpu_start >= hip->cpu_desc_count()) cpu_start = 0;

    Region r = _free_phys.alloc_max(12);
    if (!r.virt || !r.size) Logging::panic("no memory for client data available\n");

    assert(r.virt < (1ULL << 32) && r.size < (1ULL << 32));
    MemoryClientData::items = reinterpret_cast<struct MemoryClientData *>(r.virt);
    MemoryClientData::max   = r.size;
    MemoryClientData::max   = MemoryClientData::max / (sizeof(struct MemoryClientData));

    Logging::printf("admission service: log=%s, measure=%s, top=%s, verbose=%s, max clients=%lu, handle cpus=%u-%u\n",
                    enable_log ? "yes" : "no", enable_measure ? "yes" : "no",
                    enable_top ? "yes" : "no", enable_verbose ? "yes" : "no", MemoryClientData::max, cpu_start, cpu_end);

    if (!start_service(utcb, hip))
      Logging::printf("failure - starting admission service\n");

    if (enable_measure && !run_statistics(utcb, hip))
      Logging::printf("failure - running statistic loop\n");

    block_forever();
  }
};

  char *   AdmissionService::flag_revoke;
  unsigned AdmissionService::cursor_pos;
  unsigned long AdmissionService::MemoryClientData::max;
  struct AdmissionService::MemoryClientData * AdmissionService::MemoryClientData::items;

} /* namespace */

ASMFUNCS(ab::AdmissionService, NovaProgram)

