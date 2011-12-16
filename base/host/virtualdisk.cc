/** @file
 * Memory backed virtual disk.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of Vancouver.
 *
 * Vancouver is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vancouver is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include "host/dma.h"
#include "nul/motherboard.h"
#include <nul/service_fs.h>
#include <nul/baseprogram.h>
#include <nul/capalloc.h>

/**
 * Provide a memory backed disk.
 *
 * State: broken
 */
class VirtualDisk : public StaticReceiver<VirtualDisk>
{

  DBus<MessageDiskCommit> &_bus_commit;
  unsigned      _disknr;
  char *        _data;
  unsigned long _length;
  const char *  _cmdline;

public:
  bool  receive(MessageDisk &msg)
  {
    if (msg.disknr != _disknr)  return false;
    MessageDisk::Status status = MessageDisk::DISK_OK;
    unsigned long long offset = msg.sector << 9;
    switch (msg.type)
      {
      case MessageDisk::DISK_READ:
      case MessageDisk::DISK_WRITE:
	for (unsigned i=0; i < msg.dmacount; i++) {
	  char *start = _data + offset;
	  char *end   = start + msg.dma[i].bytecount;
	  if (end > _data + _length || start > _data + _length || msg.dma[i].byteoffset > msg.physsize || msg.dma[i].byteoffset + msg.dma[i].bytecount > msg.physsize)
	    {
	      status = MessageDisk::Status(MessageDisk::DISK_STATUS_DEVICE | (i << MessageDisk::DISK_STATUS_SHIFT));
	      break;
	    }
	  if (msg.type == MessageDisk::DISK_READ)
	    memcpy(reinterpret_cast<void *>(msg.dma[i].byteoffset + msg.physoffset), start, end - start);
	  else
	    memcpy(start, reinterpret_cast<void *>(msg.dma[i].byteoffset + msg.physoffset), end - start);
	  offset += end - start;
	}
	break;
      case MessageDisk::DISK_GET_PARAMS:
	{
	  msg.params->flags = DiskParameter::FLAG_HARDDISK;
	  msg.params->sectors = _length >> 9;
	  msg.params->sectorsize = 512;
	  msg.params->maxrequestcount = msg.params->sectors;
	  unsigned slen = strlen(_cmdline) + 1;
	  memcpy(msg.params->name, _cmdline, slen < sizeof(msg.params->name) ? slen : sizeof(msg.params->name) - 1);
	  msg.params->name[slen] = 0;
	  return true;
	}
      case MessageDisk::DISK_FLUSH_CACHE:
	break;
      default:
	assert(0);
      }
    MessageDiskCommit msg2(msg.disknr, msg.usertag, status);
    _bus_commit.send(msg2);
    return true;
  }


  VirtualDisk(DBus<MessageDiskCommit> &bus_commit, unsigned disknr, char *data, unsigned long length, const char *cmdline) :
    _bus_commit(bus_commit), _disknr(disknr), _data(data), _length(length), _cmdline(cmdline) {}
};

PARAM_HANDLER(vdisk,
	      "vdisk:file - create a virtual disk from the given file",
	      "Example: vdisk:rom://foo/bar creates a virtual disk from the module foo/bar")
{
  char url[128];
  memcpy(url, args, args_len);
  url[args_len] = 0;

  char service_name[32] = "fs/";
  size_t service_name_len = sizeof(service_name) - 4;
  const char *filename = FsProtocol::parse_file_name(url, service_name + 3, service_name_len);
  if (filename == NULL) {
    Logging::printf("vdisk: Could not parse '%s'.\n", url);
    return;
  }

  unsigned cap_base = alloc_cap_region(FsProtocol::CAP_SERVER_PT + mb.hip()->cpu_desc_count() + 1, 0);
  FsProtocol::dirent fileinfo;
  FsProtocol fs_obj(cap_base + 1, service_name);
  FsProtocol::File file_obj(fs_obj, cap_base);
  if ((ENONE != fs_obj.get(*BaseProgram::myutcb(), file_obj, filename)) ||
      (ENONE != file_obj.get_info(*BaseProgram::myutcb(), fileinfo))) {
    Logging::printf("vdisk: Failed to load file '%s'\n", url);
    return;
  }

  char *module = new(4096) char[fileinfo.size];

  unsigned res = file_obj.copy(*BaseProgram::myutcb(), module, fileinfo.size);
  fs_obj.close(*BaseProgram::myutcb(), FsProtocol::CAP_SERVER_PT + mb.hip()->cpu_desc_count());
  dealloc_cap_region(cap_base, FsProtocol::CAP_SERVER_PT + mb.hip()->cpu_desc_count());

  if (res) { Logging::printf("vdisk: Couldn't read file.\n"); delete module; return; }

  Logging::printf("vdisk: Opened '%s' 0x%llx bytes.\n"
		  "vdisk: Attached as vdisk %u.\n",
		  fileinfo.name, fileinfo.size, mb.bus_disk.count());
          

  VirtualDisk * dev = new VirtualDisk(mb.bus_diskcommit,
				      mb.bus_disk.count(),
				      module,
				      fileinfo.size,
				      "virtualdisk");
  mb.bus_disk.add(dev, VirtualDisk::receive_static<MessageDisk>);
}

PARAM_HANDLER(vdisk_empty,
	      "vdisk_empty:size - create a virtual disk of a given size",
	      "Example: vdisk_empty:1048576 creates a virtual disk of 1MiB size")
{
  size_t size = argv[0];

  char *buffer = new(4096) char[size];

  Logging::printf("vdisk_empty: Attached as vdisk %u.\n", mb.bus_disk.count());

  VirtualDisk * dev = new VirtualDisk(mb.bus_diskcommit,
				      mb.bus_disk.count(),
				      buffer,
				      size,
				      "virtualdisk");
  mb.bus_disk.add(dev, VirtualDisk::receive_static<MessageDisk>);
}
