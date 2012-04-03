/** @file
 * Virtual Bios disk routines.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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
#include "executor/bios.h"
#include "host/dma.h"

/**
 * Virtual Bios disk routines.
 * Features: int13, boot from disk
 * Missing: multiple disks
 */
class VirtualBiosDisk : public StaticReceiver<VirtualBiosDisk>, public BiosCommon
{
  enum
  {
    MAX_DISKS  = 8,
    MAGIC_DISK_TAG = ~0u,
    FREQ = 1000,
    DISK_TIMEOUT = 5000,
    DISK_COMPLETION_CODE = 0x79,
    DISK_COUNT = 0x75,
    WAKEUP_IRQ = 1,
  };
  unsigned _timer;
  DiskParameter _disk_params[MAX_DISKS];
  unsigned _disk_count;
  bool _diskop_inprogress;

  void init_params() {
    // get sectors of the disk
    for (_disk_count = 0; _disk_count < MAX_DISKS; _disk_count++) {
      MessageDisk msg2(_disk_count, &_disk_params[_disk_count]);
      if (!_mb.bus_disk.send(msg2) || msg2.error) break;
    }
    write_bda(DISK_COUNT,_disk_count,1);
  }

  bool check_drive(MessageBios &msg, unsigned &disk_nr)
  {
    if (!~_disk_count) init_params();
    disk_nr = msg.cpu->dl & 0x7f;
    if (msg.cpu->dl & 0x80 && disk_nr < _disk_count) return true;
    error(msg, 0x01); // invalid parameter
    return false;
  }

  /**
   * Read/Write disk helper.
   */
  bool disk_op(MessageBios &msg, unsigned disk_nr, unsigned long long blocknr, unsigned long address, unsigned count, bool write)
  {
    DmaDescriptor dma;
    dma.bytecount  = 512*count;
    dma.byteoffset = address;

    MessageDisk msg2(write ? MessageDisk::DISK_WRITE : MessageDisk::DISK_READ, disk_nr, MAGIC_DISK_TAG, blocknr, 1, &dma, 0, ~0ul);
    if (!_mb.bus_disk.send(msg2) || msg2.error)
      {
	Logging::printf("msg2.error %x\n", msg2.error);
	error(msg, 0x01);
	return true;
      }
    else
      {
	_diskop_inprogress = true;

	// wait for completion needed for AHCI backend!
	// prog timeout during wait
	MessageTimer msg3(_timer, _mb.clock()->abstime(DISK_TIMEOUT, FREQ));
	_mb.bus_timer.send(msg3);

	return jmp_int(msg, 0x76);
      }
  }


  bool boot_from_disk(MessageBios &msg)
  {
    Logging::printf("boot from disk\n");
    msg.cpu->ss.sel  = 0;
    msg.cpu->ss.base = 0;
    msg.cpu->esp     = 0x7000;
    msg.cpu->edx     = 0x80; // booting from first disk

    msg.cpu->cs.sel  = 0;
    msg.cpu->cs.base = 0;
    msg.cpu->eip     = 0x7c00;
    msg.cpu->efl     = 0x202;

    // we push a real-mode iret frame onto the users stack
    unsigned short frame[] = {0x7c00, 0x0000, 0x0202};
    msg.cpu->esp -= sizeof(frame);
    copy_out(msg.cpu->esp, frame, sizeof(frame));

    if (!disk_op(msg, 0, 0, 0x7c00, 1, false) || msg.cpu->ah)
      Logging::panic("VB: could not read MBR from boot disk");
    msg.mtr_out |= MTD_CS_SS | MTD_RIP_LEN | MTD_RSP | MTD_RFLAGS | MTD_GPR_ACDB;
    return true;
  }


  /**
   * Disk INT.
   */
  bool handle_int13(MessageBios &msg)
  {
    COUNTER_INC("int13");
    struct disk_addr_packet {
      unsigned char size;
      unsigned char res;
      unsigned short count;
      unsigned short offset;
      unsigned short segment;
      unsigned long long block;
    } da;


    unsigned disk_nr;

    // default clears CF
    msg.cpu->efl &= ~1;

    switch (msg.cpu->ah)
      {
      case 0x00: // reset disk
	goto reset_disk;
      case 0x02: // read
      case 0x03: // write
	if (check_drive(msg,  disk_nr))
	  {
	    unsigned cylinders = msg.cpu->ch | (msg.cpu->cl << 2) & 0x300;
	    unsigned heads =  msg.cpu->dh;
	    unsigned sectors = msg.cpu->cl & 0x3f;
	    unsigned blocknr;
	    if (msg.cpu->dl & 0x80)
	      blocknr = (cylinders * 255 + heads) * 63 + sectors - 1;
	    else
	      blocknr = (cylinders * 2 + heads) * 18 + sectors - 1;
	    return disk_op(msg, disk_nr, blocknr, msg.cpu->es.base + msg.cpu->bx, msg.cpu->al, msg.cpu->ah & 1);
	  }
	break;
      case 0x08: // get drive params
	// we report maximum parameters
	if (check_drive(msg,  disk_nr))
	  {
	    msg.cpu->cx = 0xfeff;
	    msg.cpu->dx = 0xfe00 | _disk_count;
	    msg.cpu->ah = 0;  // successful
	  }
	break;
      case 0x15: // get disk type
	if (check_drive(msg, disk_nr))
	  {
	    msg.cpu->ah = 0x03;  // we report a harddisk
	    unsigned sectors = (_disk_params[disk_nr].sectors >> 32) ? 0xffffffff : _disk_params[disk_nr].sectors;
	    msg.cpu->dx = sectors & 0xffff;
	    msg.cpu->cx = sectors >> 16;
	  }
	break;
      case 0x41:  // int13 extension supported?
	if (check_drive(msg, disk_nr))
	  switch (msg.cpu->bx)
	    {
	    case 0x55aa:
	      // we report that version1 is supported
	      msg.cpu->ah = 0x01;
	      msg.cpu->cx = 0x0001;
	      msg.cpu->bx = 0xaa55;
	      break;
	    default:
	      DEBUG(msg.cpu);
	    }
	break;
      reset_disk:
      case 0x0d: // reset disk
	if (check_drive(msg, disk_nr))  msg.cpu->ah = 0x00; // successful
	break;
      case 0x42: // extended read
      case 0x43: // extended write
	if (check_drive(msg, disk_nr))
	  {
	    copy_in(msg.cpu->ds.base + msg.cpu->si, &da, sizeof(da));
	    return disk_op(msg, disk_nr, da.block, (da.segment << 4) + da.offset, da.count, msg.cpu->ah & 1);
	  }
	break;
      case 0x48: // get drive params extended
	if (check_drive(msg, disk_nr))
	  {
	    struct drive_parameters
	    {
	      unsigned short size;
	      unsigned short flags;
	      unsigned pcylinders;
	      unsigned pheads;
	      unsigned psectors;
	      unsigned long long sectors;
	      unsigned short sectorsize;
	    } params;
	    params.flags = 2;
	    params.sectors = _disk_params[disk_nr].sectors;
	    params.pheads = 255;
	    params.psectors = 63;
	    unsigned long long sectors =  _disk_params[disk_nr].sectors;
	    Math::div64(sectors, params.psectors*params.pheads);
	    params.pcylinders = sectors;
	    params.size = 0x1a;
	    params.sectorsize = 512;
	    copy_out(msg.cpu->ds.base + msg.cpu->si, &params, params.size);
	    msg.cpu->ah = 0; // function supported
	    Logging::printf("VB: driveparam[%d] size %x sectors %llx efl %x eax %x\n", disk_nr, params.size, params.sectors, msg.cpu->efl, msg.cpu->eax);
	    //msg.cpu->head.res1 = 0x100;
	  }
	break;
      default:
	switch (msg.cpu->ax)
	  {
	  case 0x4b00:  // bootable CDROM Emulation terminate
	  case 0x4b01:  // bootable CDROM Emulation status
	    error(msg, 0x4b);
	    break;
	  default:
	    DEBUG(msg.cpu);
	  }
      }
    msg.mtr_out |= MTD_GPR_ACDB | MTD_RFLAGS;
    return true;
  }

public:

  /**
   * Get disk commit.
   */
  bool  receive(MessageDiskCommit &msg)
  {
    if (msg.usertag == MAGIC_DISK_TAG) {
	write_bda(DISK_COMPLETION_CODE, msg.status, 1);
	if (_diskop_inprogress) {
	  _diskop_inprogress = false;
	  MessageIrqLines msg2(MessageIrq::ASSERT_IRQ, WAKEUP_IRQ);
	  _mb.bus_irqlines.send(msg2);
	  return true;
	}
    }
    return false;
  }

  /**
   * Get disk timeout.
   */
  bool  receive(MessageTimeout &msg)
  {
    if (msg.nr == _timer) {
      if (_diskop_inprogress) {

	// a timeout happened
	Logging::printf("BIOS disk timeout\n");
	write_bda(DISK_COMPLETION_CODE, 1, 1);
	_diskop_inprogress = false;

	// send a message to wakeup the client
	MessageIrqLines msg2(MessageIrq::ASSERT_IRQ, WAKEUP_IRQ);
	_mb.bus_irqlines.send(msg2);
      }
      return true;
    }
    return false;
  };

  bool  receive(MessageBios &msg) {
    switch(msg.irq) {
    case 0x13:  return handle_int13(msg);
    case 0x19:  return boot_from_disk(msg);
    case 0x76:
      if (_diskop_inprogress) {

	// make sure we are interruptible
	msg.cpu->efl |= 0x200;
	return jmp_int(msg, 0x76);
      }
      msg.cpu->ah = read_bda(DISK_COMPLETION_CODE);
      msg.mtr_out |= MTD_GPR_ACDB;
      return true;
    default:    return false;
    }
  }


  VirtualBiosDisk(Motherboard &mb) : BiosCommon(mb) {
    mb.bus_diskcommit.add(this,  VirtualBiosDisk::receive_static<MessageDiskCommit>);
    mb.bus_timeout.add(this,     VirtualBiosDisk::receive_static<MessageTimeout>);

    _disk_count = ~0u;

    // get timer
    MessageTimer msg0;
    if (!mb.bus_timer.send(msg0))
      Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);
    _timer = msg0.nr;
  }



};

PARAM_HANDLER(vbios_disk,
	      "vbios_disk- provide disk related virtual BIOS functions.")
{
  mb.bus_bios.add(new VirtualBiosDisk(mb), VirtualBiosDisk::receive_static<MessageBios>);
}

