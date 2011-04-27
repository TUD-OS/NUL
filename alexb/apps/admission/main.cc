/*
 * (C) 2011 Alexander Boettcher
 *     economic rights: Technische Universitaet Dresden (Germany)
 *
 * This is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This is distributed in the hope that it will be useful,
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

#include <host/keyboard.h>

namespace ab {
class AdmissionService : public NovaProgram, public ProgramConsole
{
  
  enum {
    VALUEWIDTH = 2U,
    WIDTH  = 80U,
    HEIGHT = 25U,
  };

  static char * flag_revoke;

  private:
    enum {
      CAP_REGION_START = 0x2000,
      CAP_REGION_ORDER = 13,
    };
    RegionList<512> * cap_range;

    struct ClientData : public GenericClientData {
      bool statistics;
      char name[32];
      struct {
        unsigned idx;
        unsigned cpu;
        unsigned prio;
        unsigned quantum;
        timevalue m_last1;
        timevalue m_last2;
        char name[32];
      } scs[32];
    };

    struct ClientData idle_scs; 
    struct ClientData own_scs;

    timevalue global_sum[Config::MAX_CPUS];
    timevalue global_prio[Config::MAX_CPUS][256]; //XXX MAX_PRIO

    ALIGNED(8) ClientDataStorage<ClientData, AdmissionService> _storage;

  public:

  void init_service() {
    cap_range = new RegionList<512>;
    assert(0 == (CAP_REGION_START & (1 << CAP_REGION_ORDER) - 1));
    cap_range->add(Region(CAP_REGION_START, 1 << CAP_REGION_ORDER));
  }

  inline unsigned alloc_cap(unsigned num = 1) { return cap_range->alloc(num, 0); } ///XXX locking !
  inline unsigned alloc_crd() { return Crd(alloc_cap(), 0, DESC_CAP_ALL).value(); }
  inline void dealloc_cap(unsigned cap, unsigned count = 1) { cap_range->add(Region(cap, count));} ///XXX locking !

  #include "top.h"

  void check_clients(Utcb &utcb) {
    ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
    ClientData volatile * data = _storage.get_invalid_client(utcb, this);
    while (data) {
      Logging::printf("ad: found dead client - freeing datastructure\n");
      _storage.free_client_data(utcb, data, this);
      data = _storage.get_invalid_client(utcb, this, data);
    }
  }

  unsigned portal_func(Utcb &utcb, Utcb::Frame &input, bool &free_cap)
    {
      unsigned op, res;
      check1(EPROTO, input.get_word(op));

      switch (op) {
      case ParentProtocol::TYPE_OPEN:
      {
        unsigned idx = input.received_cap();
        unsigned cap_session = 0;

        if (!idx) return EPROTO;

        //check whether we have already a session with this client
        res = ParentProtocol::check_singleton(utcb, idx, cap_session);
        if (!res && cap_session)
        {
          ClientData volatile *data = 0;
          ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
          while (data = _storage.next(data)) {
            if (data->identity == cap_session) {
              dealloc_cap(data->pseudonym); //replace old pseudonym, first pseudnym we got via parent and gets obsolete as soon as client becomes running
              data->pseudonym = idx;
              utcb << Utcb::TypedMapCap(data->identity);
              free_cap = false;
              return ENONE;
            }
          }
        }

        ClientData *data = 0;
        res = _storage.alloc_client_data(utcb, data, idx, this);
        if (res == ERESOURCE) { check_clients(utcb); return ERETRY; } //force garbage collection run
        else if (res) return res;
        if (*flag_revoke) { check_clients(utcb); *flag_revoke = 0; }

        res = ParentProtocol::set_singleton(utcb, data->pseudonym, data->identity);
        assert(!res);

        free_cap = false;
        //Logging::printf("----- created admission client 0x%x 0x%x\n", data->pseudonym, data->identity);
        utcb << Utcb::TypedMapCap(data->identity);
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
          unsigned i, cpu, len, idx_ec = input.received_cap();
          bool self = false;

          check1(EPROTO, !idx_ec);
          check1(EPROTO, input.get_word(sched));
          check1(EPROTO, input.get_word(cpu));
          char const * name = input.get_zero_string(len);
          if (!name) return EPROTO;

          if (op == AdmissionProtocol::TYPE_SC_PUSH)
            check1(EPROTO, input.get_word(self));

          ClientData *data = 0;
          ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
          if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

          //check for sigma0 (first data item) - only sigma0 is allowed to push
          if (op == AdmissionProtocol::TYPE_SC_PUSH && data->next) return EPROTO;
          if (op == AdmissionProtocol::TYPE_SC_PUSH && self) data = &own_scs;

          //XXX make it atomic
          for (i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
            if (data->scs[i].idx) continue;
            data->scs[i].idx = 0xaffe;
            break;
          }
          if (i >= sizeof(data->scs) / sizeof(data->scs[0])) return ERESOURCE;
          //XXX make it atomic

          //XXX
/*          
          if (op != AdmissionProtocol::TYPE_SC_PUSH) {
            if (prio > 1)
            {
              prio    = 1;
              quantum = 1000;
            } else if (quantum > 10000) {
              quantum = 10000;
            }
          }
*/
          data->scs[i].prio    = !data->next ? sched.type : (sched.type > sched.TYPE_PERIODIC ? sched.TYPE_PERIODIC : sched.type);
          data->scs[i].quantum = 10000U;
          data->scs[i].cpu = cpu;
          data->scs[i].m_last1 = data->scs[i].m_last2 = 0;
          memcpy(data->scs[i].name, name, len > sizeof(data->scs[i].name) - 1 ? sizeof(data->scs[i].name) - 1 : len);
          data->scs[i].name[sizeof(data->scs[i].name) - 1] = 0;

          if (op == AdmissionProtocol::TYPE_SC_PUSH) {
            data->scs[i].idx = idx_ec; //got from outside
            free_cap = false;
          } else {
            unsigned idx_sc = alloc_cap();
            unsigned char res;
            //XXX security bug -- force ec to be on right cpu //XXX
            res = nova_create_sc(idx_sc, idx_ec, Qpd(data->scs[i].prio, data->scs[i].quantum));
            if (res != NOVA_ESUCCESS) {
              memset(&data->scs[i], 0, sizeof(data->scs[i]));
              dealloc_cap(idx_sc);
              Logging::printf("create_sc failed %x\n", res);
              return EPROTO;
            }
            data->scs[i].idx = idx_sc;
          }
          //Logging::printf("created sc - prio=%u quantum=%u cpu=%u\n", data->scs[i].prio, data->scs[i].quantum, data->scs[i].cpu);
          return ENONE;
        }
        break;
      case AdmissionProtocol::TYPE_SC_USAGE:
      {
        ClientData *caller, * client;
        ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
        if (res = _storage.get_client_data(utcb, caller, input.identity())) return res; //caller
//        if (!caller->statistics) return EPERM;
//        input.dump_typed_items();
        if (res = _storage.get_client_data(utcb, client, input.identity(1))) return res; //the client you want to have statistics about
        return get_usage(utcb, client);
      }
      default:
        return EPROTO;
      }
    }

  bool start_service (Utcb *utcb, Hip * hip)
    {
      flag_revoke = new (0x1000) char[0x1000];
      if (!flag_revoke) return false;

      const char * service_name = "/admission";
      unsigned res;
      unsigned exc_base_wo, exc_base_pf, pt_wo, pt_pf, num_cpus = 0;
      unsigned service_cap = alloc_cap();
      Utcb *utcb_wo, *utcb_pf;
      
      memset(&idle_scs, 0, sizeof(idle_scs));
      memset(&own_scs , 0, sizeof(idle_scs));
      memcpy(&own_scs.name, "admission", 9);

      for (unsigned cpunr = 0; cpunr < hip->cpu_count(); cpunr++) {
        Hip_cpu *cpu = reinterpret_cast<Hip_cpu *>(reinterpret_cast<char *>(hip) + hip->cpu_offs + cpunr*hip->cpu_size);
        if (~cpu->flags & 1) continue;

        idle_scs.scs[cpunr].idx = 512 + cpunr;
        idle_scs.scs[cpunr].cpu = cpunr;

        exc_base_wo = cap_range->alloc(16,4);
        exc_base_pf = cap_range->alloc(16,4);
        if (!exc_base_wo || !exc_base_pf) return false;
        pt_wo       = alloc_cap();
        pt_pf       = exc_base_wo + 0xe;

        unsigned cap_ec = create_ec_helper(this, num_cpus, exc_base_wo, &utcb_wo, 0, alloc_cap());
        if (!cap_ec) return false;
        unsigned cap_pf = create_ec_helper(this, num_cpus, exc_base_pf, &utcb_pf, 0, alloc_cap());
        if (!cap_pf) return false;

        utcb_wo->head.crd = alloc_crd();
        utcb_wo->head.crd_translate = Crd(CAP_REGION_START, CAP_REGION_ORDER, DESC_TYPE_CAP).value();
        utcb_pf->head.crd = 0;

        unsigned long portal_func = reinterpret_cast<unsigned long>(StaticPortalFunc<AdmissionService>::portal_func);
        res = nova_create_pt(pt_wo, cap_ec, portal_func, 0);
        if (res) return false;
        res = ParentProtocol::register_service(*utcb, service_name, num_cpus, pt_wo, service_cap, flag_revoke);
        if (res) return !res;
        num_cpus ++;
      }

      return true;
    }

  bool run_statistics(Utcb * utcb, Hip * hip) {
    Clock * _clock = new Clock(hip->freq_tsc);
    if (!_clock) return false;

    TimerProtocol * timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_SERVER_PT + hip->cpu_count()));
    TimerProtocol::MessageTimer msg(_clock->abstime(0, 1000));
    unsigned res = timer_service->timer(*utcb, msg);
    if (res) return false;

    KernelSemaphore sem = KernelSemaphore(timer_service->get_notify_sm());

    StdinConsumer stdinconsumer;
    Sigma0Base::request_stdin(utcb, &stdinconsumer, sem.sm());

    {
      ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, *utcb, this);
      measure_scs(hip);
    }

    unsigned update = true;
    unsigned show = 0, client_num = 0;
    unsigned timeout = 2000; //2s

    while (true) {
      while (stdinconsumer.has_data()) {
        MessageInput *kmsg = stdinconsumer.get_buffer();
        if (!(kmsg->data & KBFLAG_RELEASE)) {
          bool _update = true;
          if ((kmsg->data & 0x7f) == 77) { show = show == 2 ? 0 : 2; } //"p"
          else if ((kmsg->data & 0x7f) == 44) show = 0; //"t"
          else if ((kmsg->data & 0x3ff) == KBCODE_DOWN) { if (show == 0 && client_num) client_num--; show = 0; }
          else if ((kmsg->data & 0x3ff) == KBCODE_UP)   { if (show == 0 && client_num < HEIGHT - 2) client_num++; show = 0; }
          else if ((kmsg->data & 0x7f) == KBCODE_ENTER) { show = show == 1 ? 0 : 1; }
          else _update = false;
          update = update ? update : _update;
        }
        stdinconsumer.free_buffer();
      }

      {
        ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, *utcb, this);
        unsigned tcount;

        if (!timer_service->triggered_timeouts(*utcb, tcount) && tcount) measure_scs(hip);

        if (update || tcount) {
          if (show == 0)
            top_dump_scs(*utcb, hip, client_num);
          else if (show == 1)
            top_dump_client(client_num);
          else
            top_dump_prio(hip);
        }
      }

      TimerProtocol::MessageTimer to(_clock->abstime(timeout, 1));
      if (timer_service->timer(*utcb,to)) Logging::printf("failure - programming timer\n");

      sem.downmulti();
      update = false;
    }
  }
 
  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);
    init_service();

    console_init("admission service");
//    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_count()));

    if (!start_service(utcb, hip))
      Logging::printf("failure - starting admission service\n");

    if (!run_statistics(utcb, hip))
      Logging::printf("failure - running statistic loop\n");

    block_forever();
  }
};

  char *   AdmissionService::flag_revoke;
  unsigned AdmissionService::cursor_pos;
} /* namespace */

ASMFUNCS(ab::AdmissionService, NovaProgram)

