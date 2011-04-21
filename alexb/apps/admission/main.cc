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

namespace ab {
class AdmissionService : public NovaProgram, public ProgramConsole
{
  
  enum {
    VALUEWIDTH = 2,
    WIDTH  = 80,
    HEIGHT = 25,
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
      } scs[32];
    };
    struct ClientData idle_scs; 
    struct ClientData own_scs;

    timevalue global_sum[Config::MAX_CPUS];

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

  void get_idle(Hip * hip) {
    cursor_pos = 0;

    for (unsigned cpu=0; cpu < hip->cpu_count(); cpu++) {
      timevalue time_con;
      unsigned res = get_usage(&idle_scs, cpu, time_con);
      assert(!res);

      unsigned util = 0;
      if (time_con && global_sum[cpu]) {
        time_con *= 100;
        Math::div64(time_con, global_sum[cpu]);
        util = time_con;
      }
      Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH, "%3u%%", util);
      cursor_pos -= 1;
    }
    memset(_vga_console + VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH, 0, 1);
    cursor_pos = 0;
  }

  unsigned get_usage(Utcb & utcb, ClientData * data) {
    unsigned res, cpu;

    for (cpu=0; cpu < 32 ; cpu++) { //XXX CPU count
      timevalue time_con;
      res = get_usage(data, cpu, time_con);
      if (res == ERESOURCE) continue;
      if (res) return res;

      unsigned util = 0;
      if (time_con && global_sum[cpu]) {
        time_con *= 100;
        Math::div64(time_con, global_sum[cpu]);
        util = time_con;
      }

      utcb << cpu << util;
    }

    utcb << ~0UL;
    return ENONE;
  }

  unsigned get_usage(ClientData volatile * data, unsigned cpu, timevalue &time_consumed)
  {
    unsigned i, res = ERESOURCE;

    time_consumed = 0;
    for (i=0; i < sizeof(data->scs) / sizeof(data->scs[0]); i++) {
      if (!data->scs[i].idx || data->scs[i].cpu != cpu) continue;
      res = ENONE;

      timevalue computetime;
      unsigned res = nova_ctl_sc(data->scs[i].idx, computetime);
      if (res != NOVA_ESUCCESS) return EPERM;

      timevalue diff        = computetime - data->scs[i].m_last1;
      time_consumed        += diff;
      global_sum[cpu]      += diff;
      global_sum[cpu]      -= data->scs[i].m_last1 - data->scs[i].m_last2;
      data->scs[i].m_last2  = data->scs[i].m_last1;
      data->scs[i].m_last1  = computetime;
    }
    return res;
  }

  void dump_scs(Utcb &utcb, Hip * hip) {

    ClientData volatile * data = &own_scs;
    ClientData * client;
    unsigned client_count = HEIGHT - 1, res, more = 0;
    data->next = _storage.next();

    do {
      utcb.head.mtr = 1; //manage utcb by hand (alternative using add_frame, drop_frame which copies unnessary here)
      cursor_pos = 0;
      client = reinterpret_cast<ClientData *>(reinterpret_cast<unsigned long>(data));

      res = get_usage(utcb, client);
      if (client_count > 1) {
        memset(_vga_console + client_count * VALUEWIDTH * WIDTH, 0, VALUEWIDTH * WIDTH);
        cursor_pos = 5 * (1 + hip->cpu_count());
        Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH, "%s", client->name);
        memset(_vga_console + client_count * VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH - 1, 0, 1);
        if (res != ENONE) {
          Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH, "%s", "error");
          continue;
        }

        unsigned i = 1;
        while (i < sizeof(utcb.msg) / sizeof(utcb.msg[0]) && utcb.msg[i] != ~0UL) {
          cursor_pos = utcb.msg[i] * 5;
          Vprintf::printf(&_putc, _vga_console + client_count * VALUEWIDTH * WIDTH, "%3u%%", utcb.msg[i+1]);
          memset(_vga_console + client_count * VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH - 1, 0, 1);
          i += 2;
        }
        client_count --;
      } else {
        more += 1;
        cursor_pos = 0;
        Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH * HEIGHT - 12 * VALUEWIDTH, "...%u", more);
      }
    } while (data = _storage.next(data));

    get_idle(hip);
  }

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
          unsigned i, cpu, idx_ec = input.received_cap();
          bool self = false;

          check1(EPROTO, !idx_ec);
          check1(EPROTO, input.get_word(sched));
          check1(EPROTO, input.get_word(cpu));
          if (op == AdmissionProtocol::TYPE_SC_PUSH)
            check1(EPROTO, input.get_word(self));

          ClientData *data = 0;
          ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, utcb, this);
          if (res = _storage.get_client_data(utcb, data, input.identity())) return res;

          //XXX check that only sigma0 can call us (last data)
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
          data->scs[i].prio    = sched.type;
          data->scs[i].quantum = 10000U;
          data->scs[i].cpu = cpu;
          data->scs[i].m_last1 = data->scs[i].m_last2 = 0;

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

    unsigned timeout = 3000; //3s

    cursor_pos = 0;
    for (unsigned i=0; i < hip->cpu_count(); i++) {
      Vprintf::printf(&_putc, _vga_console, "%3u ", i);
      cursor_pos -=1;
    }
    memset(_vga_console + cursor_pos * 2, 0, 1);
    cursor_pos = 5 * (1 + hip->cpu_count());
    Vprintf::printf(&_putc, _vga_console + VALUEWIDTH * WIDTH, "idle");
    memset(_vga_console + VALUEWIDTH * WIDTH + cursor_pos * VALUEWIDTH - 1, 0, 1);
    cursor_pos = 0;

    while (true) {
      TimerProtocol::MessageTimer to(_clock->abstime(timeout, 1));
      if (timer_service->timer(*utcb,to)) Logging::printf("failure - programming timer\n");

      sem.downmulti();
      {
        ClientDataStorage<ClientData, AdmissionService>::Guard guard_c(&_storage, *utcb, this);
        dump_scs(*utcb, hip);
      }
    }
  }
 
  static unsigned cursor_pos;
  static void _putc(void * data, int value) {
    value &= 0xff;
    if (value == -1 || value == - 2) return; // -1 start, -2 end, can be used for locking
    Screen::vga_putc(0xf00 | value, reinterpret_cast<unsigned short *>(data), cursor_pos);
  }

  NORETURN
  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);
    init_service();

    console_init("admission service");
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_count()));

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

