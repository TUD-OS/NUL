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

#include <service/string.h> //memset
#include <service/math.h> // htonl, htons
#include <service/logging.h>

#include <nul/service_config.h>
#include <nul/baseprogram.h>

#include "server.h"

struct Remcon::server_data * Remcon::check_uuid(char uuid[UUID_LEN]) {
  unsigned i;
  for(i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++)
    if (!memcmp(server_data[i].uuid, uuid, UUID_LEN)) return &server_data[i];
  return NULL;
}

void Remcon::recv_call_back(void * in_mem, size_t in_len, void * &out, size_t &out_len) {

  memcpy(buf_in, in_mem, data_received + in_len > sizeof(buf_in) ? sizeof(buf_in) - data_received : in_len);
  data_received += in_len;

  if (data_received < sizeof(buf_in)) return;

  data_received = 0;
  memset(buf_out, 0, sizeof(buf_out));

  handle_packet();

  if (dowrite) {
    out = buf_out;
    out_len = sizeof(buf_out);
    dowrite = false;
  }
}

#define GET_LOCAL_ID \
        uint32_t * _id = reinterpret_cast<uint32_t *>(&_in->opspecific); \
        uint32_t localid = Math::ntohl(*_id) - 1; \
        if (localid > sizeof(server_data)/sizeof(server_data[0]) || server_data[localid].id == 0) break

void Remcon::handle_packet(void) {

  //version check
  if (_in->version != Math::htons(0xafff)) {
    _out->version = Math::htons(0xafff);
    _out->result  = NOVA_UNSUPPORTED_VERSION;
    if (sizeof(buf_out) != send(buf_out, sizeof(buf_out)))
      Logging::printf("failure - sending reply\n");

    Logging::printf("failure - bad version\n");

    return;
  }

  //set default values 
  unsigned op = Math::ntohs(_in->opcode);
  _out->version = Math::htons(0xafff);
  _out->opcode  = Math::htons(op);
  _out->result  = NOVA_OP_FAILED;
  
  switch (op) {
    case NOVA_LIST_ACTIVE_DOMAINS:
      {
        unsigned i, k = 0;
        uint32_t * ids = reinterpret_cast<uint32_t *>(&_out->opspecific);

        for(i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
          if (server_data[i].id == 0 || !server_data[i].active) continue;

          k++;
          ids[k] = Math::htonl(server_data[i].id);
  
          if (reinterpret_cast<unsigned char *>(&ids[k + 1]) >= buf_out + NOVA_PACKET_LEN) break; //no space left
        }
        ids[0] = Math::htonl(k); //num of ids
        _out->result  = NOVA_OP_SUCCEEDED;
        break;
      }
    case NOVA_NUM_OF_ACTIVE_DOMAINS:
      {
        unsigned i, k = 0;
        for(i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
          if (server_data[i].id == 0) continue;
          k++;
        }

        uint32_t _num = Math::htonl(k);
        memcpy(&_out->opspecific, &_num, 4);
        _out->result  = NOVA_OP_SUCCEEDED;
        break;
      }
    case NOVA_NUM_OF_DEFINED_DOMAINS:
      {
        unsigned i, k = 0;
        for(i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
          if (server_data[i].id == 0 || server_data[i].active) continue;
          k++;
        }

        uint32_t num = Math::htonl(k);
        memcpy(&_out->opspecific, &num, 4);
        _out->result  = NOVA_OP_SUCCEEDED;
        break;
      }
    case NOVA_LIST_DEFINED_DOMAINS:
      {
        uint32_t * num = reinterpret_cast<uint32_t *>(&_out->opspecific);
        unsigned char * names = reinterpret_cast<unsigned char *>(&_out->opspecific) + 4;

        unsigned i,k = 0;
        for(i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
          if (server_data[i].id == 0 || server_data[i].active) continue;
          if (names + server_data[i].showname_len + 1 > buf_out + NOVA_PACKET_LEN) {
            Logging::printf("no space left\n");
            break; //no space left
          }

          memcpy(names, server_data[i].showname, server_data[i].showname_len);
          names[server_data[i].showname_len] = 0;
          names += server_data[i].showname_len + 1;
          k++; 
        }
        *num = Math::htonl(k);
        _out->result  = NOVA_OP_SUCCEEDED;

        break;
      }
    case NOVA_GET_NAME:
      {
        unsigned char * _name = reinterpret_cast<unsigned char *>(&_in->opspecific);

        unsigned i, len;
        for(i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
          if (server_data[i].id == 0 || server_data[i].active) continue;
          len = server_data[i].showname_len;
          if (memcmp(server_data[i].showname, _name, len)) continue;
          if (buf_in + NOVA_PACKET_LEN <=_name + len || _name[len] != 0) continue;
          if (&_out->opspecific + UUID_LEN + len + 1 > buf_out + NOVA_PACKET_LEN) break; //no space left
      
          memcpy(&_out->opspecific, server_data[i].uuid, UUID_LEN);
          memcpy(&_out->opspecific + UUID_LEN, server_data[i].showname, len);
          *(&_out->opspecific + UUID_LEN + len) = 0;
          _out->result  = NOVA_OP_SUCCEEDED;
          break;
        }
        break;
      }
    case NOVA_GET_NAME_UUID:
      {
        struct server_data * entry = check_uuid(reinterpret_cast<char *>(&_in->opspecific));
        if (!entry) break;

        unsigned len = entry->showname_len + 1;
        uint32_t id = Math::htonl(entry->id);
        memcpy(&_out->opspecific, &id, sizeof(id));
        memcpy(&_out->opspecific + sizeof(id), entry->showname, len);
        *(&_out->opspecific + sizeof(id) + len - 1) = 0;
        _out->result  = NOVA_OP_SUCCEEDED;
      }
    case NOVA_GET_NAME_ID:
      {
        GET_LOCAL_ID;
        if (!server_data[localid].active) break;

        unsigned len = server_data[localid].showname_len + 1;
        if (&_out->opspecific + UUID_LEN + len > buf_out + NOVA_PACKET_LEN) break; // no space left

        memcpy(&_out->opspecific, server_data[localid].uuid, UUID_LEN);
        memcpy(&_out->opspecific + UUID_LEN, server_data[localid].showname, len);
        *(&_out->opspecific + UUID_LEN + len - 1) = 0;
        _out->result  = NOVA_OP_SUCCEEDED;
        break;
      }
    case NOVA_VM_START:
      {
        char * _uuid = reinterpret_cast<char *>(&_in->opspecific);
        if (check_uuid(_uuid + UUID_LEN)) break; //if we have this uuid already deny starting

        unsigned i, j;
        for(i=0; i < sizeof(server_data)/sizeof(server_data[0]); i++) {
          if(memcmp(server_data[i].uuid, _uuid, UUID_LEN)) continue;
          
          if(server_data[i].istemplate) {
            for(j=0; j < sizeof(server_data)/sizeof(server_data[0]); j++) {
              if(server_data[j].id != 0) continue; //XXX racy

              unsigned crdout;
              unsigned char rrres;
              unsigned res = EABORT;
              char * module = 0;
              unsigned portal_num = FsProtocol::CAP_SERVER_PT + cpu_count;
              unsigned cap_base = alloc_cap(portal_num);
              cap_sel scs_usage = alloc_cap();
              if (!cap_base || !scs_usage) {
                if (cap_base) dealloc_cap(cap_base, portal_num);
                if (scs_usage) dealloc_cap(scs_usage);
                break;
              }

              server_data[j].id = j + 1; //XXX racy!
              server_data[j].active = true;
              memcpy(server_data[j].uuid, _uuid + UUID_LEN, UUID_LEN);
              server_data[j].filename     = server_data[i].filename;
              server_data[j].filename_len = server_data[i].filename_len;
              server_data[j].showname     = server_data[i].showname; //XXX which name to choose
              server_data[j].showname_len = server_data[i].showname_len; //XXX which name to choose
              memcpy(server_data[j].fsname, server_data[i].fsname, sizeof(server_data[i].fsname)); //XXX which name to choose

              FsProtocol::dirent fileinfo;
              FsProtocol fs_obj(cap_base, server_data[j].fsname);
              if (res = fs_obj.get_file_info(*BaseProgram::myutcb(), fileinfo,
                                             server_data[j].filename, server_data[j].filename_len))
              {
                Logging::printf("failure - err=0x%x, config %30s could not load file '%s'\n", res, server_data[j].showname, server_data[j].filename);
                goto cleanup;
              }

              module = new(4096) char[fileinfo.size];
              if (!module) goto cleanup;

              res = fs_obj.get_file_copy(*BaseProgram::myutcb(), module, fileinfo.size,
                                         server_data[j].filename, server_data[j].filename_len);
              if (res != ENONE) goto cleanup;

              unsigned short id;
              unsigned long mem;
              res = service_config->start_config(*BaseProgram::myutcb(), id, mem, scs_usage, module, fileinfo.size);
              if (res == ENONE) {
                rrres = nova_syscall(NOVA_LOOKUP, Crd(scs_usage,0,DESC_CAP_ALL).value(), 0, 0, 0, &crdout); //sanity check that we got a cap
                if (rrres != NOVA_ESUCCESS || crdout == 0) { res = EPERM; goto cleanup; }
                if (ENONE != service_admission->rebind_usage_cap(*BaseProgram::myutcb(), scs_usage)) { Logging::printf("failure - rebind of sc usage cap failed\n"); };

                server_data[j].scs_usage = scs_usage;
                server_data[j].maxmem    = mem;
                server_data[j].remoteid  = id;
                _out->result  = NOVA_OP_SUCCEEDED;
              } else if (res == ConfigProtocol::ECONFIGTOOBIG)
                Logging::printf("failure - configuration '%10s' is to big (size=%llu)\n", server_data[j].showname, fileinfo.size);

              cleanup:

              if (module) delete [] module;
              fs_obj.destroy(*BaseProgram::myutcb(), portal_num, this);

              if (res != ENONE) {
                dealloc_cap(scs_usage); //cap_base is deallocated in fs_obj.destroy
                server_data[j].active = false;
                server_data[j].id = 0;
              }

              break;
            }
          } else {
            server_data[i].active = true;
            _out->result  = NOVA_OP_SUCCEEDED;
          }
          break;
        }
        break;
      }
    case NOVA_VM_DESTROY:
      {
        GET_LOCAL_ID;
        unsigned res = service_config->kill(*BaseProgram::myutcb(), server_data[localid].remoteid);
        if (res == ENONE) {
          server_data[localid].id     = 0;
          server_data[localid].active = false;
          _out->result  = NOVA_OP_SUCCEEDED;
        }
        break;
      }
    case NOVA_GET_VM_INFO:
      {
        struct server_data * entry = check_uuid(reinterpret_cast<char *>(&_in->opspecific));
        if (!entry) break;
       
        uint64 consumed_time = 0;
        if (entry->active &&
            ENONE != service_admission->get_statistics(*BaseProgram::myutcb(), entry->scs_usage, consumed_time))
          Logging::printf("failure - could not get consumed time of client\n");

        *reinterpret_cast<uint32_t *>(&_out->opspecific) = Math::htonl(entry->maxmem / 1024); //in kB
        *reinterpret_cast<uint32_t *>(&_out->opspecific + sizeof(uint32_t)) = Math::htonl(1); //vcpus
        *reinterpret_cast<uint64_t *>(&_out->opspecific + 2*sizeof(uint32_t)) = (0ULL + Math::htonl(consumed_time)) << 32 + Math::htonl(consumed_time >> 32); //in microseconds
        _out->result  = NOVA_OP_SUCCEEDED;

        break;
      }
    case NOVA_ENABLE_EVENT:
    case NOVA_DISABLE_EVENT:
      {
        uint32_t * _id = reinterpret_cast<uint32_t *>(&_in->opspecific);
        uint32_t localid = Math::ntohl(*_id);

        if (localid == ~0U) {
          gevents = (op == NOVA_ENABLE_EVENT);
          _out->result  = NOVA_OP_SUCCEEDED;
          break;
        }

        localid -= 1;
        if (localid > sizeof(server_data)/sizeof(server_data[0]) || server_data[localid].id == 0) break;
        server_data[localid].events = (op == NOVA_ENABLE_EVENT);

        _out->result  = NOVA_OP_SUCCEEDED;
        break;
      }
    default:
      Logging::printf("got bad packet op=%u\n", op);
  }

  if (sizeof(buf_out) != send(buf_out, sizeof(buf_out))) {
    Logging::printf("failure - sending reply\n");
    return;
  }
}
