/*
 * Copyright (C) 2012, Alexander Boettcher <boettcher@tudos.org>
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

#include <nul/baseprogram.h>

#include <service/endian.h>

#include "server.h"
#include "sha.h"

void Remcon::recv_file(uint32 remoteip, uint16 remoteport, uint16 localport, void * in, size_t in_len) {
  static struct connection {
    uint32 ip;
    uint32 port;
    char uuid[UUID_LEN];
    unsigned diskid;
    uint64_t sector;
    unsigned buffer_size;
    unsigned buffer_pos;
    Sha1::Context sha;
    char * buffer;
  } connections[4];
  unsigned i, free = 0;

  if (!in || in_len == 0) return;

  for (i=0; i < sizeof(connections) / sizeof(connections[0]); i++) {
    if (!free && !connections[i].buffer) free = i + 1;
    if (connections[i].ip   != remoteip)   continue;
    if (connections[i].port != remoteport) continue;
    break;
  }

  if (i >= (sizeof(connections) / sizeof(connections[0]))) {
    struct client {
      char uuid[UUID_LEN];
      uint32_t diskid;
      uint64_t disk_size;
    } PACKED * client = reinterpret_cast<struct client *>(in);
    if (in_len < sizeof(*client)) return;

    struct server_data * entry = check_uuid(client->uuid);
    if (!entry) return;
    client->diskid = Math::ntohl(client->diskid);
    if (client->diskid >= sizeof(entry->disks) / sizeof(entry->disks[0])) return;
    if (entry->disks[client->diskid].internal.diskid == ~0U ||
        !entry->disks[client->diskid].internal.sectorsize) return;
    if (!entry->disks[client->diskid].size ||
         entry->disks[client->diskid].size != Endian::hton64(client->disk_size)) return; //actual ntoh64

    entry->disks[client->diskid].read = 0;

    //Logging::printf("connection %u, disk id %u, internal disk id %u, sector size %u\n", free - 1, client->diskid,
    //  entry->disks[client->diskid].internal.diskid, entry->disks[client->diskid].internal.sectorsize);
    Logging::printf(".......   receiving image of size %llu from %u.%u.%u.%u:%u -> %u\n        [",
                    entry->disks[client->diskid].size, remoteip & 0xff, (remoteip >> 8) & 0xff, (remoteip >> 16) & 0xff,
                    (remoteip >> 24) & 0xff, remoteport, localport);

    connections[free - 1].ip   = remoteip;
    connections[free - 1].port = remoteport;
    memcpy(connections[free - 1].uuid, client->uuid, UUID_LEN);
    connections[free - 1].diskid = client->diskid;
    connections[free - 1].sector = 0;
    connections[free - 1].buffer_pos  = 0;
    connections[free - 1].buffer_size = 4096;
    connections[free - 1].buffer = new (0x1000) char[connections[free - 1].buffer_size];
    Sha1::init(&connections[free - 1].sha);

    if (in_len > sizeof(*client)) {
      in = reinterpret_cast<char *>(in) + sizeof(*client);
      in_len -= sizeof(*client);
      i = free - 1;
    } else return;
  }

  struct server_data * entry = check_uuid(connections[i].uuid);
  if (!entry) return;
  if (entry->disks[connections[i].diskid].read >= entry->disks[connections[i].diskid].size) return;

  uint64_t part = entry->disks[connections[i].diskid].size;
  uint64_t pos  = entry->disks[connections[i].diskid].read;
  Math::div64(part, 68);
  Math::div64(pos, part);
  if (((pos + 1) * part) <= (entry->disks[connections[i].diskid].read + in_len)) {
    Logging::printf("=");
/*
    Logging::printf("connection %u, disk id %u, internal disk id %u, sector %llu\n", i,
      connections[i].diskid,
      entry->disks[connections[i].diskid].internal.diskid,
      connections[i].sector);
*/
  }
  entry->disks[connections[i].diskid].read += in_len;

  size_t rest = in_len;
  while (rest) {
    size_t cplen = (connections[i].buffer_pos + rest) > connections[i].buffer_size ? connections[i].buffer_size - connections[i].buffer_pos : rest;
    rest -= cplen;

    memcpy(&connections[i].buffer[connections[i].buffer_pos], in, cplen);
    Sha1::hash(&connections[i].sha, reinterpret_cast<unsigned char *>(in), cplen);

    connections[i].buffer_pos  += cplen;
    in = reinterpret_cast<unsigned char *>(in) + cplen;

    if (connections[i].buffer_pos == connections[i].buffer_size ||
      entry->disks[connections[i].diskid].read == entry->disks[connections[i].diskid].size)
    {
      assert(sizeof(service_disk->disk_buffer) == connections[i].buffer_size);
      memcpy(service_disk->disk_buffer, connections[i].buffer, connections[i].buffer_size);
      unsigned ssize = (((connections[i].buffer_pos - 1) / entry->disks[connections[i].diskid].internal.sectorsize) + 1) * entry->disks[connections[i].diskid].internal.sectorsize;
      unsigned res = service_disk->write_synch(entry->disks[connections[i].diskid].internal.diskid, connections[i].sector, ssize);
      if (res != ENONE) {
        Logging::printf("disk operation failed: %#x\n", res); //XXX todos here
        free_entry(entry);
        return;
      }
/*
      //for debugging currently only
      memset(service_disk->disk_buffer, 0, connections[i].buffer_size);
      res = service_disk->read_synch(entry->disks[connections[i].diskid].internal.diskid, connections[i].sector, ssize);
      if (res != ENONE) Logging::printf("failure - could not read what was written: %#x\n", res);
      if (memcmp(service_disk->disk_buffer, connections[i].buffer, connections[i].buffer_size)) Logging::printf("failure - could not read back what was written\n");
      // debugging stuff end
*/
      connections[i].sector     += connections[i].buffer_size / entry->disks[connections[i].diskid].internal.sectorsize;
      connections[i].buffer_pos = 0;
    }
  }

  if (entry->disks[connections[i].diskid].read >= entry->disks[connections[i].diskid].size) {
    Sha1::finish(&connections[i].sha);
    Logging::printf("]\ndone    - image sha1: ");
    for (unsigned j=0; j < sizeof(connections[i].sha.hash); j++) {
      Logging::printf("%02x", connections[i].sha.hash[j]);
    }
    Logging::printf("\n");
    delete [] connections[i].buffer;
    memset(&connections[i], 0, sizeof(connections[i]));
    if (entry->disks[connections[i].diskid].read > entry->disks[connections[i].diskid].size)
      Logging::printf("error   - image was larger then specified\n");
    unsigned res = start_entry(entry);
    Logging::printf("%s - starting VM %u (err=%u)\n", res == ENONE ? "success" : "failure", entry->id, res);
    if (res != ENONE) free_entry(entry);
  }
}

