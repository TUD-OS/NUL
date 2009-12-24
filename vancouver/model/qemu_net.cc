/**
 * Wrapping Qemu network device models.
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

#include "vmm/motherboard.h"

typedef void (IOPortWriteFunc)(void *opaque, unsigned address, unsigned data);
typedef unsigned (IOPortReadFunc)(void *opaque, unsigned address);


/**
 * A single IO region as registered by the Qemu device.
 */
struct IORegion : public StaticReceiver<IORegion>
{
  const char* debug_getname() { return "IORegion"; }
  int _start;
  int _length;
  int _size;
  void*_func;
  void *_opaque;
  IORegion(int start, int length, int size, void *func, void *opaque) : _start(start), _length(length), _size(size), _func(func), _opaque(opaque) {}
  bool  receive(MessageIOIn  &msg)
  {
    if (!in_range(msg.port, _start, _length)) return false;
    if (_size != (1<<msg.type)) return false;
    msg.value = reinterpret_cast<IOPortReadFunc *>(_func)(_opaque, msg.port);
    return true;
  }
  bool  receive(MessageIOOut &msg)  {
    if (!in_range(msg.port, _start, _length)) return false;
    if (_size != (1<<msg.type)) return false;
    reinterpret_cast<IOPortWriteFunc *>(_func)(_opaque, msg.port, msg.value);
    return true;
  }
};


extern "C" void network_receive_packet(void *opaque, const unsigned char *buf, int size);

/**
 * Wrapper object that moderates between Qemu and Vancouver.
 *
 * State: unstable
 * Features: ioin,ioout,irq
 * Missing: split word access in byte ones.
 */
struct QemuDevice : public StaticReceiver<QemuDevice>
{
  const char* debug_getname() { return "QemuDevice"; }
  Motherboard &_mb;
  unsigned _base;
  unsigned _len;
  unsigned _irq;
  void *_opaque;
  const unsigned char *_lastpacket;
  DBus<MessageIOIn>  bus_ioin;
  DBus<MessageIOOut> bus_ioout;

  bool  receive(MessageIOIn  &msg) {
    if (!in_range(msg.port, _base, _len)) return false;
    return bus_ioin.send(msg);
  };


  bool  receive(MessageIOOut &msg) {
    if (!in_range(msg.port, _base, _len)) return false;
    return bus_ioout.send(msg);
  }

  void irq(unsigned level) {
    MessageIrq msg(level ? MessageIrq::ASSERT_IRQ : MessageIrq::DEASSERT_IRQ, _irq);
    _mb.bus_irqlines.send(msg);
  }

  void register_client(void *opaque) { _opaque = opaque; }
  void send(const unsigned char *buf, int size)
  {
    MessageNetwork msg(buf, size, 0);
    _lastpacket = buf;
    _mb.bus_network.send(msg);
    _lastpacket = 0;
  }

  bool  receive(MessageNetwork &msg) {
    // avoid loop
    if (msg.buffer == _lastpacket) return false;
    network_receive_packet(_opaque, msg.buffer, msg.len);
    return true;
  }


  QemuDevice(Motherboard &mb, unsigned base, unsigned len, unsigned irqnr)
    : _mb(mb), _base(base), _len(len), _irq(irqnr), _opaque(0), _lastpacket(0)
  {
    mb.bus_ioin.add(this, QemuDevice::receive_static<MessageIOIn>);
    mb.bus_ioout.add(this, QemuDevice::receive_static<MessageIOOut>);
    mb.bus_network.add(this, QemuDevice::receive_static<MessageNetwork>);
  }
};


/**
 * Global variable, as the register_* functions depend on them.
 * Need to be locked, if we create devices in parallel.
 */
static QemuDevice *qemudev;

/* REGISTER */
extern "C"
int register_ioport_read(int start, int len, int size, IOPortReadFunc *func, void *opaque)
{
  qemudev->bus_ioin.add(new IORegion(start, len, size, reinterpret_cast<void *>(func), opaque),
			IORegion::receive_static<MessageIOIn>);
  return 0;
}


extern "C"
int register_ioport_write(int start, int len, int size, IOPortWriteFunc *func, void *opaque)
{
  qemudev->bus_ioout.add(new IORegion(start, len, size, reinterpret_cast<void *>(func), opaque),
			 IORegion::receive_static<MessageIOOut>);
  return 0;
}



/* IRQ */
extern "C"
void qemu_set_irq(void *dev, int level) { static_cast<QemuDevice *>(dev)->irq(level); }


/* NETWORK */
extern "C"
void network_register_client(void **clientptr, void *opaque) { assert(qemudev); *clientptr = qemudev;  qemudev->register_client(opaque); }
extern "C"
void network_send_packet(void **clientptr, const unsigned char *buf, int size) { assert(*clientptr); reinterpret_cast<QemuDevice *>(*clientptr)->send(buf, size); }




extern "C" void *qemu_mallocz(unsigned long size)
{
  void *res;
  res = malloc(size);
  memset(res, 0, size);
  return res;
}
extern "C" void qemu_free(void *) {};




extern "C" void new_ne2000(unsigned base, void *irq, unsigned long long mac);
PARAM(qemu_ne2000,
      {

	MessageHostOp msg(MessageHostOp::OP_GET_UID, ~0);
	if (!mb.bus_hostop.send(msg))
	  Logging::printf("Could not get an UID");
	qemudev = new QemuDevice(mb, argv[0], 0x20, argv[1]);
	new_ne2000(argv[0], qemudev, (static_cast<unsigned long long>(argv[2]) << 16) | msg.value);
      },
      "qemu_ne2000:base,irq,mac - instanciate an ne2000 network card model from Qemu.",
      "Example: 'qemu_ne2000:0x300,0x9,0x16deaf'.")
