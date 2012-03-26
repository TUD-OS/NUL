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
#include <nul/disk_helper.h>

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
    DiskHelper<Remcon, 4096> * service_disk;

    bool gevents;

    struct server_data {
      enum status {
        OFF     = 0,
        RUNNING = 1,
        BLOCKED = 2,
        PAUSED  = 3,
      } state;
      struct event {
        uint32_t id;
        uint32_t action;
        bool used;
      } events[16];
      cap_sel scs_usage;
      unsigned long maxmem;
      short istemplate;
      unsigned short remoteid;
      char uuid[UUID_LEN];
      char fsname[32];
      const char * config;
      const char * showname;
      unsigned showname_len;
      unsigned config_len;
      struct {
        uint64_t size;
        uint64_t read;
        struct {
          unsigned diskid;
          unsigned sectorsize;
        } internal;
      } disks[4];
      uint32_t id;
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

    struct server_data * check_uuid(char const uuid[UUID_LEN]);
    struct server_data * get_free_entry();
    void free_entry(struct server_data * entry);
    unsigned start_entry(struct server_data * entry);

    EventProducer     * eventproducer;
    AdmissionProtocol * service_admission;

    const char * file_template, * file_diskuuid;
    unsigned file_len_template, file_len_diskuuid;

    const char * ldisks [4];
    bool ldisks_used[4];

  public:

    Remcon(char const * _cmdline, ConfigProtocol * _sconfig, unsigned _cpu_count,
           unsigned long cap_start, unsigned long cap_order, EventProducer * producer,
           const char * templatefile, unsigned len_template, const char * diskfile, unsigned len_disk) :
      CapAllocator(cap_start, cap_start, cap_order), gevents(false),
      data_received(0), dowrite(false), cmdline(_cmdline), service_config(_sconfig),
      cpu_count(_cpu_count), eventproducer(producer),
      file_template(templatefile), file_diskuuid(diskfile),
      file_len_template(len_template), file_len_diskuuid(len_disk)
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
        server_data[pos].config = filename;
        server_data[pos].config_len = len - (filename - cmdl);
        pos ++;

        cmdl += len;
      }

      //attach to disk service
      service_disk = new (0x1000) DiskHelper<Remcon, 4096>(this, 0);
      assert(service_disk);

      ldisks[0] = "uuid:aa428dc4-b26a-47d5-8b47-083d561639ea"; //XXX alex local test box
      ldisks[1] = "uuid:bacb3e30-eb8c-4a93-85f9-5e86f6101357"; //XXX alex local test box
      ldisks[2] = "uuid:75919be6-df33-4f2d-bc54-d4cc829c65d3"; //XXX alex qemu box
      ldisks[3] = "uuid:d4d38d86-7231-4a28-a97d-91cf35c801ae"; //XXX alex qemu box
      ldisks_used[0] = false; //XXX
      ldisks_used[1] = false; //XXX
      ldisks_used[2] = false; //XXX
      ldisks_used[3] = false; //XXX
    }

    void recv_file(uint32 remoteip, uint16 remoteport, uint16 localport, void * in, size_t in_len);
    void recv_call_back(void * in, size_t in_len, void * & out, size_t & out_len);
    int send(void * mem, size_t count) { assert(mem == buf_out); assert(count == sizeof(buf_out)); dowrite = true; return count; }

    bool find_uuid(unsigned remoteid, char * uuid) {
      for(unsigned i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
        if (server_data[i].remoteid != 0 && server_data[i].remoteid == remoteid)
          return memcpy(uuid, server_data[i].uuid, UUID_LEN);
      }
      return false;
    }

    bool clean_disk(unsigned internal_id) {
      unsigned count = 0, j;
      if (ENONE != service_disk->get_disk_count(*BaseProgram::myutcb(), count)) return false;

      assert(internal_id < count);
      for (j=0; j < sizeof(ldisks) / sizeof(ldisks[0]); j++) {
        if (!ldisks_used[j]) continue;

        bool match;
        unsigned res = service_disk->check_name(*BaseProgram::myutcb(), internal_id, ldisks[j], match);
        if (res != ENONE || !match) continue;
        ldisks_used[j] = false;
        return true;
      }
      return false;
    }

    unsigned get_free_disk(uint64_t disksize, unsigned & sectorsize, char * diskuuid, unsigned diskuuidmax) {
      unsigned count = 0, i, j;
      if (ENONE != service_disk->get_disk_count(*BaseProgram::myutcb(), count)) return ~0U;

      for (i=0; i < count; i++) {
        for (j=0; j < (sizeof(ldisks) / sizeof(ldisks[0])); j++) {
          if (ldisks_used[j]) continue;

          bool match;
          unsigned res = service_disk->check_name(*BaseProgram::myutcb(), i, ldisks[j], match);
          if (res != ENONE || !match) continue;

          DiskParameter params;
          res = service_disk->get_params(*BaseProgram::myutcb(), i, &params);
          if (res != ENONE || params.sectors * params.sectorsize < disksize) continue;

          //params.name[sizeof(params.name) - 1] = 0;
          //Logging::printf("params %s - %u : flags=%u sectors=%#llx sectorsize=%u maxrequest=%u name=%s\n", res == ENONE ? " success" : "failure", i,
          //params.flags, params.sectors, params.sectorsize, params.maxrequestcount, params.name);

          if (diskuuidmax < strlen(ldisks[j])) return ~0U;

          memcpy(diskuuid, ldisks[j], strlen(ldisks[j]));
          sectorsize = params.sectorsize;
          ldisks_used[j] = true; //XXX
          return i;
        }
      }
      return ~0U;
    }

    unsigned chg_event_slot(struct server_data * entry, uint32_t eventID, bool newentry) {
      for(unsigned i=0; i < sizeof(entry->events)/sizeof(entry->events[0]); i++) {
        if (newentry && entry->events[i].used) continue;
        if (!newentry && entry->events[i].id != eventID) continue;
        entry->events[i].id   = eventID;
        entry->events[i].used = newentry;
        return i;
      }
      return ~0U;
    }
    unsigned get_event_slot(struct server_data * entry, uint32_t eventID)
    {
      for(unsigned i=0; i < sizeof(entry->events)/sizeof(entry->events[0]); i++)
        if (entry->events[i].used && entry->events[i].id == eventID) return i;
      return ~0U;
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
        if (!entry || (~0U == get_event_slot(entry, eventid))) return false;
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
