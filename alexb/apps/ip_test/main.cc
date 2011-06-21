/*
 * (C) 2010 Alexander Boettcher
 *     economic rights: Technische Universitaet Dresden (Germany)
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
#include <nul/timer.h> //clock
#include <nul/service_timer.h> //TimerService
#include <nul/service_log.h>
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

extern "C" void nul_ip_input(void * data, unsigned size);
extern "C" bool nul_ip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac); 
extern "C" bool nul_ip_config(unsigned para, void * arg);

enum {
  IP_NUL_VERSION  = 0,
  IP_DHCP_START   = 1,
  IP_IPADDR_DUMP  = 2,
  IP_TIMEOUT_NEXT = 3,
  IP_UDP_OPEN     = 4,
  IP_TCP_OPEN     = 5,
  IP_SET_ADDR     = 6,
  IP_TCP_SEND     = 7
};

namespace ab {

class TestIP : public NovaProgram, public ProgramConsole
{
  public:

    static void send_network(char unsigned const * data, unsigned len) {
      bool res;

      MessageNetwork net = MessageNetwork(data, len, 0);

      res = Sigma0Base::network(net);
      if (res)
      Logging::printf("%s - sending packet to network, len = %u\n", 
                      (res == 0 ? "success" : "failure"), len);
    }

    bool use_network(Utcb *utcb, Hip * hip, unsigned sm) {
      bool res;
      unsigned long long arg = 0;
      Clock * _clock = new Clock(hip->freq_tsc);

      if (!nul_ip_config(IP_NUL_VERSION, &arg) || arg != 0x1) return false;

      NetworkConsumer * netconsumer = new NetworkConsumer();
      if (!netconsumer) return false;

      TimerProtocol * timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
      TimerProtocol::MessageTimer msg(_clock->abstime(0, 1000));
      res = timer_service->timer(*utcb, msg);

      Logging::printf("request timer attach - %s\n", (res == 0 ? "success" : "failure"));
      if (res) return false;

      KernelSemaphore sem = KernelSemaphore(timer_service->get_notify_sm());
      res = Sigma0Base::request_network_attach(utcb, netconsumer, sem.sm());

      Logging::printf("request network attach - %s\n", (res == 0 ? "success" : "failure"));
      if (res) return false;

      MessageNetwork msg_op(MessageNetwork::QUERY_MAC, 0);
      res = Sigma0Base::network(msg_op);
      Logging::printf("got mac %02llx:%02llx:%02llx:%02llx:%02llx:%02llx - %s\n",
                      (msg_op.mac >> 40) & 0xFF, (msg_op.mac >> 32) & 0xFF,
                      (msg_op.mac >> 24) & 0xFF, (msg_op.mac >> 16) & 0xFF,
                      (msg_op.mac >> 8) & 0xFF, (msg_op.mac) & 0xFF,
                      (res == 0 ? "success" : "failure"));

      unsigned long long mac = ((0ULL + Math::htonl(msg_op.mac)) << 32 | Math::htonl(msg_op.mac >> 32)) >> 16;

      if (!nul_ip_config(IP_TIMEOUT_NEXT, &arg)) Logging::panic("failed - requesting timeout\n");

      TimerProtocol::MessageTimer to(_clock->time() + arg * hip->freq_tsc);
      if (timer_service->timer(*utcb, to)) Logging::panic("failed  - starting timer\n");
      if (!nul_ip_init(send_network, mac)) Logging::panic("failed - starting ip\n");
      if (!nul_ip_config(IP_DHCP_START, NULL)) Logging::panic("failed - starting dhcp\n");

      unsigned long port[2] = { 5555, 0 };
      if (!nul_ip_config(IP_UDP_OPEN, &port[0])) Logging::panic("failed - creating udp port\n");
      port[0] = 7777;
      port[1] = 0;
      if (!nul_ip_config(IP_TCP_OPEN, &port[0])) Logging::panic("failed - creating tcp port\n");

      while (1) {
        unsigned char *buf;
        unsigned tcount = 0;

        sem.downmulti();

        //check whether timer triggered
        if (!timer_service->triggered_timeouts(*utcb, tcount) && tcount) {
          unsigned long long timeout;
          nul_ip_config(IP_TIMEOUT_NEXT, &timeout);
          //Logging::printf("info    - next timeout in %llu ms\n", timeout);

          TimerProtocol::MessageTimer to(_clock->time() + timeout * hip->freq_tsc);
          if (timer_service->timer(*utcb,to))
            Logging::printf("failed  - starting timer\n");

          //dump ip addr if we got one
          nul_ip_config(IP_IPADDR_DUMP, NULL);
        }

        while (netconsumer->has_data()) {
          unsigned size = netconsumer->get_buffer(buf);
          nul_ip_input(buf, size);
          netconsumer->free_buffer();
        }
      }

      return !res;
    }

  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("IP test", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    Logging::printf("Hello\n");

    _virt_phys.debug_dump("");

    if (!use_network(utcb, hip, alloc_cap()))
      Logging::printf("failed  - starting ip stack\n");

  }
};

} /* namespace */

ASMFUNCS(ab::TestIP, NovaProgram)
