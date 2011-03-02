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

#include <service/string.h> //memset
#include <service/math.h> // htonl, htons
#include <service/logging.h>
#include <service/helper.h> //assert

#include <nul/service_fs.h>

#include <nul/types.h>
typedef uint8  uint8_t;
typedef uint16 uint16_t;
typedef uint32 uint32_t;

#include "nova_types.h"
#define UUID_LEN 16

class ConfigProtocol;

class Remcon {
  private:

    struct {
      short active;
      short istemplate;
      uint32_t id;
      unsigned remoteid;
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

    void handle_packet(void);

  public:

    Remcon(char const * _cmdline, ConfigProtocol * _sconfig) : data_received(0), dowrite(false), cmdline(_cmdline), service_config(_sconfig) {
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
      
        server_data[pos].id = pos + 1;
        server_data[pos].istemplate = 1;
        memcpy(server_data[pos].uuid, filename, UUID_LEN); //XXX generate uuid && check len of filename
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
};
