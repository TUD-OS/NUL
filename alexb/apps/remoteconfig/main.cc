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
#include <nul/timer.h>
#include <nul/service_timer.h>
#include <nul/service_log.h>
#include <nul/service_config.h>
#include <service/endian.h>
#include <service/cmdline.h>

#include <sigma0/sigma0.h> // Sigma0Base object
#include <sigma0/console.h>

#include "server.h"
#include "events.h"
//#include "log.h"

EXTERN_C void dlmalloc_init(cap_sel pool);

#define NUL_TCP_EOF (~0u)

extern "C" void nul_ip_input(void * data, unsigned size);
extern "C" bool nul_ip_init(void (*send_network)(char unsigned const * data, unsigned len), unsigned long long mac); 
extern "C" bool nul_ip_config(unsigned para, void * arg);

extern "C" bool nul_tls_init(unsigned char * server_cert, int32 server_cert_len,
                             unsigned char * server_key, int32 server_key_len,
                             unsigned char * ca_cert, int32 ca_cert_len);
extern "C" int32 nul_tls_session(void *&ssl);
extern "C" int32 nul_tls_len(void * ssl, unsigned char * &buf);
extern "C" int32 nul_tls_config(int32 transferred, void (*write_out)(uint16 localport, void * out, size_t out_len),
                                void * &appdata, size_t &appdata_len, bool bappdata, uint16 port, void * &ssl_session);
extern "C" void  nul_tls_delete_session(void * ssl_session);

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

class RemoteConfig : public NovaProgram, public ProgramConsole
{
  private:
    static Remcon * remcon;
    static void * tls_session_cmd, * tls_session_event;

    enum {
      LIBVIRT_CMD_PORT=9999,
      LIBVIRT_EVT_PORT=10000,
      LIBVIRT_FILE_PORT=10043,
    };

    static bool enable_tls;
  public:

    static void send_network(char unsigned const * data, unsigned len) {
      bool res;

      MessageNetwork net = MessageNetwork(data, len, 0);

      res = Sigma0Base::network(net);

      if (res) Logging::printf("%s - sending packet to network, len = %u, res= %u\n", 
                               (res == 0 ? "done   " : "failure"), len, res);
    }

    static void write_out(uint16 localport, void * out, size_t out_len) {
        if (out && out_len > 0) {
          struct {
            unsigned long port;
            size_t count;
            void * data;
          } arg = { localport, out_len, out }; //XXX fix port
//          Logging::printf("[da ip] - write port=%lu size=%zu\n", arg.port, arg.count);
          nul_ip_config(IP_TCP_SEND, &arg);
        }
    }

    static
    void recv_call_back_file(uint32 remoteip, uint16 remoteport, uint16 localport, void * in, size_t in_len) {
      void * out = 0;
      size_t out_len = 0;
      remcon->recv_file(remoteip, remoteport, localport, in, in_len, out, out_len);
      write_out(LIBVIRT_FILE_PORT, out, out_len);
      return;
    }


    static
    void recv_call_back(uint32 remoteip, uint16 remoteport, uint16 localport, void * in, size_t in_len) {
      void * appdata, * out;
      size_t appdata_len, out_len;
      unsigned char * sslbuf;
      int32 rc = 1;
      void *&tls_session = (localport == LIBVIRT_CMD_PORT) ? tls_session_cmd : tls_session_event;

      assert(localport == LIBVIRT_CMD_PORT || localport == LIBVIRT_EVT_PORT);

      if (!in && (in_len == NUL_TCP_EOF)) {
        if (enable_tls) {
          nul_tls_delete_session(tls_session);
          rc  = nul_tls_session(tls_session); // Prepare new session for the next time???
          assert(rc == 0);
        }
        remcon->obj_auth->reset();
        return;
      }
      if (enable_tls) {
        int32 len = nul_tls_len(tls_session, sslbuf);
        if (len < 0 || (0UL + len) < in_len) Logging::panic("buffer to small!!! %d %zu\n", len, in_len);
        //Logging::printf("[da ip] - port %u sslbuf=%p ssllen=%d in_len=%u\n", localport, sslbuf, len, in_len);
        memcpy(sslbuf, in, in_len);

        rc = nul_tls_config(in_len, write_out, appdata, appdata_len, false, localport, tls_session);
      } else {
       appdata = in; appdata_len = in_len;
      }

      if (rc > 0) {
        if (localport == LIBVIRT_EVT_PORT) return; //don't allow to send via event port messages to the daemon

        loop:

        out = 0; out_len = 0;
        remcon->recv_call_back(appdata, appdata_len, out, out_len);
        appdata = out; appdata_len = out_len;
        if (enable_tls) {
          rc = nul_tls_config(0, write_out, appdata, appdata_len, true, localport, tls_session);
          if (rc > 0) goto loop;
        } else write_out(localport, appdata, appdata_len);
      }
      if (rc < 0) nul_ip_config(IP_TCP_CLOSE, &localport);
    }

    unsigned  create_ec4pt(void * tls, phy_cpu_no cpunr, unsigned excbase, Utcb **utcb_out=0, unsigned long cap = ~0UL) {
      return NovaProgram::create_ec4pt(tls, cpunr, excbase, utcb_out, cap);
    }

    bool start_services(Utcb *utcb, Hip * hip, EventProducer * prod_event) {
      char const *args[16];
      char const *cmdline = reinterpret_cast<char const *>(hip->get_mod(0)->aux);
      unsigned argv = Cmdline::parse(cmdline, args, sizeof(args)/sizeof(char *));
      char const * templatefile = 0, * diskuuidfile = 0;
      unsigned temp_len = 0, disk_len = 0;
      unsigned verbose = 0;

      for (unsigned i=1; i < argv && i < 16; i++) {
        if (!strncmp("template=", args[i],9)) { templatefile = args[i] + 9; temp_len = strcspn(templatefile, " \n\t\f");}
        if (!strncmp("diskuuid=", args[i],9)) { diskuuidfile = args[i] + 9; disk_len = strcspn(diskuuidfile, " \n\t\f");}
        if (!strncmp("verbose",   args[i],7)) { verbose = 1;}
        if (!strncmp("nossltls",  args[i],8)) { enable_tls = false;}
        continue;
      }

      //create network service object
      ConfigProtocol *service_config = new ConfigProtocol(alloc_cap(ConfigProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
      unsigned cap_region = alloc_cap_region(1 << 14, 14);
      if (!cap_region) Logging::panic("failure - starting libvirt backend\n");
      remcon = new Remcon(reinterpret_cast<char const *>(_hip->get_mod(0)->aux), service_config, hip->cpu_desc_count(),
                          cap_region, 14, prod_event, NULL, templatefile, temp_len, diskuuidfile, disk_len, verbose);
      //create event service object
      EventService * event = new EventService(remcon, verbose);
      if (!event || !event->start_service(utcb, hip, this)) return false;
      //create log service object
//      LogService * log = new LogService(remcon, verbose);
//      if (!log || !log->start_service(utcb, hip, this)) return false;

      return true;
    }

    bool use_network(Utcb *utcb, Hip * hip, EventConsumer * sendconsumer, 
                     Clock * _clock, KernelSemaphore &sem, TimerProtocol * timer_service)
    {
      unsigned long long arg = 0;
      char const *args[16];
      char const *pos_s;
      char const *cmdline = reinterpret_cast<char *>(hip->get_mod(0)->aux);
      unsigned argv = Cmdline::parse(cmdline, args, sizeof(args)/sizeof(char *));
      unsigned res;
      unsigned char *serverkey = 0, *servercert = 0, *cacert = 0, **crypto;
      int32 serverkey_len = 0, servercert_len = 0, cacert_len = 0, *crypto_len;

      for (unsigned i=1; i < argv && i < 16; i++) {
        if (!strncmp("servercert=", args[i],11)) { pos_s = args[i] + 11; crypto=&servercert; crypto_len=&servercert_len; goto tlsparse; }
        if (!strncmp("serverkey=" , args[i],10)) { pos_s = args[i] + 10; crypto=&serverkey;  crypto_len=&serverkey_len; goto tlsparse; }
        if (!strncmp("cacert="    , args[i],7))  { pos_s = args[i] +  7; crypto=&cacert;     crypto_len=&cacert_len; goto tlsparse; }
        continue;

        tlsparse:
        unsigned portal_num = FsProtocol::CAP_SERVER_PT + hip->cpu_desc_count();
        unsigned cap_base = alloc_cap(portal_num);
        int len = strcspn(pos_s, " \t\r\n\f");
        //Logging::printf("file %d %s\n", len, pos_s);

        char fs_service_name[16] = "fs/";
        size_t fs_service_name_len = sizeof(fs_service_name);
        const char * filename = FsProtocol::parse_file_name(pos_s, fs_service_name + 3, fs_service_name_len);
        if (!filename || (filename > pos_s + len)) Logging::panic("failure - misformed file name %s\n", pos_s);

        FsProtocol::dirent fileinfo;
        FsProtocol fs_obj(cap_base, fs_service_name);
        FsProtocol::File file_obj(fs_obj, alloc_cap());
        if ((res = fs_obj.get(*myutcb(), file_obj, filename, strcspn(filename," \t\r\n\f"))) ||
            (res = file_obj.get_info(*utcb, fileinfo)))
          Logging::panic("failure - reading error (%d) of file '%s' from '%s'\n", res, filename, fs_service_name);

        *crypto = new(4096) unsigned char[fileinfo.size];
        *crypto_len = fileinfo.size;
        if (!*crypto) Logging::panic("failure - out of memory (%llu) for file\n", fileinfo.size);
        res = file_obj.copy(*utcb, *crypto, fileinfo.size);
        if (res != ENONE) Logging::panic("failure - reading file %u\n", res);

        fs_obj.destroy(*utcb, portal_num, this);
      }
      if (enable_tls && (!nul_tls_init(servercert, servercert_len, serverkey, serverkey_len, cacert, cacert_len) ||
          nul_tls_session(tls_session_cmd) < 0 || nul_tls_session(tls_session_event) < 0)) return false;

      if (!nul_ip_config(IP_NUL_VERSION, &arg) || arg != 0x5) return false;

      NetworkConsumer * netconsumer = new NetworkConsumer();
      if (!netconsumer) return false;
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

      if (timer_service->timer(*utcb, _clock->abstime(arg, 1))) Logging::panic("failure - programming timer\n");
      if (!nul_ip_init(send_network, mac)) Logging::panic("failure - starting ip stack\n");

      // check for static ip - otherwise use dhcp
      unsigned entry;
      unsigned long addr[4] = { 0, 0x00ffffffUL, 0, 0 }; //addr, netmask, gw
      bool static_ip = false;
      for (unsigned i=1; i < argv && i < 16; i++) {
        if (!strncmp("ip="      , args[i],3)) { entry=0; pos_s = args[i] + 3; static_ip = true; goto parse; }
        if (!strncmp("mask="    , args[i],5)) { entry=1; pos_s = args[i] + 5; goto parse; }
        if (!strncmp("gw="      , args[i],3)) { entry=2; pos_s = args[i] + 3; goto parse; }
        continue;

        parse:
          unsigned long num = 0;
          char * pos_e;
          for (unsigned j=0; *pos_s != 0 && j < 4; j++) {
            num = strtoul(pos_s, 0, 0);
            addr[entry] = num << 24 | (addr[entry] >> 8);
            pos_e = strstr(pos_s, ".");
            if (pos_e) pos_s = pos_e + 1;
            else break;
          }
          if (entry == 0 && pos_s && *pos_s != 0 && (pos_e = strstr(pos_s, "/"))) {
              pos_s = pos_e + 1;
              unsigned long num = strtoul(pos_s, 0, 0);
              if (num < 33) {
                addr[1] = ((1UL << (32 - num)) - 1);
                addr[1] = ~addr[1];
                addr[1] = ((addr[1] & 0xff) << 24) | ((addr[1] & 0x0000ff00UL) << 8) | ((addr[1] & 0x00ff0000UL) >> 8) | (addr[1] >>24);
              }
          }

//          Logging::printf("config  - %u %lu.%lu.%lu.%lu\n", entry, addr[entry] & 0xff,
//                         (addr[entry] >> 8) & 0xff, (addr[entry] >> 16) & 0xff, (addr[entry] >> 24) & 0xff);
      }
      if (static_ip)
        Logging::printf("%s - static ip=%lu.%lu.%lu.%lu mask=%lu.%lu.%lu.%lu gw=%lu.%lu.%lu.%lu\n",
                       nul_ip_config(IP_SET_ADDR, addr) ? "config " : "failure",
                       addr[0] & 0xff, (addr[0] >> 8) & 0xff, (addr[0] >> 16) & 0xff, (addr[0] >> 24) & 0xff,
                       addr[1] & 0xff, (addr[1] >> 8) & 0xff, (addr[1] >> 16) & 0xff, (addr[1] >> 24) & 0xff,
                       addr[2] & 0xff, (addr[2] >> 8) & 0xff, (addr[2] >> 16) & 0xff, (addr[2] >> 24) & 0xff);
      else
        if (!nul_ip_config(IP_DHCP_START, NULL)) Logging::panic("failure - starting dhcp service\n");

      struct {
        unsigned long port;
        void (*fn)(uint32 remoteip, uint16 remoteport, uint16 localport, void * data, size_t in_len);
        unsigned long addr;
        void (*connected) (bool);
      } PACKED conn = { LIBVIRT_CMD_PORT, recv_call_back, 0, 0 };
      if (!nul_ip_config(IP_TCP_OPEN, &conn.port)) Logging::panic("failure - opening tcp port %lu\n", conn.port);

      conn.port = LIBVIRT_EVT_PORT;
      conn.fn = recv_call_back;
      if (!nul_ip_config(IP_TCP_OPEN, &conn.port)) Logging::panic("failure - opening tcp port %lu\n", conn.port);

      conn.port = LIBVIRT_FILE_PORT;
      conn.fn = recv_call_back_file;
      if (!nul_ip_config(IP_TCP_OPEN, &conn.port)) Logging::panic("failure - opening tcp port %lu\n", conn.port);
      Logging::printf("done    - open tcp port %d, %d, %d - ssl/tls %s\n", LIBVIRT_CMD_PORT, LIBVIRT_EVT_PORT, LIBVIRT_FILE_PORT, enable_tls ? "enabled" : "disabled");

      if (!static_ip)
        Logging::printf(".......   looking for an IP address via DHCP\n");

      bool up = false;
      while (1) {
        unsigned char *buf;
        unsigned tcount = 0;

        sem.downmulti();

        //check whether timer triggered
        if (!timer_service->triggered_timeouts(*utcb, tcount) && tcount) {
          unsigned long long timeout;

          nul_ip_config(IP_TIMEOUT_NEXT, &timeout);
          if (timer_service->timer(*utcb, _clock->time() + timeout * hip->freq_tsc))
            Logging::printf("failure - programming timer\n");

          //dump ip addr if we got one
          if (nul_ip_config(IP_IPADDR_DUMP, NULL)) {
            Logging::printf("ready   - NOVA management daemon is up. Waiting for libvirt connection ... \n");
            up = true;
          }

          if (up) remcon->obj_auth->check_connection();
        }

        while (netconsumer->has_data()) {
          unsigned size = netconsumer->get_buffer(buf);
          nul_ip_input(buf, size);
          netconsumer->free_buffer();
        }

        while (sendconsumer->has_data()) {
          unsigned size = sendconsumer->get_buffer(buf);
          void * data   = buf;

          if (enable_tls) nul_tls_config(0, write_out, data, size, true, LIBVIRT_EVT_PORT, tls_session_event); //XXX check here
          else write_out(LIBVIRT_EVT_PORT, data, size);

          sendconsumer->free_buffer();
        }
      }

      return !res;
    }

  void run(Utcb *utcb, Hip *hip)
  {
    enable_tls = true;

    init(hip);
    init_mem(hip);

    console_init("NOVA daemon", new Semaphore(alloc_cap(), true));
    _console_data.log = new LogProtocol(alloc_cap(LogProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));

    dlmalloc_init(alloc_cap(0x10));

    Logging::printf("booting - NOVA daemon ...\n");

    Clock * _clock = new Clock(hip->freq_tsc);

    TimerProtocol * timer_service = new TimerProtocol(alloc_cap(TimerProtocol::CAP_SERVER_PT + hip->cpu_desc_count()));
    bool res = timer_service->timer(*utcb, _clock->abstime(0, 1000));

    Logging::printf("%s - request timer attach\n", (res == 0 ? "done   " : "failure"));
    if (res) Logging::panic("failure - attaching to timer service");

    KernelSemaphore sem = KernelSemaphore(timer_service->get_notify_sm());

    EventConsumer * send_consumer = new EventConsumer();
    EventProducer * send_producer = new EventProducer(send_consumer, sem.sm());

    if (!start_services(utcb, hip, send_producer)) Logging::panic("failure - starting services failed\n");
    if (!use_network(utcb, hip, send_consumer, _clock, sem, timer_service)) Logging::printf("failure - starting ip stack\n");

  }
};

} /* namespace */

Remcon * ab::RemoteConfig::remcon;
void * ab::RemoteConfig::tls_session_cmd, * ab::RemoteConfig::tls_session_event;
bool ab::RemoteConfig::enable_tls;

ASMFUNCS(ab::RemoteConfig, NovaProgram)
