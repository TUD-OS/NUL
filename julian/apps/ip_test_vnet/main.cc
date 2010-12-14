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
#include <nul/motherboard.h>
#include <service/net.h>
#include <service/endian.h>
#include <nul/timer.h> //clock
#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

#include <nul/service_timer.h> //TimerService

#include "../../model/simplenet.h"

extern "C" void lwip_init();
extern "C" void nul_lwip_input(void * data, unsigned size);
extern "C" void nul_lwip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac); 
extern "C" void nul_lwip_dhcp_start(void);
extern "C" unsigned long long nul_lwip_netif_next_timer(void);

namespace js {

  class StrangeMemory : public SimpleNetworkClient::Memory
  {
  public:
    virtual void *allocate_backend(size_t size)
    {
      return memalloc(size, 0x1000);
    }

    virtual mword ptr_to_phys(const void *ptr, size_t size)
    {
      return reinterpret_cast<mword>(ptr) - (1UL << 31);
    }

    
    StrangeMemory() {}
  };


  class TestLWIP : public NovaProgram, public ProgramConsole, public StaticReceiver<TestLWIP>
  {
    static StrangeMemory           *net_mem;
    static SimpleNetworkClient     *net;
    static DBus<MessageVirtualNet> *bus_vnet;

  public:

    static void send_network(uint8 const * data, unsigned len)
    {
      static class TxDone : public SimpleNetworkClient::Callback
      {
      public:
        void send_callback(uint8 const *data)
        {
          Logging::printf("%p sent!\n", data);
          delete data;
        }
      } tx_done;


      uint8 *bdata = new uint8[len];
      Logging::printf("Send %p+%x\n", bdata, len);
      memcpy(bdata, data, len);
      net->send_packet(bdata, len, &tx_done);
    }

    bool receive(MessageVirtualNet &msg)
    {
      // XXX ???
      msg.physoffset = (1UL << 31);
      return Sigma0Base::vnetop(msg);
    }

    bool use_network(Utcb *utcb, Hip * hip) {
      bool res;
      Clock * _clock = new Clock(hip->freq_tsc);

      TimerProtocol *_timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_NUM));
      TimerProtocol::MessageTimer msg(_clock->abstime(0, 1000));
      res = _timer_service->timer(*utcb, msg);
      Logging::printf("request timer attach - %s\n",
                      (res == 0 ? "success" : "failure"));
      if (res)
        return false;

      KernelSemaphore sem = KernelSemaphore(_timer_service->get_notify_sm());

      Sigma0Base::request_vnet_attach(utcb, _timer_service->get_notify_sm());

      MessageHostOp msg_op(MessageHostOp::OP_GET_MAC, 0UL);
      res = Sigma0Base::hostop(msg_op);
      EthernetAddr mac(Endian::hton64(msg_op.mac) >> 16);
      Logging::printf("got mac " MAC_FMT " - %s\n",
                      MAC_SPLIT(&mac),
                      (res == 0 ? "success" : "failure"));

      unsigned long long timeout = nul_lwip_netif_next_timer();
      //      Logging::printf("info    - next timeout in %llu ms\n", timeout);

      TimerProtocol::MessageTimer to(_clock->time() + timeout * hip->freq_tsc);
      if (_timer_service->timer(*utcb, to))
         Logging::printf("failed  - starting timer\n");


      //lwip init
      lwip_init();
      nul_lwip_init(send_network, mac.raw);

      net->set_link(true);
      net->enable_wakeups();

      // Queue 2 receive buffers
      {
        uint8 *buffer = net_mem->allocate<uint8>(4096);
        net->queue_buffer(buffer, 2048);
        net->queue_buffer(buffer + 2048, 2048);
      }

      nul_lwip_dhcp_start();

      while (1) {
        unsigned tcount = 0;
        sem.downmulti();

        //check whether timer triggered
        if (!_timer_service->triggered_timeouts(*utcb, tcount) && tcount) {
          unsigned long long timeout = nul_lwip_netif_next_timer();
          //Logging::printf("info    - next timeout in %llu ms\n", timeout);

          TimerProtocol::MessageTimer to(_clock->time() + timeout * hip->freq_tsc);
          if (_timer_service->timer(*utcb,to))
            Logging::printf("failed  - starting timer\n");
        }
        
        uint8 *data;
        size_t len;
        
        net->consume_wakeup();

        while (net->poll_receive(&data, &len)) {
          Logging::printf("Received %p+%x: " MAC_FMT "\n", data, len,
                          MAC_SPLIT(reinterpret_cast<EthernetAddr *>(data)));
          nul_lwip_input(data, len);
          net->queue_buffer(data, 2048);
        }

        net->unmask_wakeups();
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

      bus_vnet = new DBus<MessageVirtualNet>;
      bus_vnet->add(this, TestLWIP::receive_static<MessageVirtualNet>);

      net_mem  = new StrangeMemory;
      net      = new SimpleNetworkClient(*net_mem, *bus_vnet);

      if (!use_network(utcb, hip))
        Logging::printf("failed  - starting lwip stack\n");
    
    }
  };

  StrangeMemory           *TestLWIP::net_mem;
  SimpleNetworkClient     *TestLWIP::net;
  DBus<MessageVirtualNet> *TestLWIP::bus_vnet;
} /* namespace */

ASMFUNCS(js::TestLWIP, NovaProgram)


// EOF
