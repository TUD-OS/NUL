/*
 * (C) 2010-2012 Alexander Boettcher
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
#include <service/endian.h>

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
  IP_TCP_SEND     = 7,
  IP_TCP_CLOSE    = 8
};

namespace ab {

class TestIP : public NovaProgram, public ProgramConsole
{
    static bool _connection_online;

    static void send_network(char unsigned const * data, unsigned len) {
      bool res;

      MessageNetwork net = MessageNetwork(data, len, 0);

      res = Sigma0Base::network(net);
      if (res)
      Logging::printf("%s - sending packet to network, len = %u\n", 
                      (res == 0 ? "success" : "failure"), len);
    }

    static
    void fn_connected(bool status) {
        Logging::printf("%s - connection established\n", 
                        status ? "success" : "failure");
        _connection_online = status;
    }

    bool use_network(Utcb *utcb, Hip * hip, unsigned sm) {
      bool res;
      unsigned long long arg = 0;
      Clock * _clock = new Clock(hip->freq_tsc);

      if (!nul_ip_config(IP_NUL_VERSION, &arg) || arg != 0x5) return false;

      NetworkConsumer * netconsumer = new NetworkConsumer();
      if (!netconsumer) return false;

      TimerProtocol * timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
      res = timer_service->timer(*utcb, _clock->abstime(0, 1000));

      Logging::printf("%s - request timer attach\n", (res == 0 ? "success" : "failure"));
      if (res) return false;

      KernelSemaphore sem = KernelSemaphore(timer_service->get_notify_sm());
      res = Sigma0Base::request_network_attach(utcb, netconsumer, sem.sm());

      Logging::printf("%s - request network attach\n", (res == 0 ? "success" : "failure"));
      if (res) return false;

      MessageNetwork msg_op(MessageNetwork::QUERY_MAC, 0);
      res = Sigma0Base::network(msg_op);
      Logging::printf("%s - got mac %02llx:%02llx:%02llx:%02llx:%02llx:%02llx\n",
                      (res == 0 ? "success" : "failure"),
                      (msg_op.mac >> 40) & 0xFF, (msg_op.mac >> 32) & 0xFF,
                      (msg_op.mac >> 24) & 0xFF, (msg_op.mac >> 16) & 0xFF,
                      (msg_op.mac >> 8) & 0xFF, (msg_op.mac) & 0xFF);

      unsigned long long mac = ((0ULL + Endian::hton32(msg_op.mac)) << 32 | Endian::hton32(msg_op.mac >> 32)) >> 16;

      if (!nul_ip_config(IP_TIMEOUT_NEXT, &arg)) Logging::panic("failed - requesting timeout\n");

      if (timer_service->timer(*utcb, _clock->time() + arg * hip->freq_tsc))
        Logging::panic("failed  - starting timer\n");
      if (!nul_ip_init(send_network, mac)) Logging::panic("failed - starting ip\n");
      if (!nul_ip_config(IP_DHCP_START, NULL)) Logging::panic("failed - starting dhcp\n");

      unsigned long port = 5555;
      bool result;

      result = nul_ip_config(IP_UDP_OPEN, &port);
      Logging::printf("%s - creating udp port %lu\n", (res == 0 ? "success" : "failure"), port);
      if (!result) return result;

      struct {
        unsigned long port;
        void (*fn)(uint32 remoteip, uint16 remoteport, uint16 localport, void * data, size_t in_len);
        unsigned long addr;
        void (*connected) (bool);
      } PACKED conn = { 7777, 0, 0, 0 };
      result = nul_ip_config(IP_TCP_OPEN, &conn);
      Logging::printf("%s - creating tcp port %lu\n", (res == 0 ? "success" : "failure"), conn.port);
      if (!result) return result;

      while (1) {
        unsigned char *buf;
        unsigned tcount = 0;

        sem.downmulti();

        //check whether timer triggered
        if (!timer_service->triggered_timeouts(*utcb, tcount) && tcount) {
          unsigned long long timeout;
          nul_ip_config(IP_TIMEOUT_NEXT, &timeout);
          //Logging::printf("info    - next timeout in %llu ms\n", timeout);

          if (timer_service->timer(*utcb, _clock->time() + timeout * hip->freq_tsc))
            Logging::printf("failed  - starting timer\n");

          //dump ip addr if we got one
          if (nul_ip_config(IP_IPADDR_DUMP, NULL)) { }
/*

            conn.port = 7777;
            conn.addr = (1 << 24) | 127; //127.0.0.1
            conn.connected = fn_connected;

            result = nul_ip_config(IP_TCP_OPEN, &conn);
            Logging::printf("%s - connecting from %lu to %lu tcp port\n",
                            (res == 0 ? "success" : "failure"), conn.port, 7777UL);
            if (!result) return result;
          }

          if (!_connection_online) continue;

          //send regularly some test stuff
          struct {
            unsigned long port;
            size_t count;
            void const * data;
          } arg = { conn.port, 7, "blabla" };
          nul_ip_config(IP_TCP_SEND, &arg);
*/
        }

        while (netconsumer->has_data()) {
          unsigned size = netconsumer->get_buffer(buf);
          nul_ip_input(buf, size);
          netconsumer->free_buffer();
        }
      }

      return !res;
    }

  public:

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

 bool TestIP::_connection_online = false;
} /* namespace */

ASMFUNCS(ab::TestIP, NovaProgram)
