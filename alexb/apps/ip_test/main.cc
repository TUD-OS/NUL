/*
 * (C) 2010 Alexander Boettcher
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
#include <nul/timer.h> //clock
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

extern "C" void lwip_init();
extern "C" void nul_lwip_input(void * data, unsigned size);
extern "C" void nul_lwip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac); 
extern "C" void nul_lwip_dhcp_start(void);
extern "C" unsigned long long nul_lwip_netif_next_timer(void);

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

    bool use_network(Utcb *utcb, unsigned cpuid, Hip * hip, unsigned sm) {
      bool res;

      KernelSemaphore sem = KernelSemaphore(sm, true);

      NetworkConsumer * netconsumer = new NetworkConsumer();
      if (!netconsumer)
        return false;

      TimerConsumer * tmconsumer    = new TimerConsumer();
      if (!tmconsumer)
        return false;

      res = Sigma0Base::request_network_attach(utcb, netconsumer,
                                               sem.sm());
      Logging::printf("request network attach - %s\n",
                      (res == 0 ? "success" : "failure"));
      if (res)
        return false;

      res = Sigma0Base::request_timer_attach(utcb, tmconsumer, sem.sm());
      Logging::printf("request timer attach - %s\n",
                      (res == 0 ? "success" : "failure"));
      if (res)
        return false;


	    MessageHostOp msg_op(MessageHostOp::OP_GET_MAC, 0);
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

      Clock * _clock = new Clock(hip->freq_tsc);
      memset(tmconsumer->_buffer, 0xff, sizeof(tmconsumer->_buffer));

      unsigned long long timeout = nul_lwip_netif_next_timer();
//      Logging::printf("info    - next timeout in %llu ms\n", timeout);

      MessageTimer to(0, _clock->time() + timeout * hip->freq_tsc);
      if (Sigma0Base::timer(to))
         Logging::printf("failed  - starting timer\n");

      //lwip init
      lwip_init();
      nul_lwip_init(send_network, mac);

      nul_lwip_dhcp_start();

      while (1) {
        unsigned char *buf;

        sem.downmulti();

//        Logging::printf("time r:w %x:%x, netconsumer r:w %x:%x\n", tmconsumer->_rpos, tmconsumer->_wpos, netconsumer->_rpos, netconsumer->_wpos);
        //check whether timer triggered
        if (tmconsumer->isData()) {
          while (tmconsumer->isData()) {
            tmconsumer->get_buffer();
            tmconsumer->free_buffer();
          }
 
          unsigned long long timeout = nul_lwip_netif_next_timer();
//          Logging::printf("info    - next timeout in %llu ms\n", timeout);

          MessageTimer to(0, _clock->time() + timeout * hip->freq_tsc);
          if (Sigma0Base::timer(to))
            Logging::printf("failed  - starting timer\n");
        }

        while (netconsumer->isData()) {
          unsigned size = netconsumer->get_buffer(buf);
          nul_lwip_input(buf, size);
          netconsumer->free_buffer();
        }
      }

      if (res)
        return false;

      return true;
    }

  void run(Utcb *utcb, Hip *hip)
  {

    init(hip);
    init_mem(hip);

    console_init("LWIP test");

    Logging::printf("Hello\n");

    _virt_phys.debug_dump("");

    if (!use_network(utcb, Cpu::cpunr(), hip, alloc_cap()))
      Logging::printf("failed  - starting lwip stack\n");
    
  }
};

} /* namespace */

ASMFUNCS(ab::TestLWIP, NovaProgram)
