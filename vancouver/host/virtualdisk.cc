/**
 * Memory backed virtual disk.
 *
 * Copyright (C) 2009, Bernhard Kauer <bk@vmmon.org>
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

#include "models/dma.h"
#include "vmm/motherboard.h"

class VirtualDisk : public StaticReceiver<VirtualDisk>
{

  DBus<MessageDiskCommit> &_bus_commit;
  unsigned      _disknr;
  char *        _data;
  unsigned long _length;
  char *        _cmdline;
  const char *debug_getname() {  return "VirtualDisk"; }
  void debug_dump()
  {
    Device::debug_dump();
    Logging::printf(" size %8lx name '%s'", _length, _cmdline);
  }

public:
  bool  receive(MessageDisk &msg)
  {    
    if (msg.disknr != _disknr)  return false;
    MessageDisk::Status status = MessageDisk::DISK_OK;
    unsigned long long offset = msg.sector << 9;
    switch (msg.type)
      {
      case MessageDisk::DISK_READ:
	for (unsigned i=0; i < msg.dmacount; i++) {
	  char *start = _data + offset;
	  char *end   = start + msg.dma[i].bytecount;
	  //Logging::printf("%s dma %lx+%x\n", __PRETTY_FUNCTION__, msg.dma[i].byteoffset, msg.dma[i].bytecount);
	  if (end > _data + _length || start > _data + _length || msg.dma[i].byteoffset > msg.physsize || msg.dma[i].byteoffset + msg.dma[i].bytecount > msg.physsize)
	    { 
	      status = MessageDisk::Status(MessageDisk::DISK_STATUS_DEVICE | (i << MessageDisk::DISK_STATUS_SHIFT)); 
	      break; 
	    }
	  //Logging::printf("%s memcpy %lx\n", __PRETTY_FUNCTION__, msg.physoffset);
	  memcpy(reinterpret_cast<void *>(msg.dma[i].byteoffset + msg.physoffset), start, end - start);
	  offset += end - start;
	}
	break;
      case MessageDisk::DISK_WRITE:
	status = MessageDisk::DISK_STATUS_DEVICE;
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


  VirtualDisk(DBus<MessageDiskCommit> &bus_commit, unsigned disknr, char *data, unsigned long length, char *cmdline) :
    _bus_commit(bus_commit), _disknr(disknr), _data(data), _length(length), _cmdline(cmdline) {}
};

PARAM(vdisk,
      /**
       * Create virtual disks from the modules.
       */
      for (unsigned modcount = 1; ; modcount++)
	{
	  MessageHostOp msg(modcount, 0);
	  if (!(mb.bus_hostop.send(msg)) || !msg.start)  break;
	  
	  Device * dev = new VirtualDisk(mb.bus_diskcommit,
					 mb.bus_disk.count(),
					 msg.start,
					 msg.size,
					 msg.cmdline);
	  mb.bus_disk.add(dev, &VirtualDisk::receive_static<MessageDisk>, mb.bus_disk.count());
	},
      "vdisk - create virtual disks from all modules")

