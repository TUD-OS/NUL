/*
 * Copyright (C) 2011, Alexander Boettcher <boettcher@tudos.org>
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
#pragma once

#include <service/string.h> //memset
#include <service/math.h> // htonl, htons
#include <service/logging.h>
#include <service/helper.h> //assert

#include <nul/service_admission.h>
#include <nul/service_fs.h>
#include <nul/capalloc.h>

#include <nul/types.h>
typedef uint8  uint8_t;
typedef uint16 uint16_t;
typedef uint32 uint32_t;
typedef uint64 uint64_t;

#include "nova_types.h"
#define UUID_LEN 16
#define DAEMON_VERSION 0xb001U

class ConfigProtocol;

#include <sigma0/consumer.h>

typedef PacketConsumer<NOVA_PACKET_LEN * 64> EventConsumer;
typedef PacketProducer<NOVA_PACKET_LEN * 64> EventProducer;

class Remcon : public CapAllocator {
  private:

    bool gevents;

    struct server_data {
      enum status {
        OFF     = 0,
        RUNNING = 1,
        BLOCKED = 2,
        PAUSED  = 3,
      } state;
      bool events;
      cap_sel scs_usage;
      unsigned long maxmem;
      short istemplate;
      uint32_t id;
      unsigned short remoteid;
      char uuid[UUID_LEN];
      char fsname[32];
      const char * filename;
      const char * showname;
      unsigned showname_len;
      unsigned filename_len;
    } server_data[32];

    unsigned char buf_out[NOVA_PACKET_LEN];
    unsigned char buf_in[NOVA_PACKET_LEN];
    unsigned data_received;
    struct incoming_packet * _in;
    struct outgoing_packet * _out;

    bool dowrite;
    char const * cmdline;
    ConfigProtocol *service_config;
    unsigned cpu_count;

    void handle_packet(void);

    void gen_uuid(char const * text, char uuid[UUID_LEN]) { //XXX not really uuid
      unsigned i = 0;
      while (text[0]) {
        uuid[i % UUID_LEN] += text[0];
        i++; text++;
      }
    }

    struct server_data * check_uuid(char uuid[UUID_LEN]);

    EventProducer     * eventproducer;
    AdmissionProtocol * service_admission;

  public:

    Remcon(char const * _cmdline, ConfigProtocol * _sconfig, unsigned _cpu_count,
           unsigned long cap_start, unsigned long cap_order, EventProducer * producer) :
      CapAllocator(cap_start, cap_start, cap_order), gevents(false),
      data_received(0), dowrite(false), cmdline(_cmdline), service_config(_sconfig),
      cpu_count(_cpu_count), eventproducer(producer)
    {
      service_admission = new AdmissionProtocol(alloc_cap(AdmissionProtocol::CAP_SERVER_PT + _cpu_count));

      _in  = reinterpret_cast<struct incoming_packet *>(buf_in);
      _out = reinterpret_cast<struct outgoing_packet *>(buf_out);

      memset(buf_in , 0, sizeof(buf_out));
      memset(buf_out, 0, sizeof(buf_out));
      memset(server_data, 0, sizeof(server_data));
 
      unsigned proto_len, pos = 0, showname_len;
      char const * filename, * showname;
      char const * cmdl = cmdline;

      int len = strspn(cmdl, " \t\r\n\f"); //skip some leading separator characters
      cmdl += len;
      len = strcspn(cmdl, " \t\r\n\f"); //skip first xyz:// - that is this binary
      cmdl += len;
      while (cmdl[0]) {  //format  showname:rom://file
        len = strspn(cmdl, " \t\r\n\f"); //skip separator characters
        cmdl += len;
        if (!cmdl[0]) break;

        showname = strstr(cmdl, ":");
        if (!showname) break;
        showname_len = showname - cmdl;
        showname = cmdl;
        cmdl += showname_len + 1;
        if (!cmdl[0]) break;

        proto_len = sizeof(server_data[pos].fsname) - 3;
        memcpy(server_data[pos].fsname, "fs/", 3);
        filename = FsProtocol::parse_file_name(cmdl, server_data[pos].fsname + 3, proto_len);
        if (!filename) break;
        len = strcspn(cmdl, " \t\r\n\f");
        if (filename > cmdl + len) { cmdl += len; continue; }
      
        char _uuid[UUID_LEN];
        gen_uuid(filename, _uuid);
        if (check_uuid(_uuid)) Logging::panic("Generating uuid failed\n");
        memcpy(server_data[pos].uuid, _uuid, UUID_LEN);

        server_data[pos].id = pos + 1;
        server_data[pos].istemplate = 1;
        server_data[pos].showname = showname;
        server_data[pos].showname_len = showname_len;
        server_data[pos].filename = filename;
        server_data[pos].filename_len = len - (filename - cmdl);
        pos ++;

        cmdl += len;
      }
    }

    void recv_call_back(void * in, size_t in_len, void * & out, size_t & out_len);
    int send(void * mem, size_t count) { assert(mem == buf_out); assert(count == sizeof(buf_out)); dowrite = true; return count; }

    bool find_uuid(unsigned remoteid, char * uuid) {
      for(unsigned i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
        if (server_data[i].remoteid != 0 && server_data[i].remoteid == remoteid)
          return memcpy(uuid, server_data[i].uuid, UUID_LEN);
      }
      return false;
    }

    bool push_event(int guid, uint32_t eventid, uint32_t extra_len = 0, char const * extra = 0) {
      unsigned char buf[NOVA_PACKET_LEN];
      struct outgoing_packet * out  = reinterpret_cast<struct outgoing_packet *>(buf);

      char * uuid  = reinterpret_cast<char *>(&out->opspecific);
      if (!find_uuid(guid, uuid)) {
        if (!gevents) return false;
        memset(uuid, -1, UUID_LEN);
      }
      if (!gevents) {
        struct server_data * entry = check_uuid(uuid);
        if (!entry || !entry->events) return false;
      }

      *reinterpret_cast<uint32_t *>(&out->opspecific + UUID_LEN) = Math::htonl(eventid);
      *reinterpret_cast<uint32_t *>(&out->opspecific + UUID_LEN + sizeof(uint32_t)) = Math::htonl(extra_len);
      if (&out->opspecific - buf + UUID_LEN + 2 * sizeof(uint32_t) + extra_len > NOVA_PACKET_LEN) return false;
      if (extra_len) memcpy(&out->opspecific + UUID_LEN + 2 * sizeof(uint32_t), extra, extra_len);

      out->version = Math::htons(DAEMON_VERSION);
      out->opcode  = Math::htons(NOVA_EVENT);
      out->result  = NOVA_OP_SUCCEEDED;

      eventproducer->produce(buf, NOVA_PACKET_LEN);

      return true;
    }
};
