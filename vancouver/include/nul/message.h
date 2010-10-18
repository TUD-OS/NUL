/** -*- Mode: C++ -*-
 * Message Type defintions.
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

#pragma once

/****************************************************/
/* IOIO messages                                    */
/****************************************************/
/**
 * An in() from an ioport.
 */
struct MessageIOIn
{
  enum Type {
    TYPE_INB = 0,
    TYPE_INW = 1,
    TYPE_INL = 2
  } type;
  unsigned short port;
  unsigned count;
  union {
    unsigned value;
    void *ptr;
  };
  MessageIOIn(Type _type, unsigned short _port) : type(_type), port(_port), count(0), value(~0u) {}
  MessageIOIn(Type _type, unsigned short _port, unsigned _count, void *_ptr) : type(_type), port(_port), count(_count), ptr(_ptr) {}
};


/**
 * An out() to an ioport.
 */
struct MessageIOOut {
  enum Type {
    TYPE_OUTB = 0,
    TYPE_OUTW = 1,
    TYPE_OUTL = 2
  } type;
  unsigned short port;
  unsigned count;
  union {
    unsigned value;
    void *ptr;
  };
  MessageIOOut(Type _type, unsigned short _port, unsigned _value) : type(_type), port(_port), count(0), value(_value) {}
  MessageIOOut(Type _type, unsigned short _port, unsigned _count, void *_ptr) : type(_type), port(_port), count(_count), ptr(_ptr) {}
};


/****************************************************/
/* Memory messages                                  */
/****************************************************/

/**
 * A dword aligned memory access.
 */
struct MessageMem
{
  enum {
    MSI_ADDRESS = 0xfee00000,
    MSI_DM      = 1 << 2,
    MSI_RH      = 1 << 3
  };
  bool read;
  unsigned long phys;
  unsigned *ptr;
  MessageMem(bool _read, unsigned long _phys, unsigned *_ptr) : read(_read), phys(_phys), ptr(_ptr) {}
};

/**
 * Request a region that is directly mapped into our memory.  Used for
 * mapping it to the user and optimizing internal access.
 *
 * Note, that clients can also return an empty region by not setting
 * the ptr.
 */
struct MessageMemRegion
{
  unsigned long page;
  unsigned long start_page;
  unsigned      count;
  char *        ptr;
  MessageMemRegion(unsigned long _page) : page(_page), count(0), ptr(0) {}
};


/****************************************************/
/* PCI messages                                     */
/****************************************************/

/**
 * A PCI config space transaction.
 */
struct MessagePciConfig
{
  enum Type {
    TYPE_READ,
    TYPE_WRITE
  } type;
  unsigned bdf;
  unsigned dword;
  unsigned value;
  MessagePciConfig(unsigned _bdf, unsigned _dword) : type(TYPE_READ), bdf(_bdf), dword(_dword), value(0xffffffff) {}
  MessagePciConfig(unsigned _bdf, unsigned _dword, unsigned _value) : type(TYPE_WRITE), bdf(_bdf), dword(_dword), value(_value) {}
};


/****************************************************/
/* SATA messages                                    */
/****************************************************/

class FisReceiver;

// XXX Use SATA bus to comunicated FISes
/**
 * Set a drive on a port of an AHCI controller.
 */
struct MessageAhciSetDrive
{
  FisReceiver *drive;
  unsigned port;
  MessageAhciSetDrive(FisReceiver *_drive, unsigned _port) : drive(_drive), port(_port) {}
};


/****************************************************/
/* IRQ messages                                     */
/****************************************************/


/**
 * Raise an IRQ.
 */
struct MessageIrq
{
  enum Type
    {
      ASSERT_IRQ,
      ASSERT_NOTIFY,
      DEASSERT_IRQ
    } type;
  unsigned char line;

  MessageIrq(Type _type, unsigned char _line) :  type(_type), line(_line) {}
};


/**
 * Notify that a level-triggered IRQ can be reraised.
 */
struct MessageIrqNotify
{
  unsigned char baseirq;
  unsigned char mask;
  MessageIrqNotify(unsigned char _baseirq, unsigned char _mask) : baseirq(_baseirq), mask(_mask)  {}
};


/**
 * Message on the PIC bus.
 */
struct MessagePic
{
  unsigned char slave;
  unsigned char vector;
  MessagePic(unsigned char _slave) :  slave(_slave) { }
};

/**
 * IPI-Message on the APIC bus.
 */
struct MessageApic
{
  enum {
    /**
     * The HW uses a special cycle for broadcast EOIs. We model that
     * by performing a write transaction to the first IOAPIC EOI
     * registers that is snooped by all IOApics.
     */
    IOAPIC_EOI = 0xfec00040,
    ICR_DM     = 1 << 11,
    ICR_ASSERT = 1 << 14,
    ICR_LEVEL  = 1 << 15
  };
  unsigned icr; // only bits 0xcfff are used
  unsigned dst; // 32bit APIC ID
  void    *ptr; // to distinguish loops
  MessageApic(unsigned _icr, unsigned _dst, void *_ptr) : icr(_icr), dst(_dst), ptr(_ptr) {};
};

/****************************************************/
/* Legacy messages                                  */
/****************************************************/

/**
 * Various messages of the legacy chips such as PIT, PPI...
 */
struct MessageLegacy
{
  enum Type
    {
      GATE_A20,
      FAST_A20,
      RESET,
      INIT,
      NMI,
      INTR,
      DEASS_INTR,
      INTA,
    } type;
  unsigned value;
  MessageLegacy(Type _type, unsigned _value=0) : type(_type), value(_value) {}
};

/**
 * Pit messages.
 */
struct MessagePit
{
  enum Type
    {
      GET_OUT,
      SET_GATE
    } type;
  unsigned pit;
  bool value;
  MessagePit(Type _type, unsigned _pit, bool _value=false) : type(_type), pit(_pit), value(_value) {}
};


/****************************************************/
/* Keyboard and Serial messages                     */
/****************************************************/
/**
 * Message on the PS2 bus between a KeyboardController and a connected Keyboard/Mouse
 */
struct MessagePS2
{
  unsigned port;
  enum Type
    {
      NOTIFY,
      READ_KEY,
      SEND_COMMAND
    }  type;
  unsigned char value;
  MessagePS2(unsigned char _port, Type _type, unsigned char _value) : port(_port), type(_type), value(_value) {}
};


/**
 * A keycode or a mouse packet. See keyboard.h for the format.
 */
struct MessageInput
{
  unsigned device;
  unsigned data;
  MessageInput(unsigned _device=0, unsigned _data=0) : device(_device), data(_data) {}
};


/**
 * An ascii character from the serial port.
 */
struct MessageSerial
{
  unsigned serial;
  unsigned char ch;
  MessageSerial(unsigned _serial, unsigned char _ch) : serial(_serial), ch(_ch) {}
};


/****************************************************/
/* Console messages                                 */
/****************************************************/

#include "host/vesa.h"

struct VgaRegs
{
  unsigned short mode;
  unsigned short cursor_style;
  unsigned cursor_pos;
  unsigned long offset;
};


typedef Vbe::ModeInfoBlock ConsoleModeInfo;

/**
 * VGA Console.
 */
struct MessageConsole
{
  enum Type
    {
      // allocate a new client
      TYPE_ALLOC_CLIENT,
      // allocate a new view for a client
      TYPE_ALLOC_VIEW,
      // get information about a mode
      TYPE_GET_MODEINFO,
      // get a FONT
      TYPE_GET_FONT,
      // switch to another view
      TYPE_SWITCH_VIEW,
      // the user pressed a key
      TYPE_KEY,
      // the user requests a reset
      TYPE_RESET,
      // the user requests to start a new PD
      TYPE_START,
      // the user requests to kill a PD
      TYPE_KILL,
      // the user requests a debug feature
      TYPE_DEBUG
    } type;
  unsigned short id;
  unsigned short view;
  union
  {
    struct {
      const char *clientname;
    };
    struct {
      const char *name;
      char    * ptr;
      unsigned size;
      VgaRegs *regs;
    };
    struct {
      unsigned index;
      ConsoleModeInfo *info;
    };
    struct {
      unsigned input_device;
      unsigned input_data;
    };
  };
  MessageConsole(Type _type = TYPE_ALLOC_CLIENT, unsigned short _id=0) : type(_type), id(_id), ptr(0) {}
  MessageConsole(unsigned _index, ConsoleModeInfo *_info) : type(TYPE_GET_MODEINFO), index(_index), info(_info) {}
  MessageConsole(const char *_name, char * _ptr, unsigned _size, VgaRegs *_regs)
    : type(TYPE_ALLOC_VIEW), id(~0), name(_name), ptr(_ptr), size(_size), regs(_regs) {}
  MessageConsole(unsigned short _id, unsigned short _view, unsigned _input_device, unsigned _input_data)
    : type(TYPE_KEY), id(_id), view(_view), input_device(_input_device), input_data(_input_data) {}
};


/**
 * VESA support.
 */
struct MessageVesa
{
  enum Type
    {
      // return available modes
      TYPE_GET_MODEINFO,
      // switch mode
      TYPE_SWITCH_MODE
    } type;
  unsigned index;
  Vbe::ModeInfoBlock *info;
  MessageVesa(unsigned _index, Vbe::ModeInfoBlock *_info) : type(TYPE_GET_MODEINFO), index(_index), info(_info) {}
  MessageVesa(unsigned _index) : type(TYPE_SWITCH_MODE), index(_index) {}
};

/****************************************************/
/* HOST messages                                    */
/****************************************************/

class VCpu;
/**
 * Request to the host, such as notify irq or request IO region.
 */
struct MessageHostOp
{
  enum Type
    {
      OP_ATTACH_IRQ,
      OP_NOTIFY_IRQ,
      OP_ATTACH_MSI,
      OP_ALLOC_IOIO_REGION,
      OP_ALLOC_IOMEM,
      OP_ALLOC_SEMAPHORE,
      OP_ALLOC_SERVICE_THREAD,
      OP_ASSIGN_PCI,
      OP_VIRT_TO_PHYS,
      OP_GET_MODULE,
      OP_GET_MAC,
      OP_GUEST_MEM,
      OP_ALLOC_FROM_GUEST,
      OP_VCPU_CREATE_BACKEND,
      OP_VCPU_BLOCK,
      OP_VCPU_RELEASE,
      OP_REGISTER_SERVICE,
    } type;
  unsigned long value;
  union {
    struct {
      unsigned long phys;
      unsigned long phys_len;
    };
    struct {
      char *ptr;
      unsigned long len;
    };
    struct {
      unsigned module;
      char * start;
      unsigned long size;
      char * cmdline;
      unsigned long cmdlen;
    };
    struct {
      unsigned msi_gsi;
      unsigned msi_value;
      unsigned long long msi_address;
    };
    struct {
      unsigned long long mac;
    };
    struct {
      VCpu *vcpu;
    };
  };
  MessageHostOp(VCpu *_vcpu) : type(OP_VCPU_CREATE_BACKEND), value(0), vcpu(_vcpu) {}
  MessageHostOp(unsigned _module, char * _start) : type(OP_GET_MODULE), module(_module), start(_start), size(0), cmdlen(0)  {}
  MessageHostOp(Type _type, unsigned long _value, unsigned long _len=0) : type(_type), value(_value), ptr(0), len(_len) {}
};


struct MessageAcpi
{
  enum  Type {
    ACPI_GET_TABLE,
    ACPI_GET_IRQ
  } type;
  union {
    struct {
      const char *name;
      unsigned instance;
      char *table;
      unsigned len;
    };
    struct {
      unsigned parent_bdf;
      unsigned bdf;
      unsigned char pin;
      unsigned gsi;
    };
  };
  MessageAcpi(const char *_name): type(ACPI_GET_TABLE), name(_name), instance(0), table(0), len(0) {}
  MessageAcpi(unsigned _parent_bdf, unsigned _bdf, unsigned char _pin): type(ACPI_GET_IRQ), parent_bdf(_parent_bdf), bdf(_bdf), pin(_pin), gsi(~0u) {}
};


/**
 * Resource discovery between device models is done by the virtual
 * BIOS.
 *
 * Resources can be ACPI tables, BIOS BDA, EBDA,...
 */
struct MessageDiscovery
{
  enum Type {
    DISCOVERY,
    WRITE,
    READ
  } type;
  struct {
    const char * resource;
    unsigned     offset;
    union {
      const void * data;
      unsigned   * dw;
    };
    unsigned     count;
  };
  MessageDiscovery() : type(DISCOVERY) {}
  MessageDiscovery(const char * _resource, unsigned _offset, const void * _data, unsigned _count)
    : type(WRITE), resource(_resource), offset(_offset), data(_data), count(_count) {}
  MessageDiscovery(const char * _resource, unsigned _offset, unsigned * _dw)
    : type(READ), resource(_resource), offset(_offset), dw(_dw) {}
};

/****************************************************/
/* DISK messages                                    */
/****************************************************/

class DmaDescriptor;
class DiskParameter;

/**
 * Request/read from the disk.
 */
struct MessageDisk
{
  enum Type
    {
      DISK_GET_PARAMS,
      DISK_READ,
      DISK_WRITE,
      DISK_FLUSH_CACHE
    } type;
  unsigned disknr;
  union
  {
    DiskParameter *params;
    struct {
      unsigned long long sector;
      unsigned long usertag;
      unsigned dmacount;
      DmaDescriptor *dma;
      unsigned long physoffset;
      unsigned long physsize;
    };
  };
  enum Status {
    DISK_OK = 0,
    DISK_STATUS_BUSY,
    DISK_STATUS_DEVICE,
    DISK_STATUS_DMA,
    DISK_STATUS_USERTAG,
    DISK_STATUS_SHIFT = 4,
    DISK_STATUS_MASK = (1 << DISK_STATUS_SHIFT) -1
  } error;
  MessageDisk(unsigned _disknr, DiskParameter *_params) : type(DISK_GET_PARAMS), disknr(_disknr), params(_params), error(DISK_OK) {}
  MessageDisk(Type _type, unsigned _disknr, unsigned long _usertag, unsigned long long _sector,
	      unsigned _dmacount, DmaDescriptor *_dma, unsigned long _physoffset, unsigned long _physsize)
    : type(_type), disknr(_disknr), sector(_sector), usertag(_usertag), dmacount(_dmacount), dma(_dma), physoffset(_physoffset), physsize(_physsize) {}
};


/**
 * A disk.request is completed.
 */
struct MessageDiskCommit
{
  unsigned disknr;
  unsigned long usertag;
  MessageDisk::Status status;
  MessageDiskCommit(unsigned _disknr=0, unsigned long _usertag=0, MessageDisk::Status _status=MessageDisk::DISK_OK) : disknr(_disknr), usertag(_usertag), status(_status) {}
};


/****************************************************/
/* Executor messages                                */
/****************************************************/

class CpuState;

#include "sys/utcb.h"
struct MessageBios
{
  VCpu *vcpu;
  CpuState *cpu;
  unsigned irq;
  unsigned mtr_out;
  MessageBios(VCpu *_vcpu, CpuState *_cpu, unsigned _irq) : vcpu(_vcpu), cpu(_cpu), irq(_irq), mtr_out() {}
};



/****************************************************/
/* Timer messages                                   */
/****************************************************/

typedef unsigned long long timevalue;


/**
 * Timer infrastructure.
 *
 * There is no frequency and clock here, as all is based on the same
 * clocksource.
 */
struct MessageTimer
{
  enum Type
    {
      TIMER_NEW,
      TIMER_REQUEST_TIMEOUT
    } type;
  unsigned  nr;
  timevalue abstime;
  MessageTimer()              : type(TIMER_NEW) {}
  MessageTimer(unsigned  _nr, timevalue _abstime) : type(TIMER_REQUEST_TIMEOUT), nr(_nr), abstime(_abstime) {}
};


/**
 * A timeout triggered.
 */
struct MessageTimeout
{
  unsigned  nr;
  timevalue time;
  MessageTimeout(unsigned  _nr, timevalue _time) : nr(_nr), time(_time) {}
};


/**
 * Returns the wall clock time in microseconds.
 *
 * It also contains a timestamp of the Motherboard clock in
 * microseconds, to be able to adjust to the time already passed and
 * to detect out-of-date values.
 */
struct MessageTime
{
  enum { FREQUENCY = 1000000 };
  timevalue wallclocktime;
  timevalue timestamp;
  MessageTime() :  wallclocktime(0), timestamp(0) {}
};


/****************************************************/
/* Network messages                                 */
/****************************************************/

struct MessageNetwork
{
  const unsigned char *buffer;
  unsigned len;
  unsigned client;
  MessageNetwork(const unsigned char *_buffer, unsigned _len, unsigned _client) : buffer(_buffer), len(_len), client(_client) {}
};

#include <nul/types.h>

struct MessageVirtualNet
{
  enum ops {
    ANNOUNCE,
  };

  unsigned op;
  unsigned vnet;	 	// Virtual Net
  unsigned client;

  uint32  *registers;

  // Filled in by forwarder
  mword    physsize;
  mword    physoffset;

  MessageVirtualNet(unsigned vnet, uint32 *registers) :
    op(ANNOUNCE), vnet(vnet), client(0), registers(registers)
  {}

};

struct MessageVirtualNetPing
{
  unsigned client;

  MessageVirtualNetPing(unsigned client)
    : client(client)
  { }
};

/* EOF */
