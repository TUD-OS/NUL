/** @file
 * Host IDE driver.
 *
 * Copyright (C) 2008-2009, Bernhard Kauer <bk@vmmon.org>
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

#include "nul/motherboard.h"
#include "host/hostgenericata.h"
#include "host/hostpci.h"

/**
 * A simple IDE host driver.
 *
 * State: testing
 * Features: lba28, lba48, PIO
 * Missing: dma, IRQ
 */
class HostIde : public StaticReceiver<HostIde>
{
  #include "host/simplehwioin.h"
  #include "host/simplehwioout.h"
  DBus<MessageDiskCommit> &_bus_commit;
  unsigned _disknr;
  unsigned short _iobase;
  unsigned short _iobase_ctrl;
  HostGenericAta _params[2];
  Clock * _clock;
  unsigned _disk_count;
  static unsigned const FREQ=1000; // millisecond


  /**
   * Wait with timeout for the right disk state.
   */
  unsigned  wait_disk_state(unsigned char mask, unsigned char value, unsigned msec, bool check_error)
  {
    unsigned char status;
    timevalue timeout = _clock->clock(FREQ) + msec;

    do {
	status = inb(_iobase + 7);
	if (check_error && status & 0x21) return (inb(_iobase + 1) << 8) | status;
	Cpu::pause();
    } while ((status & mask) != value && _clock->clock(FREQ) < timeout);
    return (status & mask) != value;
  }

  unsigned send_packet(unsigned char *packet)
  {
    unsigned res = wait_disk_state(0x80, 0, 10, false); // wait 10ms that BSY clears
    if (res)  return res;

    unsigned index = 0;
    for (unsigned i=0; i<8; i++)
      if (packet[0] & (1<<i))
	outb(packet[++index], _iobase + i);
    return 0;
  }

  /**
   * Send a packet to the ata controller.
   */
  unsigned  ata_command(unsigned char *packet, void *output, unsigned outlen, bool read)   __attribute__((noinline))
  {
    unsigned res = send_packet(packet);
    if (!res && outlen)
      {
	outlen /= 2;
	if (read)
	  {
	    if (wait_disk_state(0x88, 0x08, 10000, true)) return ~0x3ffU | inb(_iobase + 7); // wait 10seconds that we got the data
	    insw(output, outlen, _iobase);
	  }
	else
	  {
	    if (wait_disk_state(0x88, 0x08, 100, true)) return ~0x4ffU | inb(_iobase + 7); // wait 100ms that we can send the data
	    outsw(output, outlen, _iobase);
	  }
	if (wait_disk_state(0x88, 0, 20, true)) return ~0x5ffU | inb(_iobase + 7); // wait 20ms to let status settle and make sure there is no data left to transfer
      }
    return res;
  }



  /**
   * Initialize packets for a single sector LBA function.
   */
  bool make_sector_packets(HostGenericAta &params, unsigned char *packets, unsigned char command, unsigned long long sector, unsigned count)
  {
    if (sector >= params._maxsector || sector > (params._maxsector - count))  return true;
    if ((count >> 16) || !params._lba48 && count >> 8)  return true;
    packets[0] = 0x3c;
    packets[1] = count >> 8;
    packets[2] = sector >> 24;
    packets[3] = sector >> 32;
    packets[4] = sector >> 40;
    packets[8] = 0xfc;
    packets[9] = count;
    packets[10] = sector >> 0;
    packets[11] = sector >> 8;
    packets[12] = sector >> 16;
    packets[13] = (params._slave ? 0xf0 : 0xe0) | (sector >> 24) & (params._lba48 ? 0 : 0xf);
    packets[14] = command;
    return false;
  }


  unsigned  identify_drive(unsigned short *buffer, HostGenericAta &params, bool slave)
  {
    // select drive
    unsigned char packet_select[] = { 0x40, static_cast<unsigned char>(slave ? 0xb0 : 0xa0)};
    send_packet(packet_select);
    // select used and device ready
    if ((inb(_iobase + 6) & 0x10) != (slave ? 0x10 : 0) || ~inb(_iobase + 7) & 0x40)  return -1;

    unsigned char packet[] = { 0x80, 0xec };
    Logging::printf("ATA: identify status %x drive %x\n", inb(_iobase + 7), inb(_iobase + 6));
    unsigned res = ata_command(packet, buffer, 512, true);

    // abort?
    if (((res & 0x401) == 0x401) && ((res & 0x7f) != 0x7f))
      if (inb(_iobase + 4) == 0x14 && inb(_iobase + 5) == 0xeb)
	{
	  unsigned char atapi_packet[] = { 0xc0, static_cast<unsigned char>(slave ? 0xb0 : 0xa0), 0xa1 };
	  res = ata_command(atapi_packet, buffer, 512, true);
	  Logging::printf("ATA: identify packet err %x\n", res);
	}
    if (!res)  res = params.update_params(buffer, slave);
    return res;
  }


 public:
  unsigned disk_count() { return _disk_count; }

  bool  receive(MessageDisk &msg)
  {
    if (!in_range(msg.disknr, _disknr, _disk_count))  return false;
    MessageDisk::Status status = MessageDisk::DISK_OK;
    HostGenericAta &params = _params[msg.disknr - _disknr];
    unsigned char packets[18];

    switch (msg.type)
      {
      case MessageDisk::DISK_READ:
      case MessageDisk::DISK_WRITE:
	{

	  unsigned long length = DmaDescriptor::sum_length(msg.dmacount, msg.dma);

	  if (length & 0x1ff) { status = MessageDisk::DISK_STATUS_DMA; break; }
	  unsigned long offset = 0;
	  unsigned res = 0;
	  unsigned long long sector = msg.sector;
	  while (length > offset)
	    {
	      char buffer[512];

	      if (msg.type == MessageDisk::DISK_WRITE && DmaDescriptor::copy_inout(buffer, 512, offset, msg.dmacount, msg.dma, false, msg.physoffset, msg.physsize))
		{
		  status = MessageDisk::DISK_STATUS_DEVICE;
		  break;
		}

	      unsigned char command = (params._lba48 ? 0x24 : 0x20);
	      if (msg.type == MessageDisk::DISK_WRITE) command += 0x10;

	      // build sector packet
	      if (make_sector_packets(params, packets, command, sector, 1))
		{
		  Logging::printf("ATA: %s(%lx, %llx, %x) failed state %x\n", __func__, msg.usertag, sector, msg.dmacount, res);
		  status = MessageDisk::DISK_STATUS_DEVICE;
		  break;
		}
	      if (params._lba48)  send_packet(packets);
	      if ((res = ata_command(packets+8, buffer, 512, msg.type != MessageDisk::DISK_WRITE)))
		{
		  Logging::printf("ATA: %s(%lx, %llx, %x) failed with %x\n", __func__, msg.usertag, sector, msg.dmacount, res);
		  status = MessageDisk::DISK_STATUS_DEVICE;
		  break;
		}
	      if (msg.type == MessageDisk::DISK_READ && DmaDescriptor::copy_inout(buffer, 512, offset, msg.dmacount, msg.dma, true, msg.physoffset, msg.physsize))
		{
		  status = MessageDisk::DISK_STATUS_DEVICE;
		  break;
		}
	      offset += 512;
	      sector++;
	    }
	}
	break;
      case MessageDisk::DISK_FLUSH_CACHE:
	{
	  // XXX handle RO media
	  if (make_sector_packets(params, packets, params._lba48 ? 0xea: 0xe7, 0, 0) && ata_command(packets+8, 0, 0, true))
	    status = MessageDisk::DISK_STATUS_DEVICE;
	}
	break;
      case MessageDisk::DISK_GET_PARAMS:
	params.get_disk_parameter(msg.params);
	return true;
      default:
	Logging::panic("%s %x", __PRETTY_FUNCTION__, msg.type);
      }
    MessageDiskCommit msg2(msg.disknr, msg.usertag, status);
    _bus_commit.send(msg2);
    return true;
  };

  HostIde(DBus<MessageHwIOIn> &bus_hwioin, DBus<MessageHwIOOut> &bus_hwioout, DBus<MessageDiskCommit> &bus_commit,
	  unsigned disknr, unsigned short iobase, unsigned short iobase_ctrl, Clock *clock) :
     _bus_hwioin(bus_hwioin), _bus_hwioout(bus_hwioout), _bus_commit(bus_commit),
     _disknr(disknr), _iobase(iobase), _iobase_ctrl(iobase_ctrl), _clock(clock), _disk_count(0)
  {
    unsigned short buffer[256];
    if (!identify_drive(buffer, _params[_disk_count], false)) _disk_count++;
    if (!identify_drive(buffer,  _params[_disk_count], true)) _disk_count++;
  }

};


PARAM_HANDLER(hostide,
	      "hostide:mask - provide a hostdriver for all IDE controller.",
	      "Example: Use 'hostide:1' to have a driver for the first IDE controller.",
	      "The mask allows to ignore certain controllers. The default is to use all controllers.")
{
  HostPci pci(mb.bus_hwpcicfg, mb.bus_hostop);
  for (unsigned bdf, num = 0; bdf = pci.search_device(0x1, 0x1, num++);)
    {
      if (~argv[0] & (1UL << num) || (~pci.conf_read(bdf, 1) & 1))
	{
	  Logging::printf("Ignore IDE controller #%x at %x\n", num, bdf);
	  continue;
	}
      Logging::printf("DISK controller #%x IDE at %x\n", num, bdf);

      // primary and secondary controller
      for (unsigned i=0; i < 2; i++)
	{
	  unsigned bar1 = pci.conf_read(bdf, 4+i*2);
	  unsigned bar2 = pci.conf_read(bdf, 4+i*2 + 1);

	  // try legacy port
	  if (!bar1 && !bar2) { if (!i) { bar1=0x1f1; bar2=0x3f7; } else { bar1=0x171; bar2=0x377; } }
	  // we need both ports
	  if (!(bar1 & bar2 & 1)) continue;

	  // alloc io-ports
	  MessageHostOp msg1(MessageHostOp::OP_ALLOC_IOIO_REGION, ((bar1 & ~3) << 8) |  3);
	  MessageHostOp msg2(MessageHostOp::OP_ALLOC_IOIO_REGION, ((bar2 & ~3) << 8) |  0);
	  if (!mb.bus_hostop.send(msg1) || !mb.bus_hostop.send(msg2))
	    {
	      Logging::printf("%s could not allocate ioports %x, %x\n", __PRETTY_FUNCTION__, bar1, bar2);
	      continue;
	    }
	  // create controller
	  HostIde *dev = new HostIde(mb.bus_hwioin, mb.bus_hwioout, mb.bus_diskcommit,
				     mb.bus_disk.count(), bar1 & ~0x3, bar2 & ~0x3, mb.clock());
	  for (unsigned j=0; j < dev->disk_count(); j++)  mb.bus_disk.add(dev, HostIde::receive_static<MessageDisk>);
	}
    }
}
