/*
 * (C) 2011 Alexander Boettcher
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
#include <nul/service_config.h>
#include <service/endian.h>

#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

#include "server.h"

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
  IP_SET_ADDR     = 6
};

namespace ab {

class RemoteConfig : public NovaProgram, public ProgramConsole
{
  private:
    static Remcon * remcon;

  public:

    static void send_network(char unsigned const * data, unsigned len) {
      bool res;

      MessageNetwork net = MessageNetwork(data, len, 0);

      res = Sigma0Base::network(net);

      if (res) Logging::printf("%s - sending packet to network, len = %u, res= %u\n", 
                               (res == 0 ? "done   " : "failure"), len, res);
    }

    static
    void recv_call_back(void * in, size_t in_len, void * & out, size_t & out_len) {
      remcon->recv_call_back(in, in_len, out, out_len);    
    }

    bool use_network(Utcb *utcb, Hip * hip, unsigned sm) {
      bool res;
      unsigned long long arg = 0;
      Clock * _clock = new Clock(hip->freq_tsc);

      if (!nul_ip_config(IP_NUL_VERSION, &arg) || arg != 0x1) return false;

      NetworkConsumer * netconsumer = new NetworkConsumer();
      if (!netconsumer) return false;

      TimerProtocol * timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_SERVER_PT + hip->cpu_count()));
      TimerProtocol::MessageTimer msg(_clock->abstime(0, 1000));
      res = timer_service->timer(*utcb, msg);

      Logging::printf("%s - request timer attach\n", (res == 0 ? "done   " : "failure"));
      if (res) return false;

      KernelSemaphore sem = KernelSemaphore(timer_service->get_notify_sm());
      res = Sigma0Base::request_network_attach(utcb, netconsumer, sem.sm());

      Logging::printf("%s - request network attach\n", (res == 0 ? "done   " : "failure"));
      if (res) return false;

      unsigned long long nmac;
      MessageNetwork msg_op(MessageNetwork::QUERY_MAC, 0);
      res = Sigma0Base::network(msg_op);
      nmac = msg_op.mac;
      if (res) {
        MessageHostOp msg_op1(MessageHostOp::OP_GET_MAC, 0UL);
        res = Sigma0Base::hostop(msg_op1);
        nmac = msg_op1.mac;
      }
      Logging::printf("%s - mac %02llx:%02llx:%02llx:%02llx:%02llx:%02llx\n",
                      (res == 0 ? "done   " : "failure"),
                      (nmac >> 40) & 0xFF, (nmac >> 32) & 0xFF,
                      (nmac >> 24) & 0xFF, (nmac >> 16) & 0xFF,
                      (nmac >> 8) & 0xFF, (nmac) & 0xFF);

      unsigned long long mac = Endian::hton64(nmac) >> 16;

      if (!nul_ip_config(IP_TIMEOUT_NEXT, &arg)) Logging::panic("failure - request for timeout\n");

      TimerProtocol::MessageTimer to(_clock->abstime(arg, 1));
      if (timer_service->timer(*utcb, to)) Logging::panic("failure - programming timer\n");
      if (!nul_ip_init(send_network, mac)) Logging::panic("failure - starting ip stack\n");
      if (!nul_ip_config(IP_DHCP_START, NULL)) Logging::panic("failure - starting dhcp service\n");

      //create server object
      ConfigProtocol *service_config = new ConfigProtocol(alloc_cap(ConfigProtocol::CAP_SERVER_PT + hip->cpu_count()));
      unsigned cap_region = alloc_cap_region(1 << 14, 14);
      if (!cap_region) Logging::panic("failure - starting libvirt backend\n");
      remcon = new Remcon(reinterpret_cast<char const *>(_hip->get_mod(0)->aux), service_config, hip->cpu_count(),
                          cap_region, 14);

      struct {
        unsigned long port;
        void (*fn)(void * in_data, size_t in_len, void * &out_data, size_t & out_len);
      } conn = { 9999, recv_call_back };
      if (!nul_ip_config(IP_TCP_OPEN, &conn.port)) Logging::panic("failure - opening tcp port\n");

      Logging::printf("done    - tcp port=%lu\n", conn.port);
      Logging::printf(".......   looking for an IP address via DHCP\n");

      while (1) {
        unsigned char *buf;
        unsigned tcount = 0;

        sem.downmulti();

        //check whether timer triggered
        if (!timer_service->triggered_timeouts(*utcb, tcount) && tcount) {
          unsigned long long timeout;

          nul_ip_config(IP_TIMEOUT_NEXT, &timeout);
          TimerProtocol::MessageTimer to(_clock->time() + timeout * hip->freq_tsc);
          if (timer_service->timer(*utcb,to)) Logging::printf("failure - programming timer\n");

          //dump ip addr if we got one
          if (nul_ip_config(IP_IPADDR_DUMP, NULL))
            Logging::printf("ready   - NOVA management daemon is up. Waiting for libvirt connection ... \n");
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

    console_init("remote nova daemon");
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_count()));

    Logging::printf("booting - NOVA management daemon ...\n");

    if (!use_network(utcb, hip, alloc_cap())) Logging::printf("failure - starting ip stack\n");
  }
};

} /* namespace */

Remcon * ab::RemoteConfig::remcon;

ASMFUNCS(ab::RemoteConfig, NovaProgram)
