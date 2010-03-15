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
 * Missing:
 */
class VirtualBiosDisk : public StaticReceiver<VirtualBiosDisk>, public BiosCommon
{
  enum
  {
    FREQ = 1000,
    MAGIC_DISK_TAG = ~0u,
    DISK_TIMEOUT = 5000,
    DISK_COMPLETION_CODE = 0x79,
  };
  unsigned _timer;
  DiskParameter _disk_params;



  bool check_drive(CpuState *cpu)
  {
    if (cpu->dl == 0x80) return true;
    error(cpu, 0x01); // invalid parameter
    return false;
  }

  /**
   * Read/Write disk helper.
   */
  bool disk_op(CpuState *cpu, unsigned long long blocknr, unsigned long address, unsigned count, bool write)
  {
    DmaDescriptor dma;
    dma.bytecount  = 512*count;
    dma.byteoffset = address;

    //    Logging::printf("%s(%llx) %s count %x -> %lx\n", __func__, blocknr, write ? "write" : "read",  count, address);
    MessageDisk msg2(write ? MessageDisk::DISK_WRITE : MessageDisk::DISK_READ, cpu->dl & 0x7f, MAGIC_DISK_TAG, blocknr, 1, &dma, 0, ~0ul);
    if (!_mb.bus_disk.send(msg2) || msg2.error)
      {
	Logging::printf("msg2.error %x\n", msg2.error);
	error(cpu, 0x01);
	return true;
      }
    else
      {
	// XXX use HLT instead

	// wait for completion needed for AHCI backend!
	//Cpu::atomic_or<volatile unsigned>(&vcpu->hazard, VirtualCpuState::HAZARD_BIOS);

	// prog timeout during wait
	MessageTimer msg3(_timer, _mb.clock()->abstime(DISK_TIMEOUT, FREQ));
	_mb.bus_timer.send(msg3);

	assert(0);
	//cpu->cs.base = _base;
	//cpu->eip = WAIT_DISK_VECTOR;
	return true;
      }
  }


  bool boot_from_disk(CpuState *cpu)
  {
    cpu->cs.sel = 0;
    cpu->cs.base = 0;
    cpu->eip = 0x7c00;
    cpu->efl = 2;

    // we push an iret frame on the stack
    cpu->ss.sel  = 0;
    cpu->ss.base = 0;
    cpu->esp     = 0x7000;
    copy_out(cpu->ss.base + cpu->esp + 0, &cpu->eip,    2);
    copy_out(cpu->ss.base + cpu->esp + 2, &cpu->cs.sel, 2);
    copy_out(cpu->ss.base + cpu->esp + 4, &cpu->efl,    2);

    cpu->edx = 0x80; // booting from first disk
    if (!disk_op(cpu, 0, 0x7c00, 1, false) || cpu->ah)
      Logging::panic("VB: could not read MBR from boot disk");
    return true;
  }


  /**
   * Disk INT.
   */
  bool handle_int13(CpuState *cpu)
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
    cpu->efl &= ~1;

    switch (cpu->ah)
      {
      case 0x00: // reset disk
	goto reset_disk;
      case 0x02: // read
      case 0x03: // write
	if (check_drive(cpu))
	  {
	    unsigned cylinders = cpu->ch | (cpu->cl << 2) & 0x300;
	    unsigned heads =  cpu->dh;
	    unsigned sectors = cpu->cl & 0x3f;
	    unsigned blocknr;
	    if (cpu->dl & 0x80)
	      blocknr = (cylinders * 255 + heads) * 63 + sectors - 1;
	    else
	      blocknr = (cylinders * 2 + heads) * 18 + sectors - 1;
	    return disk_op(cpu, blocknr, cpu->es.base + cpu->bx, cpu->al, cpu->ah & 1);
	  }
	break;
      case 0x08: // get drive params
	// we report a single drive with maximum parameters
	if (check_drive(cpu))
	  {
	    cpu->cx = 0xfeff;
	    cpu->dx = 0xfe01;
	    cpu->ah = 0;  // successful
	  }
	break;
      case 0x15: // get disk type
	if (check_drive(cpu))
	  {
	    cpu->ah = 0x03;  // we report a harddisk
	    unsigned sectors = (_disk_params.sectors >> 32) ? 0xffffffff : _disk_params.sectors;
	    cpu->dx = sectors & 0xffff;
	    cpu->cx = sectors >> 16;
	  }
	break;
      case 0x41:  // int13 extension supported?
	if (check_drive(cpu))
	  switch (cpu->bx)
	    {
	    case 0x55aa:
	      // we report that version1 is supported
	      cpu->ah = 0x01;
	      cpu->cx = 0x0001;
	      cpu->bx = 0xaa55;
	      break;
	    default:
	      VB_UNIMPLEMENTED;
	    }
	break;
      reset_disk:
      case 0x0d: // reset disk
	if (check_drive(cpu))  cpu->ah = 0x00; // successful
	break;
      case 0x42: // extended read
      case 0x43: // extended write
	if (check_drive(cpu))
	  {
	    copy_in(cpu->ds.base + cpu->si, &da, sizeof(da));
	    return disk_op(cpu, da.block, (da.segment << 4) + da.offset, da.count, cpu->ah & 1);
	  }
	break;
      case 0x48: // get drive params extended
	if (check_drive(cpu))
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
	    copy_out(cpu->ds.base + cpu->si, &params, params.size);
	    Logging::printf("VB: driveparam size %x sectors %llx\n", params.size, params.sectors);
	    cpu->ah = 0; // function supported
	  }
	break;
      default:
	switch (cpu->ax)
	  {
	  case 0x4b00:  // bootable CDROM Emulation termintate
	  case 0x4b01:  // bootable CDROM Emulation status
	    error(cpu, 0x4b);
	    break;
	  default:
	    VB_UNIMPLEMENTED;
	  }
      }
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
      return true;
    }
    return false;
  };

  bool  receive(MessageBios &msg) {
    switch(msg.irq) {
    case 0x13:  return handle_int13(msg.cpu);
    case 0x19:  return boot_from_disk(msg.cpu);
    case WAIT_DISK_VECTOR:
      msg.cpu->al = read_bda(DISK_COMPLETION_CODE);
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

