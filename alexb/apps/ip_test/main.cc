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
#include <nul/service_config.h> //ConfigService
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>
#include "nul/service_log.h"

extern "C" void lwip_init();
extern "C" void nul_lwip_input(void * data, unsigned size);
extern "C" bool nul_lwip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac); 
extern "C" void nul_lwip_dhcp_start(void);
extern "C" unsigned long long nul_lwip_netif_next_timer(void);
extern "C" void nul_lwip_udp_test(void);
extern "C" void nul_lwip_tcp_test(void);
extern "C" bool get_ip_addr(void);

namespace ab {

class TestLWIP : public NovaProgram, public ProgramConsole
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
      Clock * _clock = new Clock(hip->freq_tsc);

      NetworkConsumer * netconsumer = new NetworkConsumer();
      if (!netconsumer)
        return false;

      TimerProtocol *_timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_NUM));
      TimerProtocol::MessageTimer msg(_clock->abstime(0, 1000));
      res = _timer_service->timer(*utcb, msg);
      Logging::printf("request timer attach - %s\n",
                      (res == 0 ? "success" : "failure"));
      if (res)
        return false;

      KernelSemaphore sem = KernelSemaphore(_timer_service->get_notify_sm());

      res = Sigma0Base::request_network_attach(utcb, netconsumer, sem.sm());
      Logging::printf("request network attach - %s\n",
                      (res == 0 ? "success" : "failure"));
      if (res)
        return false;


	    MessageHostOp msg_op(MessageHostOp::OP_GET_MAC, 0UL);
      res = Sigma0Base::hostop(msg_op);
      Logging::printf("got mac %02llx:%02llx:%02llx:%02llx:%02llx:%02llx - %s\n",
                      (msg_op.mac >> 40) & 0xFF,
                      (msg_op.mac >> 32) & 0xFF,
                      (msg_op.mac >> 24) & 0xFF,
                      (msg_op.mac >> 16) & 0xFF,
                      (msg_op.mac >> 8) & 0xFF,
                      (msg_op.mac) & 0xFF,
                      (res == 0 ? "success" : "failure"));

      unsigned long long mac = ((0ULL + Math::htonl(msg_op.mac)) << 32 | Math::htonl(msg_op.mac >> 32)) >> 16;

      unsigned long long timeout = nul_lwip_netif_next_timer();
//      Logging::printf("info    - next timeout in %llu ms\n", timeout);

      TimerProtocol::MessageTimer to(_clock->time() + timeout * hip->freq_tsc);
      if (_timer_service->timer(*utcb, to)) Logging::panic("failed  - starting timer\n");

      lwip_init();
      if (!nul_lwip_init(send_network, mac)) Logging::panic("failed - starting lwip\n");

      nul_lwip_dhcp_start();

      nul_lwip_udp_test();
      nul_lwip_tcp_test();

      while (1) {
        unsigned char *buf;
        unsigned tcount = 0;

        sem.downmulti();

//        Logging::printf("time r:w %x:%x, netconsumer r:w %x:%x\n", tmconsumer->_rpos, tmconsumer->_wpos, netconsumer->_rpos, netconsumer->_wpos);
        //check whether timer triggered
        if (!_timer_service->triggered_timeouts(*utcb, tcount) && tcount) {
          unsigned long long timeout = nul_lwip_netif_next_timer();
//          Logging::printf("info    - next timeout in %llu ms\n", timeout);

          TimerProtocol::MessageTimer to(_clock->time() + timeout * hip->freq_tsc);
          if (_timer_service->timer(*utcb,to))
            Logging::printf("failed  - starting timer\n");

          get_ip_addr();
        }

        while (netconsumer->has_data()) {
          unsigned size = netconsumer->get_buffer(buf);
          nul_lwip_input(buf, size);
          netconsumer->free_buffer();
        }
      }

      if (res)
        return false;

      return true;
    }

  bool use_config (Utcb *utcb, Hip * hip)
    {
      char *args = reinterpret_cast<char *>(hip->get_mod(0)->aux);
      char * name = strstr(args, "rom://");
      if (!name) return false;
      name +=6;
      char * end = strstr(name, " ");
      if (!end) return false;
      char * dir = strstr(name, "/nova/bin/test_lwip.nul");
      if (!dir || dir > end) return false;

      char const * const_config [] = {
        "/nova/bin/vancouver.nul.gz sigma0::log sigma0::mem:256 82576vf_vnet PC_PS2 sigma0::dma sigma0::log name::/s0/log name::/s0/timer name::/s0/fs/rom || ",
        "/nova/tools/munich || ",
        "/nova/linux/bzImage-js clocksource=tsc || ",
        "/nova/linux/initrd-js.lzma",
        0
      };
      char * _config = new (0x1000) char[0x1000];
      char const * config = _config;
      unsigned i = 0;

      while (const_config[i]) {
        memcpy(_config, "rom://", 6); _config += 6;
        memcpy(_config, name, dir - name); _config += dir - name;
        memcpy(_config, const_config[i], strlen(const_config[i])); _config += strlen(const_config[i]);
        i ++;
      }
      *_config = 0;

      Logging::printf("cmdline %lx %s\n", strlen(config), config);

      ConfigProtocol *service_config = new ConfigProtocol(alloc_cap(ConfigProtocol::CAP_NUM));

      return (!service_config->start_config(*utcb, config));
    }

  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("LWIP test");
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_NUM));

    Logging::printf("Hello\n");

    _virt_phys.debug_dump("");

    if (!use_network(utcb, hip, alloc_cap()))
      Logging::printf("failed  - starting lwip stack\n");

//    if (!use_config(utcb, hip)) 
//      Logging::printf("failed  - starting config service\n");
  }
};

} /* namespace */

ASMFUNCS(ab::TestLWIP, NovaProgram)
