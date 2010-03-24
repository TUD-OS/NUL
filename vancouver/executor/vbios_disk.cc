/**
 * Virtual Bios disk routines.
 *
 * Copyright (C) 2009-2010, Bernhard Kauer <bk@vmmon.org>
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
    MAGIC_DISK_TAG = ~0u,
    FREQ = 1000,
    DISK_TIMEOUT = 5000,
    DISK_COMPLETION_CODE = 0x79,
  };
  unsigned _timer;
  DiskParameter _disk_params;
  bool _diskop_inprogress;


  bool check_drive(MessageBios &msg)
  {
    if (msg.cpu->dl == 0x80) return true;
    error(msg, 0x01); // invalid parameter
    return false;
  }

  /**
   * Read/Write disk helper.
   */
  bool disk_op(MessageBios &msg, unsigned long long blocknr, unsigned long address, unsigned count, bool write)
  {
    DmaDescriptor dma;
    dma.bytecount  = 512*count;
    dma.byteoffset = address;

    //    Logging::printf("%s(%llx) %s count %x -> %lx\n", __func__, blocknr, write ? "write" : "read",  count, address);
    MessageDisk msg2(write ? MessageDisk::DISK_WRITE : MessageDisk::DISK_READ, msg.cpu->dl & 0x7f, MAGIC_DISK_TAG, blocknr, 1, &dma, 0, ~0ul);
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
    msg.cpu->cs.sel = 0;
    msg.cpu->cs.base = 0;
    msg.cpu->eip = 0x7c00;
    msg.cpu->efl = 2;

    // we push an iret frame on the stack
    msg.cpu->ss.sel  = 0;
    msg.cpu->ss.base = 0;
    msg.cpu->esp     = 0x7000;
    msg.cpu->edx = 0x80; // booting from first disk
    if (!disk_op(msg, 0, 0x7c00, 1, false) || msg.cpu->ah)
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

    // default clears CF
    msg.cpu->efl &= ~1;

    switch (msg.cpu->ah)
      {
      case 0x00: // reset disk
	goto reset_disk;
      case 0x02: // read
      case 0x03: // write
	if (check_drive(msg))
	  {
	    unsigned cylinders = msg.cpu->ch | (msg.cpu->cl << 2) & 0x300;
	    unsigned heads =  msg.cpu->dh;
	    unsigned sectors = msg.cpu->cl & 0x3f;
	    unsigned blocknr;
	    if (msg.cpu->dl & 0x80)
	      blocknr = (cylinders * 255 + heads) * 63 + sectors - 1;
	    else
	      blocknr = (cylinders * 2 + heads) * 18 + sectors - 1;
	    return disk_op(msg, blocknr, msg.cpu->es.base + msg.cpu->bx, msg.cpu->al, msg.cpu->ah & 1);
	  }
	break;
      case 0x08: // get drive params
	// we report a single drive with maximum parameters
	if (check_drive(msg))
	  {
	    msg.cpu->cx = 0xfeff;
	    msg.cpu->dx = 0xfe01;
	    msg.cpu->ah = 0;  // successful
	  }
	break;
      case 0x15: // get disk type
	if (check_drive(msg))
	  {
	    msg.cpu->ah = 0x03;  // we report a harddisk
	    unsigned sectors = (_disk_params.sectors >> 32) ? 0xffffffff : _disk_params.sectors;
	    msg.cpu->dx = sectors & 0xffff;
	    msg.cpu->cx = sectors >> 16;
	  }
	break;
      case 0x41:  // int13 extension supported?
	if (check_drive(msg))
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
	if (check_drive(msg))  msg.cpu->ah = 0x00; // successful
	break;
      case 0x42: // extended read
      case 0x43: // extended write
	if (check_drive(msg))
	  {
	    copy_in(msg.cpu->ds.base + msg.cpu->si, &da, sizeof(da));
	    return disk_op(msg, da.block, (da.segment << 4) + da.offset, da.count, msg.cpu->ah & 1);
	  }
	break;
      case 0x48: // get drive params extended
	if (check_drive(msg))
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
	    params.sectors = _disk_params.sectors;
	    params.pheads = 255;
	    params.psectors = 63;
	    unsigned long long sectors =  _disk_params.sectors;
	    Math::div64(sectors, params.psectors*params.pheads);
	    params.pcylinders = sectors;
	    params.size = 0x1a;
	    params.sectorsize = 512;
	    copy_out(msg.cpu->ds.base + msg.cpu->si, &params, params.size);
	    Logging::printf("VB: driveparam size %x sectors %llx\n", params.size, params.sectors);
	    msg.cpu->ah = 0; // function supported
	  }
	break;
      default:
	switch (msg.cpu->ax)
	  {
	  case 0x4b00:  // bootable CDROM Emulation termintate
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
	_diskop_inprogress = false;

	// send a disk IRQ
	MessageIrq msg2(MessageIrq::ASSERT_IRQ, 14);
	_mb.bus_irqlines.send(msg2);
	return true;
      }
    return false;
  }

  /**
   * Get disk timeout.
   */
  bool  receive(MessageTimeout &msg)
  {
    if (msg.nr == _timer) {
      // a timeout happened -> howto return error code?
      Logging::printf("BIOS disk timeout\n");
      write_bda(DISK_COMPLETION_CODE, 1, 1);
      _diskop_inprogress = false;

      // send a disk IRQ to wakeup the threads
      MessageIrq msg2(MessageIrq::ASSERT_IRQ, 14);
      _mb.bus_irqlines.send(msg2);
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
	msg.cpu->inst_len = 0;
	CpuMessage msg2(CpuMessage::TYPE_HLT, msg.cpu, MTD_RIP_LEN | MTD_IRQ);
	msg.vcpu->executor.send(msg2);
	return jmp_int(msg, 0x76);
      }
      msg.cpu->al = read_bda(DISK_COMPLETION_CODE);
      msg.mtr_out |= MTD_GPR_ACDB;
      return true;
    default:    return false;
    }
  }


  VirtualBiosDisk(Motherboard &mb) : BiosCommon(mb) {
    mb.bus_diskcommit.add(this,  &VirtualBiosDisk::receive_static<MessageDiskCommit>);
    mb.bus_timeout.add(this,     &VirtualBiosDisk::receive_static<MessageTimeout>);

    // get sectors of the disk
    MessageDisk msg2(0, &_disk_params);
    if (!_mb.bus_disk.send(msg2)) {
      _disk_params.flags = 0;
      _disk_params.sectors = 0;
    }

    // get timer
    MessageTimer msg0;
    if (!mb.bus_timer.send(msg0))
      Logging::panic("%s can't get a timer", __PRETTY_FUNCTION__);
    _timer = msg0.nr;
  }



};

PARAM(vbios_disk,
      mb.bus_bios.add(new VirtualBiosDisk(mb), &VirtualBiosDisk::receive_static<MessageBios>);
      ,
      "vbios_disk- provide disk related virtual BIOS functions.");

