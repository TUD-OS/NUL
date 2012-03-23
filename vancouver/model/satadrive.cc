/** @file
 * SataDrive virtualisation.
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
#include "host/dma.h"
#include "model/sata.h"



/**
 * A SATA drive. It contains the register set of a SATA drive and
 * speaks the SATA transport layer protocol with its FISes.
 *
 * State: unstable
 * Features: read,write,identify
 * Missing: better error handling, many commands
 */
class SataDrive : public FisReceiver, public StaticReceiver<SataDrive>
{
#include "model/simplemem.h"
  DBus<MessageDisk>     &_bus_disk;
  unsigned _hostdisk;
  unsigned char _multiple;
  unsigned _regs[4];
  unsigned char _ctrl;
  unsigned char _status;
  unsigned char _error;
  unsigned _dsf[7];
  unsigned _splits[32];
  DiskParameter _params;
  static unsigned const DMA_DESCRIPTORS = 64;
  DmaDescriptor _dma[DMA_DESCRIPTORS];


  /**
   * A command is completed.
   * We send a register d2h FIS to the host.
   */
  void complete_command()
  {
    // remove DRQ
    _status = _status & ~0x8;

    unsigned d2h[5];
    d2h[0] = _error << 24 | _status << 16 | 0x4000 | _regs[0] & 0x0f00 | 0x34;
    d2h[1] = _regs[1];
    d2h[2] = _regs[2];
    d2h[3] = _regs[3] & 0xffff;
    // we overload the reserved value with the last transmitted command
    d2h[4] = _dsf[6];
    // make sure we never reuse this!
    _dsf[6] = 0;
    _peer->receive_fis(5, d2h);
  }


  void send_pio_setup_fis(unsigned short length, bool irq = false)
  {
    unsigned psf[5];

    // move from BSY to DRQ
    _status = _status & ~0x80 | 0x8;

    memset(psf, 0, sizeof(psf));
    psf[0] = _error << 24 | _status << 16 | (irq ? 0x4000 : 0) | 0x2000 | _regs[0] & 0x0f00 | 0x5f;
    psf[1] = _regs[1];
    psf[2] = _regs[2];
    psf[3] = _status << 24 | _regs[3] & 0xffff;
    psf[4] = length;
    _peer->receive_fis(5, psf);
  }


  void send_dma_setup_fis(bool direction)
  {
    unsigned dsf[7];
    memset(dsf, 0, sizeof(dsf));
    dsf[0] = 0x800 /* interrupt */ | (direction ? 0x200 : 0)| 0x41;
    _peer->receive_fis(7, dsf);
  };

  /**
   * We build the identify response in a buffer to allow to use push_data.
   */
  void build_identify_buffer(unsigned short *identify)
  {
    memset(identify, 0, 512);
    for (unsigned i=0; i<20; i++)
      identify[27+i] = _params.name[2*i] << 8 | _params.name[2*i+1];
    identify[47] = 0x80ff;
    identify[49] = 0x0300;
    identify[50] = 0x4001; // capabilites
    identify[53] = 0x0006; // bytes 64-70, 88 are valid
    identify[59] = 0x100 | _multiple; // the multiple count
    unsigned maxlba28 = (_params.sectors >> 32) ? ~0u :  _params.sectors;
    identify[60] = maxlba28 & 0xffff;
    identify[61] = maxlba28 >> 16;
    identify[64] = 3;      // pio 3+4
    identify[75] = 0x1f;   // NCQ depth 32
    identify[76] = 0x002;   // disabled NCQ + 1.5gbit
    identify[80] = 1 << 6; // major version number: ata-6
    identify[83] = 0x4000 | 1 << 10; // lba48
    identify[86] = 1 << 10; // lba48 enabled
    identify[88] = 0x203f;  // ultra DMA5 enabled
    memcpy(identify+100, &_params.sectors, 8);
    identify[0xff] = 0xa5;
    unsigned char checksum = 0;
    for (unsigned i=0; i<512; i++) checksum += reinterpret_cast<unsigned char *>(identify)[i];
    identify[0xff] -= checksum << 8;
  };

  /**
   * Push data to the user by doing DMA via the PRDs.
   *
   * Return the number of byte written.
   */
  unsigned push_data(unsigned length, void *data, bool &irq)
  {
    if (!_dsf[3]) return 0;
    unsigned long prdbase = union64(_dsf[2], _dsf[1]);
    Logging::printf("push data %x prdbase %lx _dsf %x %x %x\n", length, prdbase, _dsf[1], _dsf[2], _dsf[3]);
    unsigned prd = 0;
    unsigned offset = 0;
    while (offset < length && prd < _dsf[3])
      {
	unsigned prdvalue[4];
	copy_in(prdbase + prd*16, prdvalue, 16);

	irq = irq || prdvalue[3] & 0x80000000;
	unsigned sublen = (prdvalue[3] & 0x3fffff) + 1;
	if (sublen > length - offset) sublen = length - offset;
	copy_out(union64(prdvalue[1], prdvalue[0]), reinterpret_cast<char *>(data)+offset, sublen);
	offset += sublen;
	prd++;
      }
    // mark them as consumed!
    _dsf[3] -= prd;
    return offset;
  };

  /**
   * Read or write sectors from/to disk.
   */
  unsigned readwrite_sectors(bool read, bool lba48_ext)
  {
    unsigned long long sector;
    unsigned len;

    if (lba48_ext)
      {
	len = (_regs[3] & 0xffff) << 9;
	if (!len) len = 0x10000 << 9;
	sector = (static_cast<unsigned long long>(_regs[2] & 0xffffff) << 24) | _regs[1] & 0xffffff;
      }
    else
      {
	len = (_regs[3] & 0xff) << 9;
	if (!len) len = 0x100 << 9;
	sector = _regs[1] & 0x0fffffff;
      }

    if (!_dsf[3]) return 0;
    unsigned long prdbase = union64(_dsf[2], _dsf[1]);

    assert(_dsf[6] < 32);
    assert(_splits[_dsf[6]] == 0);

    unsigned prd = 0;
    unsigned lastoffset = 0;
    while (len)
      {

	unsigned transfer = 0;
	if (lastoffset) prd--;
	unsigned dmacount = 0;
	for (; prd < _dsf[3] && dmacount < DMA_DESCRIPTORS && len > transfer; prd++, dmacount++)
	  {

	    unsigned prdvalue[4];
	    copy_in(prdbase + prd*16, prdvalue, 16);

	    unsigned sublen = ((prdvalue[3] & 0x3fffff) + 1) - lastoffset;
	    if (sublen > len - transfer) sublen = len - transfer;

	    _dma[dmacount].byteoffset = union64(prdvalue[1], prdvalue[0]) + lastoffset;
	    _dma[dmacount].bytecount = sublen;
	    transfer += sublen;
	    lastoffset = 0;
	  }

	// remove all entries that completly contribute to the even entry and split larger ones
	while (transfer & 0x1ff)
	  {
	    assert(dmacount);
	    if (_dma[dmacount-1].bytecount > transfer & 0x1ff)
	      {
		lastoffset = _dma[dmacount-1].bytecount - transfer & 0x1ff;
		transfer &= ~0x1ff;
	      }
	    else
	      {
		transfer -= _dma[dmacount-1].bytecount;
		dmacount--;
	      }
	  }

	// are there bytes left to transfer, but we do not have enough PRDs?
	assert(dmacount);
	if (!dmacount && (len - transfer < 0x200)) return len - transfer;

	/**
	 * The new entries do not fit into DMA_DESCRIPTORS, do a single sector transfer
	 * This means we have to do a read in our own buffer and than copy them out
	 */
	if (!dmacount)
	  Logging::panic("single sector transfer unimplemented!");

	_splits[_dsf[6]]++;

	MessageDisk msg(read ? MessageDisk::DISK_READ : MessageDisk::DISK_WRITE, _hostdisk, _dsf[6], sector, dmacount, _dma, 0, ~0ul);
	check1(1, !_bus_disk.send(msg), "DISK operation failed");

	sector += transfer >> 9;
	assert(len >= transfer);
	len -= transfer;
	transfer = 0;
	// XXX check error code

      }
    return 0;
  };


  /**
   * Execute ATA commands.
   */
  void execute_command()
  {
    bool lba48_command = false;
    bool read = false;
    unsigned char atacmd = (_regs[0] >> 16) & 0xff;
    switch (atacmd)
      {
      case 0x24: // READ SECTOR EXT
      case 0x25: // READ DMA EXT
      case 0x29: // READ MULTIPLE EXT
	lba48_command = true;
      case 0x20: // READ SECTOR
      case 0xc4: // READ MULTIPLE
      case 0xc8: // READ DMA
	if (atacmd == 0x25 || atacmd == 0xc8)
	  send_dma_setup_fis(true);
	else
	  send_pio_setup_fis(512);
	readwrite_sectors(true, lba48_command);
	break;
      case 0x34: // WRITE SECTOR EXT
      case 0x35: // WRITE DMA EXT
      case 0x39: // WRITE MULTIPLE EXT
	lba48_command = true;
      case 0x30: // WRITE SECTOR
      case 0xc5: // WRITE MULITIPLE
      case 0xca: // WRITE DMA
	if (atacmd == 0x35 || atacmd == 0xca)
	  send_dma_setup_fis(false);
	else
	  send_pio_setup_fis(512);
	readwrite_sectors(false, lba48_command);
	break;
      case 0x60: // READ  FPDMA QUEUED
	read = true;
      case 0x61: // WRITE FPDMA QUEUED
	{
	  // some idiot has switched feature and sector count regs in this case!
	  unsigned feature = _regs[3] & 0xffff;
	  unsigned count = (_regs[0] >> 24) | (_regs[2] >> 16) & 0xff00;
	  _regs[3] = _regs[3] & 0xffff0000 | count;
	  _regs[0] = _regs[0] & 0x00ffffff | (feature << 24);
	  _regs[2] = _regs[2] & 0x00ffffff | (feature << 16) & 0xff000000;
	  send_dma_setup_fis(read);
	  readwrite_sectors(read, true);
	}
	break;
      case 0xc6: // SET MULTIPLE
	_multiple = _regs[3] & 0xff;
	complete_command();
	break;
      case 0xe0: // STANDBY IMMEDIATE
	Logging::printf("STANDBY IMMEDIATE\n");
	_error |= 4;
	_status |= 1;
	complete_command();
	break;
      case 0xec: // IDENTIFY
	{
	  Logging::printf("IDENTIFY\n");

	  // start pio command
	  send_pio_setup_fis(512);

	  unsigned short identify[256];
	  build_identify_buffer(identify);
	  bool irq = false;
	  push_data(512, identify, irq);
	  Logging::printf("IDENTIFY transfered\n");
	  complete_command();
	};
	break;
      case 0xef: // SET FEATURES
	Logging::printf("SET FEATURES %x sc %x\n", _regs[0] >> 24, _regs[3] & 0xff);
	complete_command();
	break;
      default:
	Logging::panic("should execute command %x\n", _regs[0] >> 16);
      };
  };


 public:
  void comreset()
  {
    // initialize state
    _regs[3] = 1;
    _regs[2] = 0;
    _regs[1] = _params.flags & DiskParameter::FLAG_ATAPI ? 0xeb1401 : 1;
    _status = 0x40; // DRDY
    _error = 1;
    _ctrl = _regs[3] >> 24;
    memset(_splits, 0, sizeof(_splits));
    complete_command();
  };


  /**
   * Receive a FIS from the controller.
   */
  void receive_fis(unsigned fislen, unsigned *fis)
  {
    if (fislen >= 2)
      switch (fis[0] & 0xff)
	{
	case 0x27:  // register FIS
	  assert(fislen ==5);

	  // remain in reset asserted state when receiving a normal command
	  if (_ctrl & 0x4 && _regs[0] & 0x8000)  { complete_command(); break; }

	  // copyin to our registers
	  memcpy(_regs, fis, sizeof(_regs));

	  if (_regs[0] & 0x8000)
	    execute_command();
	  else
	    {

	      // update ctrl register
	      _ctrl = _regs[3] >> 24;

	      // software reset?
	      if (_ctrl & 0x4)
		comreset();
	      else
		complete_command();
	    }
	  break;
	case 0x41: // dma setup fis
	  assert(fislen == 7);
	  memcpy(_dsf, fis, sizeof(_dsf));
	  break;
	default:
	  assert(!"Invalid H2D FIS!");
	  break;
	}
  };

  bool receive(MessageDiskCommit &msg)
  {
    if (msg.disknr != _hostdisk || msg.usertag > 32) return false;
    // we are done
    _status = _status & ~0x8;
    assert(_splits[msg.usertag]);
    assert(!msg.status);
    if (!--_splits[msg.usertag])
      {
	_dsf[6] = msg.usertag;
	complete_command();
      }
    return true;
  }


  SataDrive(DBus<MessageDisk> &bus_disk, DBus<MessageMemRegion> *bus_memregion, DBus<MessageMem> *bus_mem, unsigned hostdisk, DiskParameter params)
    : _bus_memregion(bus_memregion), _bus_mem(bus_mem), _bus_disk(bus_disk), _hostdisk(hostdisk), _multiple(0), _ctrl(0), _params(params)
  {
    Logging::printf("SATA disk %x flags %x sectors %llx\n", hostdisk, _params.flags, _params.sectors);
  }
};

PARAM_HANDLER(drive,
	      "drive:sigma0drive,controller,port - put a drive to the given port of an ahci controller by using a drive from sigma0 as backend.",
	      "Example: 'drive:0,1,2' to put the first sigma0 drive on the third port of the second controller.")
{
  DiskParameter params;
  unsigned hostdisk = argv[0];
  MessageDisk msg0(hostdisk, &params);
  check0(!mb.bus_disk.send(msg0) || msg0.error != MessageDisk::DISK_OK, "%s could not get disk %x parameters error %x", __PRETTY_FUNCTION__, hostdisk, msg0.error);

  SataDrive *drive = new SataDrive(mb.bus_disk, &mb.bus_memregion, &mb.bus_mem, hostdisk, params);
  mb.bus_diskcommit.add(drive, SataDrive::receive_static<MessageDiskCommit>);

  // XXX put on SATA bus
  MessageAhciSetDrive msg(drive, argv[2]);
  if (!mb.bus_ahcicontroller.send(msg, argv[1]))
    Logging::panic("AHCI controller #%ld does not allow to set drive #%lx\n", argv[1], argv[2]);
}

